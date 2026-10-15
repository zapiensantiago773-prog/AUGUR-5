"""Compare libraries rendered with the pack audit's phrases (tools/compare/README.md).

Usage:  python tools/compare/compare.py NAME=folder NAME=folder ...
        (folders of 48 kHz stereo WAVs; subfolders are searched)

Per file: loud_db (loudest 400 ms, the packs' level scale), peak_db, crest_db, low_db (< 150 Hz share),
          air_db (> 8 kHz share), width (side / mid RMS).
Per note, for the pluck phrase (8 notes, 0.369 s apart; median over the notes):
  attack_ms   onset -> peak of the 1 ms RMS envelope
  punch_db    peak of a 5 ms RMS window over the RMS 60-300 ms after the onset
  decay20_ms  time for the 5 ms RMS envelope to fall 20 dB below its peak
  sweep_oct   spectral centroid in the first 12 ms over the centroid at 120-180 ms (octaves): the filter sweep
  bright_hz   95 % spectral roll-off in the first 12 ms
Prints median [25th .. 75th percentile] per library.
"""
import sys
from pathlib import Path

import numpy as np
import soundfile as sf

SR = 48000
S16 = 60.0 / 122.0 / 4.0
ONSETS = [i * 3.0 * S16 for i in range(8)]
KEYS = ["loud_db", "peak_db", "crest_db", "punch_db", "attack_ms", "decay20_ms", "sweep_oct", "bright_hz", "low_db", "air_db",
        "width"]


def moving_mean(x, win):
    c = np.concatenate(([0.0], np.cumsum(x)))
    m = (c[win:] - c[:-win]) / win
    pad = win // 2
    return np.concatenate((np.full(pad, m[0]), m, np.full(len(x) - len(m) - pad, m[-1])))


def rms_env(x, win):
    return np.sqrt(np.maximum(moving_mean(x * x, win), 0.0) + 1e-20)


def centroid(seg):
    if len(seg) < 64:
        return np.nan
    spec = np.abs(np.fft.rfft(seg * np.hanning(len(seg)))) ** 2
    f = np.fft.rfftfreq(len(seg), 1.0 / SR)
    return float(np.sum(f * spec) / (np.sum(spec) + 1e-30))


def rolloff(seg, q=0.95):
    spec = np.abs(np.fft.rfft(seg * np.hanning(len(seg)))) ** 2
    f = np.fft.rfftfreq(len(seg), 1.0 / SR)
    c = np.cumsum(spec)
    return float(f[np.searchsorted(c, q * c[-1])])


def band_share(x, lo, hi):
    spec = np.abs(np.fft.rfft(x)) ** 2
    f = np.fft.rfftfreq(len(x), 1.0 / SR)
    return 10 * np.log10(spec[(f >= lo) & (f < hi)].sum() / (spec.sum() + 1e-30) + 1e-30)


def analyse(path):
    data, sr = sf.read(path, dtype="float64", always_2d=True)
    if sr != SR:
        raise SystemExit(f"{path}: {sr} Hz, expected {SR}")
    L, R = data[:, 0], data[:, 1] if data.shape[1] > 1 else data[:, 0]
    mid, side = 0.5 * (L + R), 0.5 * (L - R)
    end = min(len(mid), int((ONSETS[-1] + 0.369) * SR))
    m = mid[:end]

    out = {"peak_db": 20 * np.log10(np.max(np.abs(data)) + 1e-12)}
    win = int(0.4 * SR)
    c = np.concatenate(([0.0], np.cumsum(0.5 * (L * L + R * R))))
    out["loud_db"] = 10 * np.log10(((c[win:] - c[:-win]) / win).max() + 1e-24)
    out["crest_db"] = out["peak_db"] - out["loud_db"]
    out["low_db"] = band_share(m, 20, 150)
    out["air_db"] = band_share(m, 8000, 20000)
    out["width"] = float(np.sqrt(np.mean(side[:end] ** 2) / (np.mean(m ** 2) + 1e-30)))

    e1, e5 = rms_env(mid, int(0.001 * SR)), rms_env(mid, int(0.005 * SR))
    notes = {k: [] for k in ("attack_ms", "punch_db", "decay20_ms", "sweep_oct", "bright_hz")}
    for t in ONSETS:
        a, b = int(t * SR), int(t * SR) + int(0.369 * SR)
        seg1 = e1[a:b]
        if len(seg1) == 0 or seg1.max() < 1e-5:
            continue
        pk = int(np.argmax(seg1))
        on = int(np.argmax(seg1 > 0.1 * seg1[pk]))
        o = a + on
        notes["attack_ms"].append((pk - on) / SR * 1000)
        seg5 = e5[o:b]
        p5 = int(np.argmax(seg5))
        tail = mid[o + int(0.06 * SR):o + int(0.30 * SR)]
        notes["punch_db"].append(20 * np.log10(seg5[p5] / (np.sqrt(np.mean(tail ** 2)) + 1e-12)))
        below = np.nonzero(seg5[p5:] < seg5[p5] * 0.1)[0]
        notes["decay20_ms"].append((below[0] if len(below) else len(seg5) - p5) / SR * 1000)
        c0, c1 = centroid(mid[o:o + int(0.012 * SR)]), centroid(mid[o + int(0.12 * SR):o + int(0.18 * SR)])
        if c0 > 0 and c1 > 0:
            notes["sweep_oct"].append(np.log2(c0 / c1))
        notes["bright_hz"].append(rolloff(mid[o:o + int(0.012 * SR)]))
    for k, v in notes.items():
        out[k] = float(np.median(v)) if v else np.nan
    return out


def main():
    libs = [a.split("=", 1) for a in sys.argv[1:] if "=" in a]
    if not libs:
        raise SystemExit(__doc__)
    table = {}
    for name, folder in libs:
        files = sorted(Path(folder).rglob("*.wav"))
        table[name] = [analyse(p) for p in files]
        print(f"{name}: {len(files)} files")
    print(f"\n{'metric':<12}" + "".join(f"{n:>26}" for n in table))
    for k in KEYS:
        line = f"{k:<12}"
        for rows in table.values():
            v = np.array([r[k] for r in rows], dtype=float)
            v = v[np.isfinite(v)]
            line += f"{np.median(v):>10.2f} [{np.percentile(v, 25):>6.2f} .. {np.percentile(v, 75):>6.2f}]" if len(v) else f"{'-':>26}"
        print(line)


if __name__ == "__main__":
    main()
