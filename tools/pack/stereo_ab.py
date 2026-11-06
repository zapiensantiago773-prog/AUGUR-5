"""A/B of the stereo bass across every preset AUGUR-5 ships (factory, SUN ATLAS, Anthology Vol.1, AUSPICE).

  python tools/pack/stereo_ab.py run <out.csv>             every preset through `augur_preset_audit --stereo-report`
  python tools/pack/stereo_ab.py compare <before> <after>  what changed

The comparison follows the bass-phase task (D-057):
  - presets with real bass: the 20..120 Hz band within 20 dB of the whole output;
  - out of phase: L/R correlation of that band under 0.9, or a mono sum losing more than 1 dB;
  - above the bass: the per-channel third-octave levels from 160 Hz up, before against after;
  - level: the loudest 400 ms, so whatever moved can be levelled again.
"""
import concurrent.futures
import csv
import io
import os
import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
AUDIT = ROOT / "build/win-release/plugin/augur_preset_audit_artefacts/Release/augur_preset_audit.exe"
PACKS = ("SUN ATLAS", "AUGUR-5 Anthology Vol.1", "AUSPICE")
BANDS = [160, 200, 250, 315, 400, 500, 630, 800, 1000, 1250, 1600, 2000, 2500, 3150, 4000, 5000, 6300, 8000, 10000, 12500, 16000]


def jobs():
    out = [("factory", "factory")]
    for pack in PACKS:
        for sub in sorted(p for p in (ROOT / "packs" / pack).iterdir() if p.is_dir()):
            out.append((pack, str(sub.relative_to(ROOT))))
    return out


def run_one(job):
    pack, target = job
    text = subprocess.run([str(AUDIT), "--stereo-report", target], capture_output=True, text=True, cwd=str(ROOT)).stdout
    rows = []
    for r in csv.DictReader(io.StringIO(text), delimiter=";"):
        prefix = pack if pack == "factory" else pack + "/" + pathlib.Path(target).name
        r["path"] = prefix + "/" + r["path"]
        rows.append(r)
    return rows


def run(out_path):
    rows = []
    with concurrent.futures.ThreadPoolExecutor(max_workers=max(2, (os.cpu_count() or 4) - 1)) as ex:
        for part in ex.map(run_one, jobs()):
            rows += part
    with open(out_path, "w", newline="", encoding="utf-8") as fh:
        w = csv.DictWriter(fh, fieldnames=["path", "role", "loud", "bassdb", "corr", "mono", "bal", "L", "R"], delimiter=";")
        w.writeheader()
        w.writerows(rows)
    print("%d presets -> %s" % (len(rows), out_path))


def load(path):
    with open(path, encoding="utf-8") as fh:
        return {r["path"]: r for r in csv.DictReader(fh, delimiter=";")}


def group_of(path):
    return path.split("/")[0]


def compare(before_path, after_path):
    a, b = load(before_path), load(after_path)
    common = sorted(set(a) & set(b))
    groups = {}
    for p in common:
        groups.setdefault(group_of(p), []).append(p)
    print("Bass 20..120 Hz, presets with real bass (band within 20 dB of the output):")
    print("%-26s %6s %16s %16s %18s" % ("set", "bass", "out of phase", "mono loss >1 dB", "worst correlation"))
    for g, paths in groups.items():
        real = [p for p in paths if float(a[p]["bassdb"]) > -20.0]
        def bad(r):
            return float(r["corr"]) < 0.9 or float(r["mono"]) < -1.0
        def lossy(r):
            return float(r["mono"]) < -1.0
        wa = min((float(a[p]["corr"]) for p in real), default=1.0)
        wb = min((float(b[p]["corr"]) for p in real), default=1.0)
        print("%-26s %6d %7d -> %-6d %7d -> %-6d %+8.2f -> %+.2f" % (g, len(real), sum(bad(a[p]) for p in real), sum(bad(b[p]) for p in real),
                                                               sum(lossy(a[p]) for p in real), sum(lossy(b[p]) for p in real), wa, wb))

    # Above the bass: the largest per-channel change per band region, over every preset.
    regions = [("160-250 Hz", 160, 250), ("315-630 Hz", 315, 630), ("800 Hz-16 kHz", 800, 16000)]
    print("\nAbove the bass, per-channel third-octave change (dB), over %d presets (bands within 30 dB of the preset's"
          " strongest band; the quieter ones are its noise floor):" % len(common))
    for name, lo, hi in regions:
        idx = [i for i, f in enumerate(BANDS) if lo <= f <= hi]
        worst, where, med = 0.0, "", []
        for p in common:
            m = 0.0
            for ch in ("L", "R"):
                va = [float(x) for x in a[p][ch].split("|")]
                vb = [float(x) for x in b[p][ch].split("|")]
                floor = max(va) - 30.0
                for i in idx:
                    if va[i] > floor:
                        m = max(m, abs(vb[i] - va[i]))
            med.append(m)
            if m > worst:
                worst, where = m, p
        med.sort()
        print("  %-14s median %.2f, 95th %.2f, worst %.2f (%s)" % (name, med[len(med) // 2], med[int(len(med) * 0.95)], worst, where))

    moved = sorted(((float(b[p]["loud"]) - float(a[p]["loud"]), p) for p in common), key=lambda x: -abs(x[0]))
    print("\nLevel (loudest 400 ms): %d presets moved more than 0.3 dB; the largest:" % sum(1 for d, _ in moved if abs(d) > 0.3))
    for d, p in moved[:12]:
        print("  %+5.2f dB  %s" % (d, p))


if __name__ == "__main__":
    if len(sys.argv) >= 3 and sys.argv[1] == "run":
        run(sys.argv[2])
    elif len(sys.argv) >= 4 and sys.argv[1] == "compare":
        compare(sys.argv[2], sys.argv[3])
    else:
        print(__doc__)
