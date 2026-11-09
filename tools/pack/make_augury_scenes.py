"""AUGURY Scenes: ten AUSPICE sounds, each with an AUGURY morph scene in its preset.

Corner A is the AUSPICE sound itself; B, C and D are variations of it that a player flies to with the bird.
Only continuous controls change between corners (cutoff, envelopes, mixes, effect sends...): switches follow
the nearest corner, and a switch flipping in the middle of a flight is a jump, not a morph. Where a corner needs
a modulation route A does not use, A gets the route with amount 0 (the same sound, sample for sample).

Each scene preset is built by the real plugin (`augur_preset_audit --augury-scene`): B, C and D are level-matched
to A, so a flight changes the sound, not the volume. Then the bird flies around the pad while the sound plays
(`--augury-flight`) and the WAV goes to build/augury_wav.

    tools\\.venv\\Scripts\\python.exe tools/pack/make_augury_scenes.py
"""
import pathlib
import re
import shutil
import subprocess
import sys
import zipfile

ROOT = pathlib.Path(__file__).resolve().parents[2]
AUDIT = ROOT / "build/win-release/plugin/augur_preset_audit_artefacts/Release/augur_preset_audit.exe"
SOURCE = ROOT / "packs/AUSPICE"
OUT = ROOT / "packs/AUGURY Scenes"
WORK = ROOT / "build/augury_scenes"
WAV = ROOT / "build/augury_wav"
DIST = ROOT / "packs/dist/TONAL LAB - AUGUR-5 AUGURY Scenes (10 presets).zip"

# Matrix sources / destinations (Parameters.h): LFO 2 = 9, FILTER CUTOFF = 4.
LFO2, CUT = 9, 4

SCENES = [
    {
        "src": "PAD/Golden Hour Strings",
        "all": {"mm4_src": LFO2, "mm4_dst": CUT, "mm4_amt": 0.0, "lfo2_sync": 1, "lfo2_div": 7, "lfo2_wave": 1},
        "B": ("Bloom", {"flt_cutoff": 5200, "flt_env_amt": 0.3, "osc2_fine": 15, "aenv_a": 0.35, "fx_chorus_mix": 0.55,
                         "fx_reverb_mix": 0.38}),
        "C": ("Night", {"flt_cutoff": 650, "flt_reso": 0.18, "aenv_a": 1.6, "aenv_r": 5.5, "fenv_a": 1.5, "lfo_amount": 0.35,
                         "fx_reverb_mix": 0.45, "fx_reverb_decay": 8.0}),
        "D": ("Pulse", {"mm4_amt": 0.32, "flt_cutoff": 1300, "flt_reso": 0.3, "aenv_a": 0.15, "fx_chorus_mix": 0.3}),
    },
    {
        "src": "BASS/Augury Rubber",
        "B": ("Snap", {"fenv_d": 0.09, "flt_env_amt": 0.75, "flt_cutoff": 1500, "aenv_d": 0.3, "aenv_s": 0.55}),
        "C": ("Sub", {"flt_cutoff": 520, "flt_reso": 0.15, "flt_env_amt": 0.3, "mix_osc2": 0.25, "mix_sub": 0.55,
                       "mix_drive": 0.25}),
        "D": ("Growl", {"flt_reso": 0.68, "mix_drive": 0.8, "osc1_pw": 16, "fenv_d": 0.35, "flt_env_amt": 0.7}),
    },
    {
        "src": "LEAD/Herald of Dawn",
        "B": ("Scream", {"flt_cutoff": 4500, "flt_reso": 0.35, "mix_drive": 0.5, "flt_env_amt": 0.4}),
        "C": ("Flute", {"flt_cutoff": 700, "flt_env_amt": 0.1, "aenv_a": 0.06, "mix_osc2": 0.2, "lfo_amount": 0.5,
                         "fx_reverb_mix": 0.3}),
        "D": ("Distant", {"delay_mix": 0.36, "delay_fb": 0.55, "fx_reverb_mix": 0.4, "flt_cutoff": 1200, "glide": 0.16}),
    },
    {
        "src": "PLUCK/Harbor Lights",
        "B": ("Glass", {"flt_cutoff": 3000, "flt_env_amt": 0.5, "aenv_d": 0.6, "fx_reverb_mix": 0.25}),
        "C": ("Felt", {"flt_cutoff": 480, "flt_env_amt": 0.25, "aenv_d": 2.4, "aenv_r": 1.2, "fx_chorus_mix": 0.5}),
        "D": ("Wide", {"voice_spread": 1.0, "fx_chorus_mix": 0.6, "fx_reverb_mix": 0.34, "fx_reverb_decay": 4.0,
                        "flt_reso": 0.35}),
    },
    {
        "src": "KEYS/Warm Poly Keys",
        "B": ("Bright", {"flt_cutoff": 3200, "flt_env_amt": 0.45, "aenv_d": 1.5}),
        "C": ("Mellow", {"flt_cutoff": 620, "flt_env_amt": 0.2, "mix_osc2": 0.3, "fx_chorus_mix": 0.5, "aenv_r": 1.6}),
        "D": ("Hold", {"aenv_s": 0.9, "fenv_s": 0.5, "flt_cutoff": 1600, "fx_reverb_mix": 0.3}),
    },
    {
        "src": "BRASS/Copper Anthem",
        "B": ("Bold", {"flt_cutoff": 2600, "flt_env_amt": 0.65, "pm_fenv_amt": 0.5, "mix_drive": 0.35}),
        "C": ("Soft", {"flt_cutoff": 700, "aenv_a": 0.25, "fenv_a": 0.3, "flt_env_amt": 0.35, "fx_reverb_mix": 0.3}),
        "D": ("Swell", {"aenv_a": 1.2, "fenv_a": 1.5, "fenv_d": 3.0, "flt_env_amt": 0.6, "fx_reverb_mix": 0.3}),
    },
    {
        "src": "ARP/Silver Augury",
        "B": ("Open", {"flt_cutoff": 3000, "flt_env_amt": 0.4, "fenv_d": 0.4}),
        "C": ("Muted", {"flt_cutoff": 800, "flt_env_amt": 0.25, "aenv_d": 0.35, "delay_mix": 0.28}),
        "D": ("Cascade", {"delay_fb": 0.45, "delay_mix": 0.3, "fx_reverb_mix": 0.28, "arp_gate": 0.7}),
    },
    {
        "src": "DRONE/Fifth of Dusk",
        "B": ("Gold", {"flt_cutoff": 2400, "flt_reso": 0.15, "mix_osc2": 0.7, "fx_reverb_mix": 0.36}),
        "C": ("Dusk", {"flt_cutoff": 450, "flt_reso": 0.3, "mix_sub": 0.3, "fx_reverb_mix": 0.4}),
        "D": ("Breath", {"mm2_amt": 0.3, "lfo_rate": 0.667, "voice_detune": 0.7, "mix_noise": 0.12}),
    },
    {
        "src": "TEXTURE/Phase Wind",
        "B": ("Gust", {"mix_noise": 0.75, "flt_cutoff": 4500, "fx_phaser_rate": 0.25, "fx_phaser_feedback": 0.75}),
        "C": ("Calm", {"mix_noise": 0.15, "flt_cutoff": 900, "fx_reverb_mix": 0.45, "mix_osc2": 0.5}),
        "D": ("Whirl", {"fx_phaser_rate": 1.2, "flt_reso": 0.55, "flt_cutoff": 1600, "fx_phaser_feedback": 0.8}),
    },
    {
        "src": "FX/White Horizon",
        "B": ("Rush", {"flt_reso": 0.45, "mm2_amt": 0.9, "menv_a": 1.5}),
        "C": ("Distant", {"flt_cutoff": 260, "fx_reverb_mix": 0.42, "fx_reverb_decay": 6.0}),
        "D": ("Siren", {"flt_reso": 0.5, "mm2_amt": 0.5, "flt_cutoff": 600, "fx_reverb_mix": 0.4}),
    },
]


