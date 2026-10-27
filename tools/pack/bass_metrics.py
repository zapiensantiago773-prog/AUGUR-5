"""Bass measurements on rendered WAVs: what makes a bass hard, powerful and fat, as numbers.

The WAVs come from `augur_preset_audit --pack <folder> --wav <dir>` with role "bass": rolling sixteenths at 122 BPM
with the kick gap (-xxx -xxx ...), two bars, each note 60 % of a sixteenth (tools/preset_audit/PackAnalysis.h).

While the notes sound (energy shares of the mid signal):
  SUB    30-90 Hz       the weight a club system moves
  BODY   90-300 Hz      the fatness
  BITE   300-2000 Hz    the attack and the growl, what cuts through a mix
  AIR    above 2 kHz
  SMALL  above 150 Hz   what a laptop or phone speaker can play: a bass with none of it disappears there
LOWW   side / mid below 150 Hz: club systems sum the lows to mono, anything here is lost or cancels.
PUNCH  per note, the peak of the first 12 ms over the RMS of the next 60 ms, in dB (median of the notes).
EVEN   spread of the notes' body RMS in dB (max - min over the two bars): the groove must not wobble.
CREST  peak over the loudest 400 ms (as the packs are levelled), in dB: above ~15 dB the peak limit, not the
       loudness target, sets the level and the bass plays quieter than the rest of the pack.

Usage:  python tools/pack/bass_metrics.py <wav dir> [--csv out.csv]
"""
import csv
import pathlib
import sys

import numpy as np
import soundfile as sf

S16 = 60.0 / 122.0 / 4.0
ONSETS = [i * S16 for i in range(32) if i % 4 != 0]
NOTE_LEN = S16 * 0.6


def band_shares(mid, sr):
    n = 1 << 13
    win = np.hanning(n)
    power = np.zeros(n // 2 + 1)
    for s in range(0, len(mid) - n, n // 2):
        power += np.abs(np.fft.rfft(mid[s:s + n] * win)) ** 2
    f = np.fft.rfftfreq(n, 1.0 / sr)
    tot = power[(f >= 20) & (f <= 20000)].sum() + 1e-30

    def share(lo, hi):
        return float(power[(f >= lo) & (f < hi)].sum() / tot)

    return {"sub": share(30, 90), "body": share(90, 300), "bite": share(300, 2000), "air": share(2000, 20000),
            "small": share(150, 20000)}


def low_width(left, right, sr):
    n = 1 << 13
    win = np.hanning(n)
    f = np.fft.rfftfreq(n, 1.0 / sr)
    band = (f >= 20) & (f < 150)
    m = s = 0.0
    for k in range(0, len(left) - n, n // 2):
        mid = np.fft.rfft(0.5 * (left[k:k + n] + right[k:k + n]) * win)
        side = np.fft.rfft(0.5 * (left[k:k + n] - right[k:k + n]) * win)
        m += (np.abs(mid[band]) ** 2).sum()
        s += (np.abs(side[band]) ** 2).sum()
    return float(np.sqrt(s / max(m, 1e-30)))


def measure(path):
    x, sr = sf.read(str(path), always_2d=True)
    left, right = x[:, 0], x[:, 1]
    mid = 0.5 * (left + right)
    active = mid[: int((32 * S16) * sr)]
    out = band_shares(active, sr)
    out["loww"] = low_width(left[: len(active)], right[: len(active)], sr)

    punches, bodies = [], []
    for t in ONSETS:
        a = int(t * sr)
        head = np.abs(mid[a:a + int(0.012 * sr)])
        body = mid[a + int(0.012 * sr):a + int(0.072 * sr)]
        if len(head) == 0 or len(body) == 0:
            continue
        rms = np.sqrt(np.mean(body ** 2)) + 1e-12
        punches.append(20 * np.log10(head.max() / rms + 1e-12))
        bodies.append(20 * np.log10(rms))
    out["punch"] = float(np.median(punches))
    out["even"] = float(max(bodies) - min(bodies))

    win = int(0.4 * sr)
    power = 0.5 * (left ** 2 + right ** 2)
    c = np.concatenate([[0.0], np.cumsum(power)])
    loudest = max((c[s + win] - c[s]) / win for s in range(0, len(power) - win, win // 4))
    peak = float(np.max(np.abs(x)))
    out["crest"] = float(20 * np.log10(peak + 1e-12) - 10 * np.log10(loudest + 1e-24))
    return out


FIELDS = ("sub", "body", "bite", "air", "small", "loww", "punch", "even", "crest")


def main():
    root = pathlib.Path(sys.argv[1])
    rows = []
    for wav in sorted(root.rglob("*.wav")):
        m = measure(wav)
        m["name"] = str(wav.relative_to(root).with_suffix("")).replace("\\", "/")
        rows.append(m)
    if "--csv" in sys.argv:
        with open(sys.argv[sys.argv.index("--csv") + 1], "w", newline="", encoding="utf-8") as fh:
            w = csv.DictWriter(fh, fieldnames=("name",) + FIELDS, delimiter=";")
            w.writeheader()
            for m in rows:
                w.writerow({k: (m[k] if k == "name" else "%.4f" % m[k]) for k in ("name",) + FIELDS})
    print("%-34s %6s %6s %6s %6s %6s %6s %6s %6s %6s" % (("name",) + FIELDS))
    for m in rows:
        print("%-34s %6.3f %6.3f %6.3f %6.3f %6.3f %6.3f %6.1f %6.1f %6.1f" % ((m["name"][:34],) + tuple(m[k] for k in FIELDS)))
    if rows:
        print("%-34s" % "median" + "".join(" %6.3f" % float(np.median([m[k] for m in rows])) for k in FIELDS))


if __name__ == "__main__":
    main()
