"""Level-match the factory presets from an augur_preset_audit report.

Usage:  augur_preset_audit > audit.txt ;  python tools/level_presets.py audit.txt
Adjusts each preset's amp_level in plugin/Presets.cpp so its short-term loudness (loudest 50 ms RMS)
hits the category target, while keeping the peak at or below the ceiling. Run, rebuild, re-audit:
two passes converge (the chain after the level control is nearly linear).
"""
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
PRESET_FILES = [ROOT / "plugin" / "Presets.cpp", ROOT / "plugin" / "PresetsExpansion.inc"]

TARGET_DB = -18.0            # melodic sounds
TARGET_DRUM_BODY_DB = -16.0  # kicks, toms
TARGET_SNARE_DB = -20.0
TARGET_DRUM_TOP_DB = -25.0   # hats, clap, rim, cowbell: 30-60 ms hits sit lower in a mix
PEAK_CEILING_DB = -3.0
DRUM_TOPS = {"Closed Hat", "Open Hat", "Noise Clap", "Rim Click", "Metal Cowbell", "Ring Metal Perc", "Noise Shaker"}


def target_for(name: str, category: str) -> float:
    if category == "Drums":
        if name in DRUM_TOPS:
            return TARGET_DRUM_TOP_DB
        return TARGET_SNARE_DB if "Snare" in name else TARGET_DRUM_BODY_DB
    return TARGET_DB


def main(report_path: str) -> None:
    rows = []
    for line in pathlib.Path(report_path).read_text(encoding="utf-8", errors="ignore").splitlines()[1:]:
        m = re.match(r"(.+?)\s{2,}(\S.*?)\s+(-?\d+\.\d)\s+(-?\d+\.\d)", line)
        if m:
            rows.append((m.group(1).strip(), m.group(2).strip(), float(m.group(3)), float(m.group(4))))

    sources = {f: f.read_text(encoding="utf-8") for f in PRESET_FILES}
    worst = 0.0
    for name, category, peak, loud in rows:
        if name == "Init":
            continue
        key = '{ "' + name + '", {'
        f = next((f for f, src in sources.items() if key in src), None)
        if f is None:
            print("not found:", name)
            continue
        source = sources[f]
        delta = target_for(name, category) - loud
        delta = min(delta, PEAK_CEILING_DB - peak)  # never push the peak over the ceiling
        start = source.index(key)
        end = source.index('}, "', start)
        body = source[start:end]
        m = re.search(r'\{ "amp_level", (-?[0-9.]+)f \}', body)
        current = float(m.group(1)) if m else -6.0
        new = round(max(-40.0, min(6.0, current + delta)), 1)
        setting = '{ "amp_level", %.1ff }' % new
        body = body.replace(m.group(0), setting) if m else body[: body.rindex("}") + 1] + ", " + setting + body[body.rindex("}") + 1 :]
        sources[f] = source[:start] + body + source[end:]
        worst = max(worst, abs(delta))
    for f, src in sources.items():
        f.write_text(src, encoding="utf-8")
    print(f"{len(rows)} presets checked, largest correction {worst:.1f} dB")


if __name__ == "__main__":
    main(sys.argv[1])
