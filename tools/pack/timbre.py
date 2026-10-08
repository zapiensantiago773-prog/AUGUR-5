"""Timbre distance between rendered presets (fingerprints from `augur_preset_audit --pack`), as in the series' atlas tools.

The fingerprint (tools/preset_audit/PackAnalysis.h) holds the spectral shape (32 log bands, dB re their mean), the
articulation (16 time slices, dB re the loudest), the brightness over time (centroid in octaves, 16 slices), spectral
flux and stereo width. The distance weighs each part so that 1.0 is roughly "clearly audible on a monitor"; two
presets closer than ~1.5 are the same sound to a listener.

Usage:  python tools/pack/timbre.py <audit.csv> [folder-filter]   -> nearest neighbours, most similar first
"""
import csv
import math
import sys


def parse(fp_text):
    return [float(v) for v in fp_text.split("|")] if fp_text else []


def _rms(a, b, mask=None):
    n, acc = 0, 0.0
    for i, (x, y) in enumerate(zip(a, b)):
        if mask is not None and not mask[i]:
            continue
        acc += (x - y) ** 2
        n += 1
    return math.sqrt(acc / n) if n else 0.0


def parts(a, b):
    """Per-part distances (spectrum dB, articulation dB, brightness octaves, flux, width)."""
    audible = [not (a[i] < -45 and b[i] < -45) for i in range(32)]  # bands both sounds leave empty do not count
    return (_rms(a[:32], b[:32], audible), _rms(a[32:48], b[32:48]), _rms(a[48:64], b[48:64]), abs(a[64] - b[64]), abs(a[65] - b[65]))


WEIGHTS = (1 / 2.5, 1 / 3.0, 1 / 0.12, 1 / 1.0, 1 / 0.12)


def distance(a, b):
    if not a or not b:
        return 99.0
    return math.sqrt(sum((p * w) ** 2 for p, w in zip(parts(a, b), WEIGHTS)))


def load(path):
    with open(path, encoding="utf-8") as fh:
        return {r["path"]: parse(r.get("fp", "")) for r in csv.DictReader(fh, delimiter=";")}


def main():
    rows = load(sys.argv[1])
    flt = sys.argv[2] if len(sys.argv) > 2 else ""
    names = [n for n in rows if flt in n]
    near = []
    for a in names:
        best = min(((distance(rows[a], rows[b]), b) for b in names if b != a), default=(99.0, ""))
        near.append((best[0], a, best[1]))
    near.sort()
    for d, a, b in near:
        print(f"{d:6.2f}  {a}  ~  {b}")


if __name__ == "__main__":
    main()
