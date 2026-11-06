"""Put back the level of the pack presets a DSP change moved (D-057).

  python tools/pack/relevel_moved.py <before.csv> <after.csv> [--threshold 0.3] [--dry]

The CSVs come from `tools/pack/stereo_ab.py run` (each preset played as its role, loudest 400 ms, as the packs are
levelled). Every pack preset whose level moved more than the threshold gets its LEVEL (amp_level) moved back by
exactly that much, so it plays where it did before; the others are not touched. Factory presets are only listed:
their levels live in plugin/Presets.cpp (tools/level_presets.py). Afterwards: `augur_preset_audit --chord-headroom
<pack> --check` and a new `stereo_ab.py run` to confirm.
"""
import csv
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]


def load(path):
    with open(path, encoding="utf-8") as fh:
        return {r["path"]: r for r in csv.DictReader(fh, delimiter=";")}


def main():
    before, after = load(sys.argv[1]), load(sys.argv[2])
    threshold = float(sys.argv[sys.argv.index("--threshold") + 1]) if "--threshold" in sys.argv else 0.3
    dry = "--dry" in sys.argv
    moved = factory = 0
    for path in sorted(set(before) & set(after)):
        delta = float(after[path]["loud"]) - float(before[path]["loud"])
        if abs(delta) <= threshold:
            continue
        if path.startswith("factory/"):
            print("factory  %+5.2f dB  %s" % (delta, path))
            factory += 1
            continue
        pack, rest = path.split("/", 1)
        file = ROOT / "packs" / pack / rest
        text = file.read_bytes().decode("utf-8")  # bytes: the file keeps its own line endings
        m = re.search(r'(<P id="amp_level" value=")([-0-9.e]+)(")', text)
        current = float(m.group(2)) if m else -6.0
        new = max(-40.0, min(6.0, round(current - delta, 2)))
        if m:
            text = text[: m.start()] + m.group(1) + ("%g" % new) + m.group(3) + text[m.end():]
        else:
            text = text.replace("</PARAMS>", '  <P id="amp_level" value="%g"/>\n  </PARAMS>' % new)
        if not dry:
            file.write_bytes(text.encode("utf-8"))
        print("%-8s %+5.2f dB  LEVEL %+.2f -> %+.2f  %s" % ("pack", delta, current, new, path))
        moved += 1
    print("%d pack presets put back%s; %d factory presets to level in plugin/Presets.cpp" % (moved, " (dry run)" if dry else "", factory))


if __name__ == "__main__":
    main()
