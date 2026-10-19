"""Level-match the factory presets from an augur_preset_audit report.

Melodic sounds (D-045), on the same scale as the SUN ATLAS / MOON ATLAS / HORIZON ATLAS libraries:
    augur_preset_audit --factory-loudness > loud.txt ;  python tools/level_presets.py loud.txt
  every sound is played as its role in a track and its loudest 400 ms are brought to -16 dB, peak <= -1 dBFS.
Drums (one-shots) keep their short-term targets (loudest 50 ms RMS):
    augur_preset_audit > audit.txt ;  python tools/level_presets.py audit.txt --drums
Then chord headroom: a polysynth sound must not clip when played as chords. The default audit plays every sound as
chords (four notes for pads, keys and stabs, a dyad for the rest, one note for leads and basses); this only lowers
the melodic sounds whose peak there passes -1 dBFS:
    augur_preset_audit > audit.txt ;  python tools/level_presets.py audit.txt --headroom
Adjusts each preset's amp_level in plugin/Presets.cpp / PresetsExpansion.inc. Run, rebuild, re-measure: two or three
passes converge (the chain after the level control is nearly linear).
"""
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
PRESET_FILES = [ROOT / "plugin" / "Presets.cpp", ROOT / "plugin" / "PresetsExpansion.inc"]

TARGET_DB = -16.0            # melodic sounds: loudest 400 ms of their role's phrase (the packs' scale)
PEAK_CEILING_DB = -1.0
TARGET_DRUM_BODY_DB = -16.0  # kicks, toms (loudest 50 ms)
TARGET_SNARE_DB = -20.0
TARGET_DRUM_TOP_DB = -25.0   # hats, clap, rim, cowbell: 30-60 ms hits sit lower in a mix
DRUM_PEAK_CEILING_DB = -3.0
DRUM_TOPS = {"Closed Hat", "Open Hat", "Noise Clap", "Rim Click", "Metal Cowbell", "Ring Metal Perc", "Noise Shaker"}


def target_for(name: str, category: str) -> float:
    if category == "Drums":
        if name in DRUM_TOPS:
            return TARGET_DRUM_TOP_DB
        return TARGET_SNARE_DB if "Snare" in name else TARGET_DRUM_BODY_DB
    return TARGET_DB


def main(report_path: str, drums: bool, headroom: bool) -> None:
    rows = []
    for line in pathlib.Path(report_path).read_text(encoding="utf-8", errors="ignore").splitlines()[1:]:
        m = re.match(r"(.+?)\s{2,}(\S.*?)\s+(-?\d+\.\d)\s+(-?\d+\.\d)", line)
        if m:
            rows.append((m.group(1).strip(), m.group(2).strip(), float(m.group(3)), float(m.group(4))))

    sources = {f: f.read_text(encoding="utf-8") for f in PRESET_FILES}
    worst = 0.0
    for name, category, peak, loud in rows:
        if name == "Init" or (category == "Drums") != drums:
            continue
        if headroom and peak <= PEAK_CEILING_DB:
            continue
        key = '{ "' + name + '", {'
        f = next((f for f, src in sources.items() if key in src), None)
        if f is None:
            print("not found:", name)
            continue
        source = sources[f]
        if headroom:
            delta = PEAK_CEILING_DB - peak - 0.05  # only down, just under the ceiling
        else:
            delta = target_for(name, category) - loud
            delta = min(delta, (DRUM_PEAK_CEILING_DB if drums else PEAK_CEILING_DB) - peak)  # never push the peak over the ceiling
        start = source.index(key)
        end = source.index('}, "', start)
        body = source[start:end]
        m = re.search(r'\{ "amp_level", (-?[0-9.]+)f \}', body)
        current = float(m.group(1)) if m else -6.0
        new = max(-40.0, min(6.0, (current + delta) // 0.1 * 0.1 if headroom else round(current + delta, 1)))
        setting = '{ "amp_level", %.1ff }' % new
        body = body.replace(m.group(0), setting) if m else body[: body.rindex("}") + 1] + ", " + setting + body[body.rindex("}") + 1 :]
        sources[f] = source[:start] + body + source[end:]
        if abs(delta) >= 0.5:
            print(f"  {name:<24} {category:<12} peak {peak:6.1f}  loud {loud:6.1f}  -> {delta:+.1f} dB")
        worst = max(worst, abs(delta))
    for f, src in sources.items():
        f.write_text(src, encoding="utf-8")
    print(f"{len(rows)} presets checked, largest correction {worst:.1f} dB")


if __name__ == "__main__":
    main(sys.argv[1], "--drums" in sys.argv[2:], "--headroom" in sys.argv[2:])