def fmt(x):
    return ("%d" % x) if float(x).is_integer() else ("%.5g" % x)


def with_values(text, values, name):
    """The preset's text with these parameter values (added where the preset leaves them at default) and name."""
    for pid, v in values.items():
        line = '    <P id="%s" value="%s"/>' % (pid, fmt(v))
        pattern = re.compile(r'    <P id="%s" value="[^"]*"/>' % re.escape(pid))
        if pattern.search(text):
            text = pattern.sub(line, text)
        else:
            text = text.replace("  </PARAMS>", line + "\n  </PARAMS>")
    return re.sub(r'(<AUGUR5_PRESET [^>]*name=)"[^"]*"', lambda m: m.group(1) + '"%s"' % name, text, count=1)


def main():
    if not AUDIT.exists():
        sys.exit("build the audit tool first (cmake --build build/win-release --target augur_preset_audit)")
    shutil.rmtree(OUT, ignore_errors=True)
    shutil.rmtree(WORK, ignore_errors=True)
    shutil.rmtree(WAV, ignore_errors=True)
    OUT.mkdir(parents=True)
    WAV.mkdir(parents=True)
    failures = 0
    for scene in SCENES:
        src = SOURCE / (scene["src"] + ".augur5")
        name = src.stem
        text = src.read_text(encoding="utf-8")
        base = dict(scene.get("all", {}))
        folder = WORK / name
        folder.mkdir(parents=True)
        files = []
        for corner in "ABCD":
            label, values = (name, {}) if corner == "A" else scene[corner]
            path = folder / ("%s.augur5" % corner)
            path.write_text(with_values(text, {**base, **values}, label), encoding="utf-8")
            files.append(path)
        out = OUT / ("%s.augur5" % name)
        print("== %s" % name, flush=True)
        r = subprocess.run([str(AUDIT), "--augury-scene", str(out)] + [str(f) for f in files], cwd=ROOT)
        failures += r.returncode != 0
        r = subprocess.run([str(AUDIT), "--augury-flight", str(out), str(WAV / ("%s.wav" % name))], cwd=ROOT)
        failures += r.returncode != 0

    (OUT / "README.txt").write_text(
        "AUGUR-5 - AUGURY Scenes (TONAL LAB)\n"
        "\n"
        "Ten sounds from AUSPICE, each with an AUGURY morph scene saved in the preset.\n"
        "Load one, open the AUGURY tab and drag the bird: corner A is the sound as it is in AUSPICE;\n"
        "B, C and D are variations of it, level-matched, so a flight changes the sound, not the volume.\n"
        "RECALL plays a corner exactly. Loading a preset without a scene keeps the corners you have.\n"
        "\n"
        "Needs an AUGUR-5 build that reads AUGURY scenes from presets (earlier versions load the sound\n"
        "of corner A and ignore the scene).\n", encoding="utf-8")
    DIST.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(DIST, "w", zipfile.ZIP_DEFLATED) as z:
        for f in sorted(OUT.iterdir()):
            z.write(f, "AUGURY Scenes/" + f.name)
    print("%d scenes in %s; WAVs in %s; zip %s" % (len(SCENES), OUT, WAV, DIST))
    sys.exit(1 if failures else 0)


if __name__ == "__main__":
    main()
