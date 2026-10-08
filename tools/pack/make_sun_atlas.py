"""AUGUR-5 - SUN ATLAS: the instrument's large bank, in the style of the series' atlas libraries (PYTHIA 32 MOON ATLAS,
MANTIS-37 HORIZON ATLAS).

1000 sounds ordered by category (PAD, KEYS, PLUCK, LEAD, BASS, BRASS, ARP, SEQ, TEXTURE, DRONE, FX). Designed for the
instrument it is: two VCOs per voice with hard sync, POLY-MOD (filter envelope and OSC B into OSC A's pitch, pulse width
and the cutoff), the 4-pole filter in its Rev 3 / Rev 1 voicings (and CASCADE, MULTIMODE, BITE), unison, the LFO and the
8-slot matrix, the arpeggiator and the effects rack. Resonance stays moderate on purpose (<= 0.45, acid basses <= 0.5,
FX may ping): the beauty comes from the oscillators, slow modulation and the effect chain, not a whistling filter.

Style families (internal tags; no preset name mentions artists or brands):
  IDM     wide detune, tape wow and flutter, chorus with hiss, a little crush, broken sequences
  NEO     felt keys (triangles, low filter, velocity -> brightness), plates, soft tape, resonance <= 0.22
  CINE    braams, brass sections, ostinatos, long-attack strings, 6-12 s halls and shimmer
  TECHNO  wide gliding leads, plucks with synced echoes (1/8., 1/4.), rolling basses, big arps, bus compression
  HOUSE   synth brass, organs, round basses, Juno chorus, plates and rooms, syncopated stabs
  COLOR   bells and glass (poly-mod FM, narrow pulses), shimmer, ensemble, ping-pong space
  HYPNO   drifting pitch and swaying filters, phasers, saturated tape, spring tanks

Pipeline (as in the series): 3 candidates per slot, each played through the real processor by
`augur_preset_audit --pack` the way its category is used (held chords for pads, chord stabs for keys and brass, rolling
sixteenths for basses, a line for plucks and leads, a held chord for arps, one held key for sequences) and levelled to
-16 dB short-term loudness (loudest 400 ms) with peaks <= -1 dBFS; quality gates (non-finite, silence, DC, clipping,
more than 1/8 of the energy above 6 kHz, basses wide / thin / bright, endless tails), the brightest 6 % of every
category out; then farthest-point sampling on the timbre fingerprint with the effects off, inside every category x
family, so no two presets sound alike. Finally every preset is played again in reverse order to prove it sounds the
same whatever was played before it. Report: docs/pack_sun_atlas.md.

Usage:  python tools/pack/make_sun_atlas.py [--seed N] [--quick] [--no-audit] [--install]
"""
import concurrent.futures
import csv
import io
import math
import os
import pathlib
import random
import re
import shutil
import subprocess
import sys
from xml.sax.saxutils import quoteattr

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import timbre  # noqa: E402

ROOT = pathlib.Path(__file__).resolve().parents[2]
PACK = "SUN ATLAS"
OUT = ROOT / "packs" / PACK
STAGE = ROOT / "build" / "sun_atlas_stage"
REPORT = ROOT / "docs" / "pack_sun_atlas.md"
AUDIT = ROOT / "build/win-release/plugin/augur_preset_audit_artefacts/Release/augur_preset_audit.exe"
INSTALL = pathlib.Path(os.environ.get("APPDATA", "")) / "TONAL LAB" / "AUGUR-5" / "Presets" / PACK
TARGET_LOUD = -16.0
PEAK_MAX = -1.0
SPIKY_DB = 4.0     # a sound whose peak keeps it this far under the target gets its transient tamed
SPIKY_DROP_DB = 6.0
CHUNK = 50
CANDIDATES_PER_SLOT = 3
JOBS = max(2, (os.cpu_count() or 4) - 1)

COUNTS = {"PAD": 160, "KEYS": 95, "PLUCK": 100, "LEAD": 95, "BASS": 120, "BRASS": 60, "ARP": 100, "SEQ": 90, "TEXTURE": 85,
          "DRONE": 55, "FX": 40}
ROLE = {"PAD": "pad", "KEYS": "keys", "PLUCK": "pluck", "LEAD": "lead", "BASS": "bass", "BRASS": "brass", "ARP": "arp", "SEQ": "seq",
        "TEXTURE": "pad", "DRONE": "drone", "FX": "fx"}
NOTE = {"PAD": 57, "KEYS": 57, "PLUCK": 60, "LEAD": 62, "BASS": 36, "BRASS": 50, "ARP": 57, "SEQ": 45, "TEXTURE": 57, "DRONE": 45, "FX": 60}
FAMILIES = ["IDM", "NEO", "CINE", "TECHNO", "HOUSE", "COLOR", "HYPNO"]
WEIGHTS = {  # how much of each category every family designs
    "PAD": [1, 2, 3, 1, 1, 3, 2], "KEYS": [1, 4, 1, 1, 3, 2, 1], "PLUCK": [1, 1, 1, 4, 2, 3, 2], "LEAD": [2, 1, 1, 4, 1, 1, 2],
    "BASS": [2, 0, 2, 4, 3, 0, 2], "BRASS": [0, 0, 4, 1, 3, 1, 0], "ARP": [2, 1, 2, 3, 1, 2, 3], "SEQ": [3, 0, 1, 3, 1, 1, 4],
    "TEXTURE": [3, 1, 2, 0, 0, 2, 3], "DRONE": [1, 1, 3, 0, 0, 2, 3], "FX": [3, 0, 3, 2, 0, 1, 1],
}


def slots():
    out = {}
    for cat, n in COUNTS.items():
        w = WEIGHTS[cat]
        alloc = [round(n * x / sum(w)) for x in w]
        alloc[w.index(max(w))] += n - sum(alloc)  # rounding goes to the leading family
        out[cat] = {f: a for f, a in zip(FAMILIES, alloc) if a > 0}
    return out


# ============================================================================================== parameter table
def load_params():
    """The instrument's own parameter list (ids, kinds, ranges) from `augur_preset_audit --params`."""
    out = subprocess.run([str(AUDIT), "--params"], capture_output=True, text=True, cwd=str(ROOT)).stdout
    table = {}
    for row in csv.DictReader(io.StringIO(out), delimiter=";"):
        table[row["id"]] = (row["kind"], float(row["min"]), float(row["max"]), float(row["default"]))
    if len(table) < 150:
        raise SystemExit("could not read the parameter table: build augur_preset_audit first")
    return table


PARAMS = {}
GLOBAL = {"master_volume", "quality", "offline_quality"}


def finish(p):
    """Every value clamped into the instrument's real range; unknown ids are a bug."""
    out = {}
    for k, v in p.items():
        if k.startswith("_"):
            continue
        if k not in PARAMS:
            raise KeyError("unknown parameter id: " + k)
        if k in GLOBAL:
            continue
        kind, lo, hi, _ = PARAMS[k]
        v = min(hi, max(lo, float(v)))
        if kind == "bool":
            v = 1.0 if v > 0.5 else 0.0
        elif kind in ("choice", "int"):
            v = float(round(v))
        out[k] = v
    return out


# ============================================================================================== vocabulary
FENV, AENV, OSC2, LFO, WHEEL, VEL, AT, NOISE, MENV, LFO2, KEY, RAND = range(12)  # matrix sources
F1, F2, PW1, PW2, CUT, RES, AMP, LFORATE, FM, RING, SUBL, DRV, L1, L2, LN, L2RATE = range(16)  # destinations
REV3, REV1, CASCADE, MULTI, BITE = range(5)  # filter models
LP, BP, HP = range(3)
SINE2, TRI2, SAWUP2, SAWDN2, SQR2, SH2, SMOOTH2 = range(7)  # LFO 2 waves
TRI1, SAW1, SQR1, SH1 = range(4)  # LFO waves
PLATE, ROOM, HALL, SHIMMER, SPRING = range(5)
TUBE, DIODE, TAPE, FOLD, CRUSH = range(5)
JUNO1, JUNO2, JUNO12, DIM, ENS = range(5)
# Rack note values (phaser / flanger / echo) and the delay's own list.
N = {"1/32": 0, "1/16T": 1, "1/32.": 2, "1/16": 3, "1/8T": 4, "1/16.": 5, "1/8": 6, "1/4T": 7, "1/8.": 8, "1/4": 9, "1/2T": 10,
     "1/4.": 11, "1/2": 12, "1/2.": 13, "1/1": 14, "2/1": 15}
D = {"1/32": 0, "1/16T": 1, "1/16": 2, "1/16D": 3, "1/8T": 4, "1/8": 5, "1/8D": 6, "1/4T": 7, "1/4": 8, "1/4D": 9, "1/2": 10, "1 BAR": 11}
ARP_RATE = {"1/4": 0, "1/8D": 1, "1/8": 2, "1/8T": 3, "1/16D": 4, "1/16": 5, "1/16T": 6, "1/32": 7}
UP, DOWN, UPDOWN, RANDOMARP, ORDER = range(5)


def pitch_amt(semitones):
    """Matrix amount for a pitch destination (the engine maps a -> sign(a) a^2 24 semitones)."""
    return math.copysign(math.sqrt(abs(semitones) / 24.0), semitones)


# ============================================================================================== building blocks
def osc_a(p, saw=True, pulse=False, pw=50.0, semi=0, fine=0.0, octave=0, sync=False):
    p.update(osc1_saw=int(saw), osc1_pulse=int(pulse), osc1_pw=pw, osc1_freq=semi, osc1_fine=fine, osc1_oct=octave + 2, osc1_sync=int(sync))


def osc_b(p, saw=True, tri=False, pulse=False, pw=50.0, semi=0, fine=0.0, octave=0, lofreq=False, kbd=True):
    p.update(osc2_saw=int(saw), osc2_tri=int(tri), osc2_pulse=int(pulse), osc2_pw=pw, osc2_freq=semi, osc2_fine=fine,
             osc2_oct=octave + 2, osc2_lofreq=int(lofreq), osc2_kbd=int(kbd))


def mixer(p, a=0.8, b=0.6, noise=0.0, sub=0.0, ring=0.0, drive=0.12, sub_oct=0, xmod=0.0):
    p.update(mix_osc1=a, mix_osc2=b, mix_noise=noise, mix_sub=sub, mix_ring=ring, mix_drive=drive, sub_oct=sub_oct, osc_xmod=xmod)


def filt(p, cutoff, reso=0.1, env=0.3, model=REV3, key=2, vel=0.0, slope=0, mode=LP, hpf=10.0):
    p.update(flt_cutoff=cutoff, flt_reso=reso, flt_env_amt=env, flt_model=model, flt_keytrack=key, flt_velocity=vel, flt_slope=slope,
             flt_mode=mode, hpf_cutoff=hpf)


def env(p, which, a, d, s, r):
    p.update({which + "_a": a, which + "_d": d, which + "_s": s, which + "_r": r})


def route(p, src, dst, amt):
    slot = p.get("_slots", 0) + 1
    if slot > 8:
        return
    p.update({"mm%d_src" % slot: src, "mm%d_dst" % slot: dst, "mm%d_amt" % slot: amt, "_slots": slot})


def voices(p, n=6, unison=False, detune=0.3, spread=0.6, legato=False, glide=0.0, mode=0):
    p.update(voice_count=n, unison=int(unison), voice_detune=detune, voice_spread=spread, legato=int(legato), glide=glide, voice_mode=mode)


def poly_mod(p, fenv=0.0, oscb=0.0, freq=True, pw=False, flt=False):
    p.update(pm_on=1, pm_fenv_amt=fenv, pm_osc2_amt=oscb, pm_dst_freqa=int(freq), pm_dst_pwa=int(pw), pm_dst_filter=int(flt))


def vibrato(p, r, cents=(8, 16), rate=(4.8, 5.8), delay=(0.25, 0.7)):
    if "lfo_rate" in p:
        return  # the shared LFO already has a job in this sound (tremolo, sway)
    c = r.uniform(*cents) / 100.0
    p.update(lfo_rate=r.uniform(*rate), lfo_delay=r.uniform(*delay), lfo_wave=TRI1, lfo_amount=1.0, lfo_sync=0)
    route(p, LFO, F1, pitch_amt(c))
    route(p, LFO, F2, pitch_amt(c))


def drift(p, r, cents=(4, 10)):
    """Slow per-voice pitch drift: LFO 2 (smooth random, free) a few cents on both oscillators."""
    if "lfo2_wave" in p:
        return  # LFO 2 already moves something else (PWM, a swaying filter)
    c = r.uniform(*cents) / 100.0
    p.update(lfo2_wave=SMOOTH2, lfo2_rate=r.uniform(0.08, 0.3), lfo2_retrig=0, lfo2_sync=0)
    route(p, LFO2, F1, pitch_amt(c))
    route(p, LFO2, F2, -pitch_amt(c * 0.7))


def sway(p, r, octaves=(0.12, 0.3), rate=(0.05, 0.25)):
    """Slow swaying filter from the shared LFO."""
    p.update(lfo_wave=TRI1, lfo_rate=r.uniform(*rate), lfo_amount=1.0, lfo_delay=0.0)
    route(p, LFO, CUT, r.uniform(*octaves) / 6.0)


def arp(p, mode, rate, octaves, gate, swing=0.0):
    p.update(arp_on=1, arp_mode=mode, arp_rate=ARP_RATE[rate], arp_oct=octaves, arp_gate=gate, arp_swing=swing, arp_latch=0)


# ---------------------------------------------------------------------------------------------- effects rack
def reverb(p, kind, size, decay, mix, predelay=15.0, damp=0.45, lowcut=140.0, mod=0.4, width=1.0, shimmer=0.4, pitch=3):
    p.update(fx_reverb_on=1, fx_reverb_type=kind, fx_reverb_size=size, fx_reverb_decay=decay, fx_reverb_mix=mix,
             fx_reverb_predelay=predelay, fx_reverb_damp=damp, fx_reverb_lowcut=lowcut, fx_reverb_mod=mod, fx_reverb_width=width,
             fx_reverb_shimmer=shimmer, fx_reverb_pitch=pitch, fx_reverb_freeze=0)


def echo(p, mode, mix, intensity=0.4, time_ms=None, sync=None, wow=0.25, flutter=0.2, sat=0.35, bass=0.0, treble=-2.0, age=0.3,
         width=0.6, spring=0.25):
    p.update(fx_echo_on=1, fx_echo_mode=mode, fx_echo_mix=mix, fx_echo_intensity=intensity, fx_echo_wow=wow, fx_echo_flutter=flutter,
             fx_echo_sat=sat, fx_echo_bass=bass, fx_echo_treble=treble, fx_echo_age=age, fx_echo_width=width, fx_echo_spring=spring)
    if sync is not None:
        p.update(fx_echo_sync=1, fx_echo_division=N[sync])
    else:
        p.update(fx_echo_sync=0, fx_echo_time=time_ms if time_ms else 177.0)


def delay(p, div, fb, mix, pingpong=False):
    p.update(delay_on=1, delay_sync=1, delay_div=D[div], delay_fb=fb, delay_mix=mix, delay_pingpong=int(pingpong))


def chorus(p, mode, mix=0.4, rate=1.0, depth=1.0, hiss=0.1, tone=0.5, width=1.0):
    p.update(fx_chorus_on=1, fx_chorus_mode=mode, fx_chorus_mix=mix, fx_chorus_rate=rate, fx_chorus_depth=depth, fx_chorus_hiss=hiss,
             fx_chorus_tone=tone, fx_chorus_width=width)


def phaser(p, mix=0.5, rate=0.3, depth=0.8, center=700.0, fb=0.3, stages=1, spread=90.0, lfo=1, sync=None):
    p.update(fx_phaser_on=1, fx_phaser_mix=mix, fx_phaser_rate=rate, fx_phaser_depth=depth, fx_phaser_center=center,
             fx_phaser_feedback=fb, fx_phaser_stages=stages, fx_phaser_spread=spread, fx_phaser_lfo=lfo)
    if sync:
        p.update(fx_phaser_sync=1, fx_phaser_division=N[sync])


def flanger(p, mix=0.5, rate=0.2, depth=0.7, manual=2.0, fb=0.5, tz=False, spread=90.0):
    p.update(fx_flanger_on=1, fx_flanger_mix=mix, fx_flanger_rate=rate, fx_flanger_depth=depth, fx_flanger_manual=manual,
             fx_flanger_feedback=fb, fx_flanger_tz=int(tz), fx_flanger_spread=spread)


def drive(p, model, amount, tone=0.6, mix=1.0, bias=0.0):
    p.update(fx_drive_on=1, fx_drive_model=model, fx_drive_amount=amount, fx_drive_tone=tone, fx_drive_mix=mix, fx_drive_bias=bias)


def comp(p, threshold=-14.0, ratio=1, attack=3, release=4, makeup=2.0, mix=1.0, schpf=60.0):
    p.update(fx_comp_on=1, fx_comp_threshold=threshold, fx_comp_ratio=ratio, fx_comp_attack=attack, fx_comp_release=release,
             fx_comp_makeup=makeup, fx_comp_mix=mix, fx_comp_schpf=schpf)


def fuzz(p, sustain, tone, mix, volume=0.5):
    p.update(fuzz_on=1, fuzz_sustain=sustain, fuzz_tone=tone, fuzz_mix=mix, fuzz_volume=volume)


SPATIAL = ("fx_chorus_on", "fx_reverb_on", "fx_echo_on", "delay_on", "fx_phaser_on", "fx_flanger_on")


def mono_low_end(p):
    """Basses stay mono and dry: no spatial effects, no stereo spread; fuzz only in parallel."""
    for k in SPATIAL:
        p.pop(k, None)
    p["voice_spread"] = 0.0
    if p.get("fuzz_on"):
        p["fuzz_mix"] = min(p.get("fuzz_mix", 0.5), 0.4)
        p["fuzz_tone"] = min(p.get("fuzz_tone", 0.5), 0.45)


# ============================================================================================== archetypes
def base(r, cat):
    p = dict(analog_age=r.uniform(0.25, 0.5), voice_detune=r.uniform(0.2, 0.45), voice_spread=r.uniform(0.4, 0.8),
             amp_velocity=r.uniform(0.3, 0.55), osc_model=1 if r.random() < 0.3 else 0, at_amount=r.uniform(0.0, 0.3))
    route(p, WHEEL, CUT, r.uniform(0.15, 0.28))  # the mod wheel opens the filter by about 1 to 1.7 octaves
    return p


def a_pad(r, kind):
    p = {}
    voices(p, 8, detune=r.uniform(0.3, 0.5), spread=r.uniform(0.6, 0.9))
    env(p, "fenv", r.uniform(0.5, 2.5), r.uniform(1.5, 4.0), r.uniform(0.4, 0.7), r.uniform(1.5, 4.0))
    env(p, "aenv", r.uniform(0.5, 2.2), r.uniform(1.0, 2.5), r.uniform(0.8, 0.95), r.uniform(1.8, 4.5))
    if kind == "strings":
        osc_a(p)
        osc_b(p, fine=r.uniform(6, 12))
        mixer(p, 0.7, 0.6)
        filt(p, r.uniform(1400, 3200), r.uniform(0.05, 0.15), r.uniform(0.15, 0.3), r.choice([REV3, REV1]), key=1)
        chorus(p, r.choice([ENS, JUNO12]), r.uniform(0.35, 0.5), rate=r.uniform(0.6, 1.2))
    elif kind == "pwm":
        osc_a(p, saw=False, pulse=True, pw=r.uniform(38, 46))
        osc_b(p, saw=False, pulse=True, pw=r.uniform(40, 48), octave=r.choice([-1, 0]), fine=r.uniform(3, 7))
        mixer(p, 0.7, 0.5)
        filt(p, r.uniform(1000, 2400), r.uniform(0.08, 0.2), r.uniform(0.15, 0.3), key=1)
        p.update(lfo2_wave=TRI2, lfo2_rate=r.uniform(0.15, 0.5), lfo2_retrig=0)
        route(p, LFO2, PW1, r.uniform(0.25, 0.4))
        route(p, LFO2, PW2, -r.uniform(0.25, 0.4))
        chorus(p, JUNO2, r.uniform(0.3, 0.45))
    elif kind == "fifths":
        osc_a(p, saw=r.random() < 0.7, pulse=r.random() < 0.4, pw=40)
        osc_b(p, saw=r.random() < 0.5, tri=True, semi=r.choice([7, -5]), fine=r.uniform(2, 6))
        if not p["osc1_saw"] and not p["osc1_pulse"]:
            p["osc1_saw"] = 1
        mixer(p, 0.65, 0.5)
        filt(p, r.uniform(900, 2000), r.uniform(0.1, 0.25), r.uniform(0.1, 0.3), key=1)
    elif kind == "glass":
        osc_a(p, saw=False, pulse=True, pw=r.uniform(10, 20))
        osc_b(p, saw=False, tri=True, semi=r.choice([12, 19, 24]))
        mixer(p, 0.4, 0.6)
        poly_mod(p, oscb=r.uniform(0.06, 0.18))
        filt(p, r.uniform(3000, 6000), r.uniform(0.05, 0.15), 0.15, MULTI, key=2, slope=1, hpf=r.uniform(120, 220))
        env(p, "aenv", r.uniform(0.2, 0.8), 2.0, r.uniform(0.6, 0.85), r.uniform(2.5, 5.0))
    elif kind == "breath":
        osc_a(p)
        osc_b(p, saw=False, tri=True, octave=-1)
        mixer(p, 0.3, 0.35, noise=r.uniform(0.18, 0.32))
        filt(p, r.uniform(900, 1800), r.uniform(0.2, 0.32), 0.15, MULTI, key=1, mode=BP, slope=1)
        p.update(lfo2_wave=SMOOTH2, lfo2_rate=r.uniform(0.1, 0.4), lfo2_retrig=0)
        route(p, LFO2, CUT, r.uniform(0.03, 0.06))
    elif kind == "sync_swell":
        osc_a(p, semi=r.choice([7, 12]), sync=True)
        osc_b(p, fine=r.uniform(3, 7))
        mixer(p, 0.6, 0.45)
        env(p, "menv", r.uniform(1.5, 4.0), 3.0, 0.6, 3.0)
        route(p, MENV, F1, pitch_amt(r.uniform(4, 9)))
        filt(p, r.uniform(1800, 3200), r.uniform(0.08, 0.18), 0.2, key=1)
        chorus(p, JUNO1, r.uniform(0.3, 0.45))
    elif kind == "dark":
        osc_a(p)
        osc_b(p, fine=r.uniform(8, 14), octave=r.choice([-1, 0]))
        mixer(p, 0.7, 0.55, sub=r.uniform(0.0, 0.25))
        filt(p, r.uniform(400, 900), r.uniform(0.12, 0.28), r.uniform(0.2, 0.4), REV1, key=1)
        p.update(osc_model=1, analog_age=r.uniform(0.5, 0.75))
    elif kind == "choir":
        osc_a(p, saw=False, pulse=True, pw=r.uniform(25, 35))
        osc_b(p, octave=1, fine=r.uniform(4, 9))
        mixer(p, 0.7, 0.3, noise=0.05)
        filt(p, r.uniform(1000, 1600), r.uniform(0.15, 0.25), 0.1, key=1)
        p.update(lfo2_wave=SMOOTH2, lfo2_rate=r.uniform(0.2, 0.6), lfo2_retrig=0)
        route(p, LFO2, CUT, r.uniform(0.02, 0.04))
        chorus(p, ENS, r.uniform(0.4, 0.55), rate=0.7)
    elif kind == "polymod":
        osc_a(p, saw=r.random() < 0.6, pulse=True, pw=r.uniform(30, 50))
        osc_b(p, saw=False, tri=True, semi=r.choice([7, 12, 5]), fine=r.uniform(1, 4))
        mixer(p, 0.7, 0.3)
        poly_mod(p, fenv=r.uniform(0.0, 0.15), oscb=r.uniform(0.08, 0.22), pw=r.random() < 0.4)
        filt(p, r.uniform(1500, 3000), r.uniform(0.08, 0.2), 0.25, key=1)
    else:  # airy
        osc_a(p, saw=False, pulse=True, pw=r.uniform(15, 25))
        osc_b(p, saw=False, tri=True, octave=1)
        mixer(p, 0.25, 0.6, noise=r.uniform(0.06, 0.12))
        filt(p, r.uniform(3500, 6500), 0.08, 0.1, key=2, hpf=r.uniform(220, 380))
    reverb(p, r.choice([HALL, HALL, SHIMMER]), r.uniform(1.2, 1.8), r.uniform(3.5, 7.0), r.uniform(0.25, 0.38), predelay=r.uniform(20, 60))
    return p


def a_keys(r, kind):
    p = {}
    voices(p, 8, detune=r.uniform(0.15, 0.3), spread=r.uniform(0.3, 0.6))
    p["flt_velocity"] = r.uniform(0.2, 0.45)
    if kind == "epiano":
        osc_a(p, saw=False, pulse=True, pw=r.uniform(40, 50))
        osc_b(p, saw=False, tri=True, semi=r.choice([12, 19]))
        mixer(p, 0.35, 0.55)
        poly_mod(p, fenv=r.uniform(0.0, 0.1), oscb=r.uniform(0.12, 0.25))
        filt(p, r.uniform(2000, 3600), 0.05, r.uniform(0.2, 0.35), key=2, vel=r.uniform(0.3, 0.5))
        env(p, "fenv", 0.002, r.uniform(0.4, 0.9), 0.2, 0.4)
        env(p, "aenv", 0.002, r.uniform(1.4, 2.4), r.uniform(0.0, 0.2), r.uniform(0.35, 0.6))
        p.update(lfo_rate=r.uniform(3.5, 5.5), lfo_amount=1.0, lfo_wave=TRI1, lfo_delay=0)
        route(p, LFO, AMP, -r.uniform(0.08, 0.18))  # tremolo
        chorus(p, JUNO1, r.uniform(0.25, 0.4))
    elif kind == "organ":
        osc_a(p, saw=False, pulse=True, pw=50)
        osc_b(p, saw=False, pulse=True, pw=50, semi=12, fine=r.uniform(0, 3))
        mixer(p, 0.7, 0.45, sub=r.uniform(0.2, 0.45))
        filt(p, r.uniform(3000, 6000), 0.0, 0.0, key=2, vel=0.0)
        env(p, "fenv", 0.001, 0.1, 1.0, 0.1)
        env(p, "aenv", 0.004, 0.1, 1.0, r.uniform(0.05, 0.12))
        p["amp_velocity"] = 0.1
        if r.random() < 0.5:
            chorus(p, JUNO12, r.uniform(0.35, 0.5), rate=r.uniform(2.5, 4.0))
        else:
            phaser(p, r.uniform(0.3, 0.45), rate=r.uniform(0.4, 0.9), stages=0, fb=0.2)
    elif kind == "felt":
        osc_a(p, saw=True)
        osc_b(p, saw=False, tri=True, fine=r.uniform(2, 5))
        mixer(p, 0.25, 0.75)
        filt(p, r.uniform(700, 1400), 0.05, r.uniform(0.2, 0.35), key=2, vel=r.uniform(0.4, 0.6))
        env(p, "fenv", 0.001, r.uniform(0.8, 1.5), 0.0, 0.5)
        env(p, "aenv", 0.002, r.uniform(2.0, 3.2), 0.0, r.uniform(0.5, 0.9))
    elif kind == "clav":
        osc_a(p, saw=False, pulse=True, pw=r.uniform(14, 24))
        osc_b(p, saw=r.random() < 0.5, pulse=True, pw=30, semi=12)
        mixer(p, 0.75, 0.25)
        filt(p, r.uniform(1300, 2200), r.uniform(0.2, 0.32), r.uniform(0.4, 0.55), MULTI, key=2, mode=r.choice([LP, BP]), slope=1)
        env(p, "fenv", 0.001, r.uniform(0.15, 0.3), 0.0, 0.1)
        env(p, "aenv", 0.001, r.uniform(0.35, 0.6), 0.0, 0.1)
    elif kind == "bell":
        osc_a(p, saw=False, pulse=True, pw=r.uniform(10, 25))
        osc_b(p, saw=False, tri=True, semi=r.choice([19, 24, 7]))
        mixer(p, 0.4, 0.5)
        poly_mod(p, oscb=r.uniform(0.22, 0.42))
        filt(p, r.uniform(4000, 7000), 0.05, 0.15, key=2, hpf=r.uniform(150, 300))
        env(p, "fenv", 0.001, 1.5, 0.3, 1.5)
        env(p, "aenv", 0.001, r.uniform(2.0, 3.5), 0.0, r.uniform(1.5, 2.5))
    elif kind == "poly_keys":
        osc_a(p)
        osc_b(p, fine=r.uniform(5, 9), octave=r.choice([0, 0, -1]))
        mixer(p, 0.75, 0.55)
        filt(p, r.uniform(900, 1700), r.uniform(0.1, 0.25), r.uniform(0.35, 0.5), r.choice([REV3, REV1]), key=2)
        env(p, "fenv", 0.002, r.uniform(0.5, 0.9), r.uniform(0.2, 0.35), 0.4)
        env(p, "aenv", 0.002, 1.0, r.uniform(0.4, 0.6), r.uniform(0.3, 0.5))
    elif kind == "vibes":
        osc_a(p, saw=False, pulse=True, pw=50)
        osc_b(p, saw=False, tri=True, semi=12)
        mixer(p, 0.2, 0.8)
        filt(p, r.uniform(2500, 4000), 0.05, 0.1, key=2)
        env(p, "aenv", 0.001, r.uniform(1.6, 2.6), 0.0, r.uniform(0.8, 1.4))
        p.update(lfo_rate=r.uniform(4.5, 6.0), lfo_amount=1.0, lfo_wave=TRI1, lfo_delay=0)
        route(p, LFO, AMP, -r.uniform(0.15, 0.25))
    else:  # harpsi
        osc_a(p, saw=False, pulse=True, pw=r.uniform(15, 22))
        osc_b(p, semi=12, fine=r.uniform(3, 7))
        mixer(p, 0.6, 0.45)
        filt(p, r.uniform(3000, 4500), 0.08, 0.35, key=2, hpf=r.uniform(150, 260))
        env(p, "fenv", 0.001, 0.2, 0.0, 0.2)
        env(p, "aenv", 0.001, r.uniform(0.7, 1.1), 0.0, r.uniform(0.25, 0.4))
    reverb(p, r.choice([PLATE, ROOM, PLATE]), r.uniform(0.8, 1.2), r.uniform(1.2, 2.5), r.uniform(0.15, 0.25))
    return p


def a_pluck(r, kind):
    p = {}
    voices(p, 6, detune=r.uniform(0.15, 0.3), spread=r.uniform(0.4, 0.7))
    p["flt_velocity"] = r.uniform(0.2, 0.4)
    if kind == "classic":
        osc_a(p)
        osc_b(p, fine=r.uniform(4, 9), octave=r.choice([0, 1]))
        mixer(p, 0.75, 0.45)
        filt(p, r.uniform(400, 900), r.uniform(0.1, 0.3), r.uniform(0.5, 0.72), r.choice([REV3, REV1, CASCADE]), key=2)
        env(p, "fenv", 0.001, r.uniform(0.12, 0.3), 0.0, 0.2)
        env(p, "aenv", 0.001, r.uniform(0.3, 0.6), 0.0, r.uniform(0.2, 0.4))
    elif kind == "sync":
        osc_a(p, semi=r.choice([7, 12, 12, 19]), sync=True)
        osc_b(p, fine=r.uniform(2, 5))
        mixer(p, 0.7, 0.35)
        env(p, "menv", 0.001, r.uniform(0.08, 0.2), 0.0, 0.1)
        route(p, MENV, F1, pitch_amt(r.uniform(7, 14)))
        filt(p, r.uniform(2400, 4000), 0.1, 0.3, key=2)
        env(p, "fenv", 0.001, 0.25, 0.0, 0.2)
        env(p, "aenv", 0.001, r.uniform(0.25, 0.45), 0.0, 0.25)
    elif kind == "kalimba":
        osc_a(p, saw=False, pulse=True, pw=r.uniform(10, 18))
        osc_b(p, saw=False, tri=True)
        mixer(p, 0.25, 0.75)
        poly_mod(p, oscb=r.uniform(0.06, 0.14))
        filt(p, r.uniform(2000, 3200), 0.05, 0.3, key=2, hpf=r.uniform(180, 300))
        env(p, "fenv", 0.001, 0.15, 0.0, 0.2)
        env(p, "aenv", 0.001, r.uniform(0.45, 0.8), 0.0, r.uniform(0.4, 0.7))
    elif kind == "harp":
        osc_a(p)
        osc_b(p, saw=False, tri=True, semi=12)
        mixer(p, 0.5, 0.6)
        filt(p, r.uniform(1300, 2200), 0.05, 0.4, key=2)
        env(p, "fenv", 0.001, r.uniform(0.2, 0.35), 0.1, 0.4)
        env(p, "aenv", 0.001, r.uniform(1.0, 1.6), 0.0, r.uniform(0.8, 1.2))
    elif kind == "polymod":
        osc_a(p, saw=True, pulse=r.random() < 0.3, pw=40)
        osc_b(p, saw=False, tri=True, semi=r.choice([12, 7, 19]))
        mixer(p, 0.75, 0.25)
        poly_mod(p, fenv=r.uniform(0.25, 0.5), oscb=r.uniform(0.0, 0.1))
        filt(p, r.uniform(1400, 2400), 0.12, r.uniform(0.35, 0.5), key=2)
        env(p, "fenv", 0.001, r.uniform(0.08, 0.18), 0.0, 0.1)
        env(p, "aenv", 0.001, r.uniform(0.3, 0.5), 0.0, 0.25)
    elif kind == "noise":
        osc_a(p)
        osc_b(p, saw=False, pulse=True, pw=30, octave=1)
        mixer(p, 0.6, 0.3, noise=r.uniform(0.15, 0.3))
        filt(p, r.uniform(1200, 2400), r.uniform(0.15, 0.3), r.uniform(0.4, 0.55), MULTI, key=2, mode=r.choice([LP, BP]))
        env(p, "fenv", 0.001, r.uniform(0.08, 0.18), 0.0, 0.1)
        env(p, "aenv", 0.001, r.uniform(0.2, 0.35), 0.0, 0.2)
    elif kind == "glass":
        osc_a(p, saw=False, pulse=True, pw=r.uniform(8, 15))
        osc_b(p, saw=False, tri=True, semi=r.choice([19, 24]))
        mixer(p, 0.35, 0.65)
        filt(p, r.uniform(4000, 7000), 0.05, 0.2, key=2, hpf=r.uniform(220, 400))
        env(p, "aenv", 0.001, r.uniform(0.3, 0.6), 0.0, r.uniform(0.5, 0.9))
    else:  # wood
        osc_a(p, saw=False, pulse=True, pw=50)
        osc_b(p, saw=False, tri=True, semi=12)
        mixer(p, 0.6, 0.35)
        filt(p, r.uniform(700, 1200), r.uniform(0.28, 0.4), r.uniform(0.4, 0.6), MULTI, key=2, mode=BP, slope=1)
        env(p, "fenv", 0.001, r.uniform(0.08, 0.15), 0.0, 0.1)
        env(p, "aenv", 0.001, r.uniform(0.18, 0.3), 0.0, 0.15)
    return p


def a_lead(r, kind):
    p = {}
    voices(p, 1, legato=True, glide=r.uniform(0.03, 0.09), spread=0.0, detune=r.uniform(0.2, 0.35))
    env(p, "aenv", r.uniform(0.004, 0.03), 0.6, r.uniform(0.8, 0.95), r.uniform(0.25, 0.6))
    env(p, "fenv", r.uniform(0.005, 0.05), r.uniform(0.4, 0.9), r.uniform(0.3, 0.6), 0.4)
    if kind == "sync":
        osc_a(p, semi=r.choice([5, 7, 12]), sync=True)
        osc_b(p, fine=r.uniform(3, 7))
        mixer(p, 0.75, 0.35)
        poly_mod(p, fenv=r.uniform(0.3, 0.6))
        filt(p, r.uniform(2500, 4500), r.uniform(0.1, 0.2), 0.3, key=2)
    elif kind == "unison":
        osc_a(p)
        osc_b(p, fine=r.uniform(6, 10), octave=r.choice([0, -1]))
        voices(p, r.choice([4, 5, 6]), unison=True, detune=r.uniform(0.35, 0.6), spread=r.uniform(0.4, 0.7), legato=True,
               glide=r.uniform(0.03, 0.08))
        mixer(p, 0.7, 0.55)
        filt(p, r.uniform(2000, 3500), r.uniform(0.1, 0.25), 0.3, key=2)
    elif kind == "soft":
        osc_a(p, saw=False, pulse=True, pw=r.uniform(35, 45))
        osc_b(p, saw=False, tri=True, octave=r.choice([0, 1]))
        mixer(p, 0.35, 0.7)
        filt(p, r.uniform(1500, 2500), 0.08, 0.2, key=2)
    elif kind == "brass_lead":
        osc_a(p)
        osc_b(p, saw=r.random() < 0.6, pulse=True, pw=40, fine=r.uniform(4, 8))
        mixer(p, 0.75, 0.5)
        env(p, "fenv", r.uniform(0.03, 0.08), r.uniform(0.5, 0.9), r.uniform(0.4, 0.6), 0.4)
        filt(p, r.uniform(700, 1200), r.uniform(0.1, 0.2), r.uniform(0.4, 0.55), REV1, key=2, vel=0.3)
    elif kind == "fm":
        osc_a(p, saw=False, pulse=True, pw=50)
        osc_b(p, saw=False, tri=True, semi=r.choice([12, 19, 24]))
        mixer(p, 0.8, 0.0, xmod=r.uniform(0.15, 0.4))
        route(p, WHEEL, FM, r.uniform(0.25, 0.45))
        filt(p, r.uniform(3000, 6000), 0.05, 0.15, key=2)
    elif kind == "horn":
        osc_a(p)
        osc_b(p, saw=False, pulse=True, pw=r.uniform(30, 45), fine=r.uniform(5, 9))
        mixer(p, 0.75, 0.6, drive=r.uniform(0.35, 0.6))
        env(p, "menv", 0.001, r.uniform(0.06, 0.14), 0.0, 0.1)
        route(p, MENV, F1, -pitch_amt(r.uniform(1.0, 2.5)))
        route(p, MENV, F2, -pitch_amt(r.uniform(1.0, 2.5)))
        env(p, "fenv", r.uniform(0.05, 0.12), 0.7, 0.45, 0.3)
        filt(p, r.uniform(600, 1000), r.uniform(0.15, 0.3), r.uniform(0.45, 0.6), REV1, key=2, vel=0.3)
        p["osc_model"] = 1
    elif kind == "pwm":
        osc_a(p, saw=False, pulse=True, pw=r.uniform(30, 45))
        osc_b(p, saw=False, pulse=True, pw=r.uniform(35, 45), fine=r.uniform(4, 8))
        mixer(p, 0.7, 0.55)
        p.update(lfo2_wave=TRI2, lfo2_rate=r.uniform(0.3, 1.0), lfo2_retrig=0)
        route(p, LFO2, PW1, r.uniform(0.25, 0.4))
        route(p, LFO2, PW2, -r.uniform(0.2, 0.35))
        filt(p, r.uniform(1500, 3000), 0.12, 0.25, key=2)
    elif kind == "whistle":
        osc_a(p, saw=False, pulse=True, pw=50)
        osc_b(p, saw=False, tri=True, octave=1)
        mixer(p, 0.15, 0.8, noise=0.03)
        filt(p, r.uniform(4000, 6000), 0.05, 0.1, key=2)
        p["glide"] = r.uniform(0.08, 0.15)
    else:  # fuzz
        osc_a(p)
        osc_b(p, saw=False, pulse=True, pw=40, fine=r.uniform(4, 8))
        mixer(p, 0.75, 0.4)
        filt(p, r.uniform(1800, 3000), 0.12, 0.25, key=2)
        fuzz(p, r.uniform(0.55, 0.85), r.uniform(0.35, 0.55), r.uniform(0.6, 0.9))
    if kind not in ("fm", "unison") or r.random() < 0.5:
        vibrato(p, r)
    p["hpf_cutoff"] = r.uniform(60, 140)
    return p


def a_bass(r, kind):
    p = {}
    voices(p, 1, legato=True, spread=0.0, detune=r.uniform(0.15, 0.3))
    env(p, "aenv", 0.002, 0.6, 0.85, r.uniform(0.06, 0.16))
    p["amp_velocity"] = r.uniform(0.25, 0.45)
    if kind == "sub":
        osc_a(p, saw=False, pulse=True, pw=r.uniform(40, 50))
        osc_b(p, saw=False, tri=True, octave=-1)
        mixer(p, r.uniform(0.25, 0.45), 0.3, sub=r.uniform(0.75, 1.0))
        filt(p, r.uniform(160, 320), 0.05, r.uniform(0.1, 0.25), key=1)
        env(p, "fenv", 0.001, r.uniform(0.15, 0.3), 0.1, 0.1)
    elif kind == "saw":
        osc_a(p)
        osc_b(p, octave=r.choice([-1, 0]), fine=r.uniform(3, 8))
        mixer(p, 0.85, 0.6)
        filt(p, r.uniform(200, 450), r.uniform(0.2, 0.38), r.uniform(0.4, 0.6), r.choice([REV3, REV1]), key=2, vel=r.uniform(0.2, 0.4))
        env(p, "fenv", 0.001, r.uniform(0.15, 0.35), r.uniform(0.0, 0.2), 0.1)
    elif kind == "rolling":
        osc_a(p)
        osc_b(p, octave=-1, fine=r.uniform(3, 6))
        mixer(p, 0.85, 0.55)
        voices(p, 1, legato=False, spread=0.0)
        filt(p, r.uniform(200, 320), r.uniform(0.3, 0.42), r.uniform(0.5, 0.65), r.choice([REV3, CASCADE]), key=2)
        env(p, "fenv", 0.001, r.uniform(0.12, 0.22), 0.0, 0.1)
        env(p, "aenv", 0.001, r.uniform(0.2, 0.3), 0.0, 0.08)
    elif kind == "reese":
        osc_a(p)
        osc_b(p, fine=r.choice([-1, 1]) * r.uniform(10, 18))
        voices(p, 2, mode=1, detune=r.uniform(0.6, 0.9), spread=0.0, legato=True)
        mixer(p, 0.75, 0.75, drive=r.uniform(0.4, 0.6))
        filt(p, r.uniform(300, 650), r.uniform(0.12, 0.28), 0.2, key=2)
        p.update(lfo2_wave=SINE2, lfo2_rate=r.uniform(0.1, 0.4), lfo2_retrig=0)
        route(p, LFO2, CUT, r.uniform(0.02, 0.04))
    elif kind == "acid":
        pulse = r.random() < 0.4
        osc_a(p, saw=not pulse, pulse=pulse, pw=45)
        osc_b(p)
        mixer(p, 0.85, 0.0, drive=r.uniform(0.35, 0.55))
        filt(p, r.uniform(240, 420), r.uniform(0.42, 0.5), r.uniform(0.55, 0.72), r.choice([BITE, BITE, REV3]), key=2, vel=0.5)
        env(p, "fenv", 0.001, r.uniform(0.14, 0.3), r.uniform(0.0, 0.1), 0.08)
        p.update(glide=r.uniform(0.03, 0.07), _acid=1)
    elif kind == "fm":
        osc_a(p, saw=False, pulse=True, pw=50)
        osc_b(p, saw=False, tri=True, semi=r.choice([12, 19, 24]))
        mixer(p, 0.7, 0.0, sub=r.uniform(0.3, 0.5))
        env(p, "menv", 0.001, r.uniform(0.08, 0.2), 0.0, 0.1)
        route(p, MENV, FM, r.uniform(0.5, 0.8))
        filt(p, r.uniform(900, 1600), 0.05, 0.2, key=2)
        env(p, "fenv", 0.001, 0.15, 0.0, 0.1)
        env(p, "aenv", 0.001, r.uniform(0.3, 0.5), r.uniform(0.0, 0.4), 0.1)
    elif kind == "808":
        osc_a(p)
        osc_b(p, saw=False, tri=True)
        mixer(p, 0.0, 1.0, sub=r.uniform(0.4, 0.6), drive=r.uniform(0.3, 0.55))
        env(p, "menv", 0.001, r.uniform(0.05, 0.1), 0.0, 0.1)
        route(p, MENV, F2, pitch_amt(r.uniform(5, 12)))
        filt(p, r.uniform(350, 700), 0.05, 0.1, key=1)
        env(p, "aenv", 0.001, r.uniform(1.0, 1.8), 0.0, r.uniform(0.2, 0.4))
        p["glide"] = r.uniform(0.04, 0.1)
    elif kind == "pluck":
        osc_a(p)
        osc_b(p, semi=12)
        mixer(p, 0.8, 0.35, sub=r.uniform(0.3, 0.55))
        voices(p, 1, legato=False, spread=0.0)
        filt(p, r.uniform(250, 450), r.uniform(0.15, 0.3), r.uniform(0.45, 0.6), key=2)
        env(p, "fenv", 0.001, r.uniform(0.1, 0.2), 0.0, 0.1)
        env(p, "aenv", 0.001, r.uniform(0.2, 0.4), 0.0, 0.1)
    elif kind == "pulse":
        osc_a(p, saw=False, pulse=True, pw=r.uniform(20, 30))
        osc_b(p, saw=False, pulse=True, pw=50, octave=-1)
        mixer(p, 0.75, 0.6)
        filt(p, r.uniform(350, 650), r.uniform(0.15, 0.3), r.uniform(0.3, 0.45), key=2)
        env(p, "fenv", 0.001, r.uniform(0.2, 0.4), 0.2, 0.1)
    else:  # unison
        osc_a(p)
        osc_b(p, octave=-1, fine=r.uniform(4, 8))
        voices(p, r.choice([2, 3]), unison=True, detune=r.uniform(0.2, 0.35), spread=0.0, legato=True)
        mixer(p, 0.75, 0.6)
        filt(p, r.uniform(280, 480), r.uniform(0.15, 0.3), r.uniform(0.35, 0.5), key=2)
        env(p, "fenv", 0.001, r.uniform(0.2, 0.35), 0.15, 0.1)
    p["hpf_cutoff"] = r.uniform(18, 32)
    return p


def a_brass(r, kind):
    p = {}
    voices(p, 8, detune=r.uniform(0.25, 0.45), spread=r.uniform(0.4, 0.7))
    osc_a(p)
    osc_b(p, saw=r.random() < 0.75, pulse=r.random() < 0.4, pw=40, fine=r.uniform(5, 10))
    if not p["osc2_saw"] and not p["osc2_pulse"]:
        p["osc2_saw"] = 1
    mixer(p, 0.75, 0.6)
    p["flt_velocity"] = r.uniform(0.2, 0.4)
    if kind == "section":
        env(p, "fenv", r.uniform(0.05, 0.15), r.uniform(0.5, 0.9), r.uniform(0.4, 0.6), 0.4)
        env(p, "aenv", r.uniform(0.02, 0.06), 0.8, 0.9, r.uniform(0.3, 0.5))
        filt(p, r.uniform(600, 1100), r.uniform(0.08, 0.18), r.uniform(0.4, 0.6), r.choice([REV3, REV1]), key=2)
        vibrato(p, r, cents=(5, 9), delay=(0.4, 0.9))
    elif kind == "stab":
        env(p, "fenv", 0.005, r.uniform(0.2, 0.35), r.uniform(0.15, 0.3), 0.2)
        env(p, "aenv", 0.003, 0.5, r.uniform(0.35, 0.5), r.uniform(0.15, 0.25))
        filt(p, r.uniform(700, 1300), 0.12, r.uniform(0.5, 0.65), key=2)
        p["mix_drive"] = r.uniform(0.3, 0.45)
    elif kind == "soft_horn":
        osc_b(p, saw=False, pulse=True, pw=r.uniform(35, 45), fine=r.uniform(5, 8))
        env(p, "fenv", r.uniform(0.15, 0.35), 0.9, 0.5, 0.5)
        env(p, "aenv", r.uniform(0.08, 0.2), 1.0, 0.85, r.uniform(0.4, 0.7))
        filt(p, r.uniform(450, 750), 0.1, r.uniform(0.3, 0.45), REV1, key=2)
        p["osc_model"] = 1
    elif kind == "braam":
        osc_a(p, octave=-1)
        osc_b(p, fine=r.uniform(6, 10))
        mixer(p, 0.8, 0.7, drive=r.uniform(0.45, 0.65), sub=r.uniform(0.2, 0.4))
        env(p, "fenv", r.uniform(0.3, 0.6), r.uniform(1.5, 2.5), r.uniform(0.35, 0.5), 1.2)
        env(p, "aenv", r.uniform(0.15, 0.3), 1.5, 1.0, r.uniform(1.2, 2.0))
        filt(p, r.uniform(250, 450), r.uniform(0.12, 0.25), r.uniform(0.5, 0.65), key=1)
    else:  # swell
        env(p, "fenv", r.uniform(0.8, 2.0), 1.5, r.uniform(0.5, 0.7), 1.0)
        env(p, "aenv", r.uniform(0.4, 0.9), 1.0, 0.95, r.uniform(0.8, 1.4))
        filt(p, r.uniform(500, 900), 0.12, r.uniform(0.45, 0.6), key=2)
    return p


def voice_for_arp(r, p):
    """The timbre an arpeggio plays: short, bright enough to articulate."""
    kind = r.choice(["classic", "bell", "sync", "pwm", "glass", "wood"])
    voices(p, 6, detune=r.uniform(0.15, 0.3), spread=r.uniform(0.4, 0.75))
    if kind == "bell":
        osc_a(p, saw=False, pulse=True, pw=r.uniform(12, 25))
        osc_b(p, saw=False, tri=True, semi=r.choice([12, 19]))
        mixer(p, 0.4, 0.55)
        poly_mod(p, oscb=r.uniform(0.15, 0.3))
        filt(p, r.uniform(3000, 5000), 0.05, 0.2, key=2)
    elif kind == "sync":
        osc_a(p, semi=r.choice([7, 12]), sync=True)
        osc_b(p)
        mixer(p, 0.7, 0.3)
        poly_mod(p, fenv=r.uniform(0.2, 0.4))
        filt(p, r.uniform(2200, 3500), 0.1, 0.3, key=2)
    elif kind == "pwm":
        osc_a(p, saw=False, pulse=True, pw=r.uniform(20, 35))
        osc_b(p, saw=False, pulse=True, pw=45, octave=1)
        mixer(p, 0.7, 0.35)
        filt(p, r.uniform(900, 1800), r.uniform(0.15, 0.3), r.uniform(0.4, 0.55), key=2)
    elif kind == "glass":
        osc_a(p, saw=False, pulse=True, pw=10)
        osc_b(p, saw=False, tri=True, semi=24)
        mixer(p, 0.35, 0.65)
        filt(p, r.uniform(4000, 6500), 0.05, 0.2, key=2, hpf=250)
    elif kind == "wood":
        osc_a(p, saw=False, pulse=True, pw=50)
        osc_b(p, saw=False, tri=True, semi=12)
        mixer(p, 0.6, 0.35)
        filt(p, r.uniform(800, 1300), 0.3, 0.5, MULTI, key=2, mode=BP, slope=1)
    else:
        osc_a(p)
        osc_b(p, fine=r.uniform(4, 8), octave=r.choice([0, 1]))
        mixer(p, 0.75, 0.4)
        filt(p, r.uniform(500, 1100), r.uniform(0.15, 0.32), r.uniform(0.5, 0.7), r.choice([REV3, CASCADE]), key=2)
    env(p, "fenv", 0.001, r.uniform(0.1, 0.25), r.uniform(0.0, 0.15), 0.15)
    env(p, "aenv", 0.001, r.uniform(0.2, 0.45), r.uniform(0.0, 0.2), r.uniform(0.15, 0.35))
    p["flt_velocity"] = r.uniform(0.15, 0.35)


def a_arp(r, kind):
    p = {}
    voice_for_arp(r, p)
    if kind == "up16":
        arp(p, UP, "1/16", r.choice([2, 3]), r.uniform(0.35, 0.6))
    elif kind == "updown":
        arp(p, UPDOWN, r.choice(["1/16", "1/8"]), r.choice([2, 3]), r.uniform(0.4, 0.7))
    elif kind == "random":
        arp(p, RANDOMARP, "1/16", r.choice([2, 3, 4]), r.uniform(0.25, 0.5))
    elif kind == "order":
        arp(p, ORDER, r.choice(["1/16", "1/8D"]), r.choice([1, 2]), r.uniform(0.4, 0.7), swing=r.uniform(0.0, 0.2))
    elif kind == "triplet":
        arp(p, r.choice([UP, DOWN]), "1/16T", r.choice([2, 3]), r.uniform(0.35, 0.6))
    else:  # octaves
        arp(p, UP, r.choice(["1/16", "1/8"]), 4, r.uniform(0.3, 0.5), swing=r.uniform(0.0, 0.15))
    return p


def a_seq(r, kind):
    """One held key becomes a pattern: the arpeggiator over octaves, the filter accented per step."""
    p = {}
    voices(p, 4, detune=r.uniform(0.15, 0.3), spread=r.uniform(0.2, 0.5))
    env(p, "fenv", 0.001, r.uniform(0.08, 0.2), 0.0, 0.1)
    env(p, "aenv", 0.001, r.uniform(0.15, 0.3), 0.0, 0.1)
    if kind == "bassline":
        osc_a(p)
        osc_b(p, octave=-1, fine=4)
        mixer(p, 0.8, 0.5, drive=r.uniform(0.3, 0.5))
        filt(p, r.uniform(300, 600), r.uniform(0.3, 0.45), r.uniform(0.5, 0.7), r.choice([BITE, REV3]), key=2)
        arp(p, UPDOWN, "1/16", 2, r.uniform(0.35, 0.55))
        route(p, RAND, CUT, r.uniform(0.04, 0.07))
    elif kind == "melodic":
        osc_a(p, saw=False, pulse=True, pw=r.uniform(25, 40))
        osc_b(p, saw=False, tri=True, semi=7)
        mixer(p, 0.6, 0.45)
        filt(p, r.uniform(900, 1800), 0.2, 0.45, key=2)
        arp(p, RANDOMARP, "1/16", 3, r.uniform(0.3, 0.55))
        route(p, RAND, CUT, r.uniform(0.03, 0.06))
    elif kind == "ratchet":
        osc_a(p)
        osc_b(p, fine=6)
        mixer(p, 0.75, 0.4)
        filt(p, r.uniform(500, 1000), r.uniform(0.25, 0.4), 0.55, key=2)
        arp(p, r.choice([UP, UPDOWN]), "1/32", 2, r.uniform(0.25, 0.4))
        route(p, RAND, AMP, -r.uniform(0.3, 0.5))
    elif kind == "triplet":
        osc_a(p, saw=False, pulse=True, pw=30)
        osc_b(p, octave=1)
        mixer(p, 0.7, 0.3)
        filt(p, r.uniform(700, 1400), 0.25, 0.5, key=2)
        arp(p, UP, "1/16T", r.choice([2, 3]), r.uniform(0.3, 0.5))
        route(p, RAND, CUT, r.uniform(0.03, 0.05))
    elif kind == "swing":
        osc_a(p)
        osc_b(p, saw=False, pulse=True, pw=40, semi=12)
        mixer(p, 0.7, 0.35)
        filt(p, r.uniform(600, 1200), 0.28, 0.5, r.choice([REV3, REV1]), key=2)
        arp(p, r.choice([UP, UPDOWN, ORDER]), "1/16", r.choice([2, 3]), r.uniform(0.35, 0.55), swing=r.uniform(0.2, 0.35))
        route(p, RAND, CUT, r.uniform(0.02, 0.05))
    else:  # pulse train (one octave, the accent makes the pattern)
        osc_a(p)
        osc_b(p, saw=False, tri=True, semi=r.choice([7, 12]))
        mixer(p, 0.75, 0.3)
        filt(p, r.uniform(500, 900), r.uniform(0.3, 0.42), 0.55, key=2)
        arp(p, UP, "1/16", 1, r.uniform(0.3, 0.5))
        route(p, RAND, CUT, r.uniform(0.05, 0.08))
        p.update(lfo_wave=TRI1, lfo_rate=r.uniform(0.1, 0.3), lfo_amount=1.0)
        route(p, LFO, CUT, r.uniform(0.04, 0.07))
    p["flt_velocity"] = 0.2
    return p


def a_texture(r, kind):
    p = {}
    voices(p, 8, detune=r.uniform(0.35, 0.6), spread=r.uniform(0.6, 0.95))
    env(p, "fenv", r.uniform(0.3, 1.5), 2.0, 0.6, 2.0)
    env(p, "aenv", r.uniform(0.3, 1.5), 1.5, 0.85, r.uniform(2.0, 4.0))
    if kind == "noise_wash":
        osc_a(p)
        osc_b(p, saw=False, tri=True, octave=-1)
        mixer(p, 0.25, 0.3, noise=r.uniform(0.45, 0.65))
        filt(p, r.uniform(800, 1600), r.uniform(0.2, 0.32), 0.15, MULTI, key=1, mode=BP, slope=1)
        p.update(lfo2_wave=SMOOTH2, lfo2_rate=r.uniform(0.1, 0.4), lfo2_retrig=0)
        route(p, LFO2, CUT, r.uniform(0.05, 0.09))
        phaser(p, r.uniform(0.3, 0.45), rate=r.uniform(0.05, 0.2), stages=2, fb=0.4)
    elif kind == "sh_bubbles":
        osc_a(p, saw=False, pulse=True, pw=30)
        osc_b(p, saw=False, tri=True, semi=12)
        mixer(p, 0.6, 0.5)
        filt(p, r.uniform(700, 1400), r.uniform(0.3, 0.42), 0.2, key=1)
        p.update(lfo2_wave=SH2, lfo2_rate=r.uniform(5.0, 10.0), lfo2_retrig=0)
        route(p, LFO2, CUT, r.uniform(0.08, 0.14))
        delay(p, r.choice(["1/16", "1/8T"]), r.uniform(0.3, 0.45), r.uniform(0.2, 0.3), pingpong=True)
    elif kind == "ring_metal":
        osc_a(p)
        osc_b(p, saw=False, tri=True, semi=r.choice([7, 11, 13]), fine=r.uniform(-30, 30))
        mixer(p, 0.15, 0.2, ring=r.uniform(0.5, 0.75))
        filt(p, r.uniform(2500, 4500), 0.1, 0.1, key=2, hpf=200)
        reverb(p, SPRING, 1.0, 2.5, r.uniform(0.25, 0.35))
    elif kind == "tape_dust":
        osc_a(p)
        osc_b(p, fine=r.uniform(10, 20))
        mixer(p, 0.6, 0.55, noise=0.08)
        filt(p, r.uniform(900, 1800), 0.15, 0.2, key=1)
        drift(p, r, cents=(8, 18))
        echo(p, r.choice([6, 8, 10]), r.uniform(0.25, 0.35), intensity=r.uniform(0.4, 0.55), time_ms=r.uniform(200, 420),
             wow=r.uniform(0.5, 0.8), flutter=r.uniform(0.3, 0.5), sat=0.5, age=r.uniform(0.5, 0.8), treble=-5)
        chorus(p, JUNO1, 0.35, hiss=r.uniform(0.35, 0.6))
    elif kind == "crushed":
        osc_a(p, saw=False, pulse=True, pw=r.uniform(20, 40))
        osc_b(p, saw=False, pulse=True, pw=50, semi=r.choice([7, 12]))
        mixer(p, 0.6, 0.45)
        filt(p, r.uniform(1500, 2800), 0.15, 0.25, key=1)
        drive(p, CRUSH, r.uniform(18, 30), tone=0.45, mix=r.uniform(0.2, 0.35))
    elif kind == "phase_wind":
        osc_a(p, saw=False, pulse=True, pw=20)
        osc_b(p, saw=False, tri=True, octave=-1)
        mixer(p, 0.3, 0.3, noise=r.uniform(0.4, 0.6))
        filt(p, r.uniform(1500, 3000), 0.2, 0.1, key=1)
        phaser(p, r.uniform(0.45, 0.6), rate=r.uniform(0.04, 0.12), stages=3, fb=r.uniform(0.5, 0.7))
    elif kind == "spring_drips":
        osc_a(p, saw=False, pulse=True, pw=15)
        osc_b(p, saw=False, tri=True, semi=19)
        mixer(p, 0.4, 0.5)
        filt(p, r.uniform(2000, 3500), 0.15, 0.4, key=2)
        env(p, "aenv", 0.001, r.uniform(0.2, 0.4), 0.0, 0.4)
        env(p, "fenv", 0.001, 0.15, 0.0, 0.2)
        reverb(p, SPRING, 1.2, 3.0, r.uniform(0.35, 0.45))
        echo(p, 9, 0.25, intensity=0.45, sync="1/8.", spring=0.5)
    else:  # flutter (granular-ish: fast random gates)
        osc_a(p)
        osc_b(p, saw=False, tri=True, octave=1)
        mixer(p, 0.6, 0.5)
        filt(p, r.uniform(1200, 2500), 0.15, 0.2, key=1)
        p.update(lfo2_wave=SH2, lfo2_rate=r.uniform(8, 16), lfo2_retrig=0)
        route(p, LFO2, AMP, -r.uniform(0.5, 0.8))
        route(p, LFO2, CUT, r.uniform(0.05, 0.1))
        delay(p, "1/16", 0.4, 0.25, pingpong=True)
    if "fx_reverb_on" not in p:
        reverb(p, r.choice([HALL, SHIMMER, PLATE]), r.uniform(1.2, 1.8), r.uniform(4.0, 8.0), r.uniform(0.3, 0.4))
    return p


def a_drone(r, kind):
    p = {}
    voices(p, 6, detune=r.uniform(0.35, 0.6), spread=r.uniform(0.6, 0.9))
    env(p, "fenv", r.uniform(1.5, 4.0), 3.0, 0.7, 3.0)
    env(p, "aenv", r.uniform(1.2, 3.0), 2.0, 1.0, r.uniform(3.0, 5.0))
    if kind == "fifth_drone":
        osc_a(p)
        osc_b(p, semi=7, fine=r.uniform(2, 5))
        mixer(p, 0.7, 0.55)
        filt(p, r.uniform(600, 1300), r.uniform(0.15, 0.3), 0.2, key=1)
        sway(p, r, octaves=(0.3, 0.6))
    elif kind == "dark_drone":
        osc_a(p)
        osc_b(p, octave=-1, fine=r.uniform(5, 9))
        mixer(p, 0.7, 0.6, sub=r.uniform(0.3, 0.5), drive=r.uniform(0.3, 0.5))
        filt(p, r.uniform(250, 550), r.uniform(0.2, 0.32), 0.25, REV1, key=1)
        p.update(osc_model=1, lfo2_wave=SMOOTH2, lfo2_rate=r.uniform(0.05, 0.15), lfo2_retrig=0)
        route(p, LFO2, CUT, r.uniform(0.04, 0.07))
    elif kind == "beating":
        osc_a(p, saw=r.random() < 0.6, pulse=True, pw=45)
        osc_b(p, saw=False, tri=True, fine=r.uniform(2, 5))
        mixer(p, 0.6, 0.6)
        filt(p, r.uniform(1200, 2400), 0.1, 0.15, key=1)
        chorus(p, ENS, r.uniform(0.35, 0.5), rate=0.5)
    elif kind == "shimmer_drone":
        osc_a(p, saw=False, pulse=True, pw=r.uniform(15, 30))
        osc_b(p, saw=False, tri=True, octave=1)
        mixer(p, 0.45, 0.55)
        filt(p, r.uniform(2000, 4000), 0.08, 0.1, key=1, hpf=150)
        reverb(p, SHIMMER, 1.8, r.uniform(7.0, 11.0), r.uniform(0.38, 0.48), shimmer=r.uniform(0.4, 0.6), pitch=r.choice([3, 4]))
    else:  # noise_drone
        osc_a(p)
        osc_b(p, saw=False, tri=True, octave=-1)
        mixer(p, 0.4, 0.4, noise=r.uniform(0.3, 0.5))
        filt(p, r.uniform(500, 1200), r.uniform(0.25, 0.38), 0.15, MULTI, key=1, mode=BP, slope=1)
        sway(p, r, octaves=(0.5, 1.0), rate=(0.03, 0.1))
        phaser(p, 0.4, rate=0.05, stages=2, fb=0.5)
    drift(p, r)
    if "fx_reverb_on" not in p:
        reverb(p, HALL, r.uniform(1.4, 2.0), r.uniform(6.0, 10.0), r.uniform(0.32, 0.42), predelay=40)
    return p


def a_fx(r, kind):
    p = {}
    voices(p, 4, detune=r.uniform(0.3, 0.5), spread=r.uniform(0.5, 0.9))
    env(p, "aenv", 0.005, 2.0, 1.0, r.uniform(1.0, 2.5))
    env(p, "fenv", 0.005, 1.0, 0.6, 1.0)
    if kind == "riser":
        osc_a(p)
        osc_b(p, fine=10)
        mixer(p, 0.5, 0.5, noise=r.uniform(0.25, 0.4))
        env(p, "menv", r.uniform(2.5, 3.0), 1.0, 1.0, 1.0)
        route(p, MENV, F1, pitch_amt(r.uniform(10, 19)))
        route(p, MENV, F2, pitch_amt(r.uniform(10, 19)))
        route(p, MENV, CUT, r.uniform(0.3, 0.5))
        filt(p, r.uniform(400, 900), r.uniform(0.2, 0.4), 0.0, key=1)
    elif kind == "fall":
        osc_a(p)
        osc_b(p, saw=False, pulse=True, pw=40, fine=8)
        mixer(p, 0.6, 0.5)
        env(p, "menv", 0.001, r.uniform(2.0, 3.0), 0.0, 1.0)
        route(p, MENV, F1, pitch_amt(r.uniform(12, 24)))
        route(p, MENV, F2, pitch_amt(r.uniform(12, 24)))
        filt(p, r.uniform(1500, 3000), 0.2, 0.2, key=2)
    elif kind == "zap":
        osc_a(p, saw=False, pulse=True, pw=30)
        osc_b(p, saw=False, tri=True)
        mixer(p, 0.7, 0.5)
        env(p, "menv", 0.001, r.uniform(0.08, 0.2), 0.0, 0.1)
        route(p, MENV, F1, pitch_amt(r.uniform(18, 24)))
        route(p, MENV, F2, pitch_amt(r.uniform(18, 24)))
        env(p, "aenv", 0.001, r.uniform(0.25, 0.4), 0.0, 0.2)
        filt(p, r.uniform(3000, 5000), 0.15, 0.1, key=2)
        delay(p, "1/8D", 0.45, 0.3, pingpong=True)
    elif kind == "laser":
        osc_a(p, semi=12, sync=True)
        osc_b(p)
        mixer(p, 0.75, 0.2)
        env(p, "menv", 0.001, r.uniform(0.3, 0.6), 0.0, 0.2)
        route(p, MENV, F1, pitch_amt(r.uniform(14, 24)))
        env(p, "aenv", 0.001, r.uniform(0.4, 0.7), 0.0, 0.4)
        filt(p, r.uniform(3000, 5000), 0.15, 0.1, key=2)
        echo(p, r.choice([3, 6]), 0.3, intensity=0.55, sync="1/8.")
    elif kind == "sweep":
        osc_a(p)
        osc_b(p)
        mixer(p, 0.2, 0.2, noise=1.0)
        env(p, "menv", r.uniform(1.5, 3.0), 1.0, 1.0, 1.0)
        route(p, MENV, CUT, r.uniform(0.5, 0.75))
        filt(p, r.uniform(250, 500), r.uniform(0.35, 0.55), 0.0, MULTI, key=0, mode=BP, slope=1)
    elif kind == "alarm":
        osc_a(p, saw=False, pulse=True, pw=50)
        osc_b(p, saw=False, tri=True, semi=12)
        mixer(p, 0.6, 0.4)
        p.update(lfo_wave=SQR1, lfo_rate=r.uniform(4.0, 8.0), lfo_amount=1.0, lfo_delay=0)
        route(p, LFO, F1, pitch_amt(r.uniform(3, 7)))
        route(p, LFO, F2, pitch_amt(r.uniform(3, 7)))
        filt(p, r.uniform(2000, 3500), 0.2, 0.1, key=2)
    elif kind == "impact":
        osc_a(p, octave=-2)
        osc_b(p, octave=-1, fine=12)
        mixer(p, 0.6, 0.6, noise=0.6, drive=0.55)
        env(p, "aenv", 0.001, r.uniform(1.0, 1.8), 0.0, 1.0)
        env(p, "fenv", 0.001, r.uniform(0.6, 1.2), 0.0, 0.5)
        filt(p, r.uniform(600, 1200), 0.2, 0.6, key=1)
        reverb(p, HALL, 2.0, r.uniform(6.0, 9.0), 0.4)
    else:  # sci
        osc_a(p, saw=False, pulse=True, pw=25)
        osc_b(p, saw=False, tri=True, semi=r.choice([5, 7, 11]))
        mixer(p, 0.3, 0.2, ring=0.6)
        p.update(lfo2_wave=SH2, lfo2_rate=r.uniform(6, 12), lfo2_retrig=0)
        route(p, LFO2, F2, pitch_amt(r.uniform(5, 12)))
        filt(p, r.uniform(2000, 4000), 0.2, 0.1, key=2)
        delay(p, "1/16", 0.45, 0.3, pingpong=True)
    if "fx_reverb_on" not in p:
        reverb(p, r.choice([HALL, SHIMMER, PLATE]), r.uniform(1.4, 2.0), r.uniform(4.0, 8.0), r.uniform(0.3, 0.4))
    return p


ARCHETYPES = {
    "PAD": (a_pad, ["strings", "pwm", "fifths", "glass", "breath", "sync_swell", "dark", "choir", "polymod", "airy"]),
    "KEYS": (a_keys, ["epiano", "organ", "felt", "clav", "bell", "poly_keys", "vibes", "harpsi"]),
    "PLUCK": (a_pluck, ["classic", "sync", "kalimba", "harp", "polymod", "noise", "glass", "wood"]),
    "LEAD": (a_lead, ["sync", "unison", "soft", "brass_lead", "fm", "horn", "pwm", "whistle", "fuzz"]),
    "BASS": (a_bass, ["sub", "saw", "rolling", "reese", "acid", "fm", "808", "pluck", "pulse", "unison"]),
    "BRASS": (a_brass, ["section", "stab", "soft_horn", "braam", "swell"]),
    "ARP": (a_arp, ["up16", "updown", "random", "order", "triplet", "octaves"]),
    "SEQ": (a_seq, ["bassline", "melodic", "ratchet", "triplet", "swing", "pulse"]),
    "TEXTURE": (a_texture, ["noise_wash", "sh_bubbles", "ring_metal", "tape_dust", "crushed", "phase_wind", "spring_drips", "flutter"]),
    "DRONE": (a_drone, ["fifth_drone", "dark_drone", "beating", "shimmer_drone", "noise_drone"]),
    "FX": (a_fx, ["riser", "fall", "zap", "laser", "sweep", "alarm", "impact", "sci"]),
}
FAMILY_FAVOURITES = {
    "IDM": {"tape_dust", "crushed", "sh_bubbles", "flutter", "pwm", "fm", "polymod", "random", "ratchet", "sci", "noise"},
    "NEO": {"felt", "epiano", "vibes", "glass", "soft", "breath", "kalimba", "harp", "beating", "airy"},
    "CINE": {"strings", "braam", "section", "swell", "dark", "choir", "shimmer_drone", "riser", "impact", "brass_lead", "fifths"},
    "TECHNO": {"rolling", "sync", "unison", "acid", "classic", "reese", "up16", "bassline", "zap", "laser", "pulse"},
    "HOUSE": {"organ", "stab", "poly_keys", "epiano", "sub", "saw", "section", "choir", "swing", "order", "pluck"},
    "COLOR": {"bell", "glass", "kalimba", "shimmer_drone", "fifths", "airy", "vibes", "fm", "octaves", "triplet", "spring_drips"},
    "HYPNO": {"pwm", "dark", "beating", "sh_bubbles", "phase_wind", "polymod", "rolling", "whistle", "melodic", "noise_drone", "updown"},
}


# ============================================================================================== style families
def family_traits(p, family, cat, r):
    bassy = cat == "BASS" or (cat == "SEQ" and p.get("flt_cutoff", 1000) < 650)
    spacious = cat in ("PAD", "TEXTURE", "DRONE", "FX")
    if family == "IDM":
        p["analog_age"] = r.uniform(0.5, 0.75)
        p["voice_detune"] = min(1.0, p.get("voice_detune", 0.3) + 0.15)
        if not bassy:
            if r.random() < 0.6 and "fx_echo_on" not in p:
                echo(p, r.choice([0, 1, 3, 5]), r.uniform(0.18, 0.28), intensity=r.uniform(0.35, 0.5), time_ms=r.uniform(150, 380),
                     wow=r.uniform(0.4, 0.7), flutter=r.uniform(0.3, 0.5), age=r.uniform(0.5, 0.8), treble=-4)
            if r.random() < 0.5 and "fx_chorus_on" not in p:
                chorus(p, r.choice([JUNO1, JUNO2]), r.uniform(0.25, 0.4), hiss=r.uniform(0.3, 0.5))
        if r.random() < 0.3:
            drive(p, CRUSH, r.uniform(14, 26), tone=0.45, mix=r.uniform(0.12, 0.22))
        if r.random() < 0.35:
            p["_order"] = [1, 2, 3, 4, 5, 0, 6, 7]  # the crusher after the echo: the repeats get dirtier
        if cat in ("PAD", "KEYS", "PLUCK", "TEXTURE") and r.random() < 0.4:
            drift(p, r, cents=(6, 14))
    elif family == "NEO":
        p["analog_age"] = r.uniform(0.25, 0.4)
        p["flt_cutoff"] = p.get("flt_cutoff", 2000) * r.uniform(0.6, 0.8)
        p["flt_reso"] = min(p.get("flt_reso", 0.1), 0.22)
        p["flt_velocity"] = max(p.get("flt_velocity", 0.0), r.uniform(0.35, 0.55))
        if not bassy:
            reverb(p, PLATE, r.uniform(0.9, 1.3), r.uniform(1.8, 3.2), r.uniform(0.2, 0.3), damp=0.6)
            if r.random() < 0.35 and "fx_echo_on" not in p:
                echo(p, r.choice([1, 2]), r.uniform(0.12, 0.2), intensity=0.3, time_ms=r.uniform(260, 420), wow=0.35, flutter=0.15,
                     treble=-5, sat=0.25)
    elif family == "CINE":
        if spacious or cat in ("BRASS", "KEYS"):
            if r.random() < 0.6:
                reverb(p, HALL, r.uniform(1.5, 2.0), r.uniform(6.0, 12.0), r.uniform(0.3, 0.42), predelay=r.uniform(30, 70))
            else:
                reverb(p, SHIMMER, r.uniform(1.4, 2.0), r.uniform(6.0, 10.0), r.uniform(0.3, 0.4), shimmer=r.uniform(0.3, 0.5),
                       pitch=r.choice([3, 4]))
        elif not bassy:
            reverb(p, HALL, r.uniform(1.2, 1.6), r.uniform(3.0, 5.0), r.uniform(0.2, 0.3))
        if cat in ("PAD", "BRASS") and r.random() < 0.5:
            p["aenv_a"] = max(p.get("aenv_a", 0.01), r.uniform(0.6, 1.8))
        if r.random() < 0.3:
            comp(p, threshold=r.uniform(-18, -12), ratio=1, makeup=2.0, mix=0.7)
    elif family == "TECHNO":
        if not bassy:
            if r.random() < 0.7 and "delay_on" not in p and "fx_echo_on" not in p:
                if r.random() < 0.5:
                    delay(p, r.choice(["1/8D", "1/4D", "1/8D"]), r.uniform(0.3, 0.45), r.uniform(0.18, 0.28), pingpong=r.random() < 0.6)
                else:
                    echo(p, r.choice([0, 3, 5]), r.uniform(0.18, 0.28), intensity=r.uniform(0.4, 0.55), sync=r.choice(["1/8.", "1/4."]),
                         wow=0.15, flutter=0.1, treble=-3)
            if "fx_reverb_on" not in p or cat in ("LEAD", "PLUCK", "ARP"):
                reverb(p, r.choice([ROOM, HALL]), r.uniform(1.0, 1.5), r.uniform(1.8, 3.5), r.uniform(0.15, 0.25))
        if r.random() < 0.5:
            comp(p, threshold=r.uniform(-20, -14), ratio=r.choice([1, 2, 3]), attack=r.choice([2, 3, 4]), makeup=3.0)
        if r.random() < 0.3:
            drive(p, TUBE, r.uniform(8, 16), tone=0.55, mix=r.uniform(0.4, 0.7))
    elif family == "HOUSE":
        if not bassy:
            if "fx_chorus_on" not in p and r.random() < 0.7:
                chorus(p, r.choice([JUNO1, JUNO2, JUNO12]), r.uniform(0.3, 0.45))
            reverb(p, r.choice([PLATE, ROOM]), r.uniform(0.9, 1.3), r.uniform(1.2, 2.5), r.uniform(0.15, 0.25))
            if r.random() < 0.3 and "delay_on" not in p:
                delay(p, "1/8D", r.uniform(0.25, 0.35), r.uniform(0.12, 0.2), pingpong=True)
        if r.random() < 0.4:
            comp(p, threshold=r.uniform(-16, -10), ratio=1, makeup=2.0)
    elif family == "COLOR":
        if not bassy:
            if r.random() < 0.5:
                reverb(p, SHIMMER, r.uniform(1.3, 1.8), r.uniform(4.0, 8.0), r.uniform(0.28, 0.38), shimmer=r.uniform(0.35, 0.6),
                       pitch=r.choice([3, 3, 4, 2]))
            else:
                reverb(p, HALL, r.uniform(1.3, 1.8), r.uniform(4.0, 7.0), r.uniform(0.25, 0.35))
            if "fx_chorus_on" not in p and r.random() < 0.6:
                chorus(p, r.choice([DIM, ENS]), r.uniform(0.3, 0.45))
            if r.random() < 0.45 and "delay_on" not in p:
                delay(p, r.choice(["1/4D", "1/8D"]), r.uniform(0.3, 0.45), r.uniform(0.15, 0.25), pingpong=True)
            if r.random() < 0.15:
                flanger(p, r.uniform(0.25, 0.4), rate=r.uniform(0.05, 0.2), fb=r.uniform(0.3, 0.6))
    else:  # HYPNO
        if cat not in ("BASS",):
            drift(p, r, cents=(4, 10))
        if r.random() < 0.6 and "lfo_rate" not in p:
            sway(p, r)
        if not bassy:
            if r.random() < 0.45 and "fx_phaser_on" not in p:
                phaser(p, r.uniform(0.3, 0.5), rate=r.uniform(0.05, 0.3), stages=r.choice([1, 2]), fb=r.uniform(0.3, 0.6))
            if "fx_echo_on" not in p and r.random() < 0.6:
                echo(p, r.choice([4, 5, 8, 9]), r.uniform(0.2, 0.3), intensity=r.uniform(0.4, 0.55), time_ms=r.uniform(220, 480),
                     wow=r.uniform(0.35, 0.6), sat=r.uniform(0.45, 0.65), spring=r.uniform(0.3, 0.5))
            if "fx_reverb_on" not in p:
                reverb(p, r.choice([SPRING, HALL]), r.uniform(1.0, 1.5), r.uniform(2.5, 5.0), r.uniform(0.2, 0.3))
    # Basses: mono, dry; fuzz only in parallel.
    if cat == "BASS":
        mono_low_end(p)
    # Resonance stays moderate everywhere (acid basses up to 0.5; FX may ping).
    if cat != "FX":
        p["flt_reso"] = min(p.get("flt_reso", 0.1), 0.5 if p.get("_acid") else 0.45)
    return p


# ============================================================================================== names
ADJ = {
    "IDM": ["Static", "Fractured", "Pixel", "Cyclic", "Bent", "Melted", "Wired", "Mercury", "Crooked", "Chalk", "Tin", "Velour",
            "Folded", "Circuit", "Granular", "Glitched", "Inverted", "Polygon", "Phosphor", "Rusted", "Sideways", "Hollow", "Liquid",
            "Neon Moss", "Wobbly", "Sleepless", "Cobalt", "Feral", "Quartz", "Sputtering"],
    "NEO": ["Felt", "Linen", "Paper", "Candle", "Porcelain", "Winter", "Ivory", "Tender", "Hushed", "Fragile", "Pale", "Gentle",
            "Snow", "Wool", "Cedar", "Unspoken", "Morning", "Woven", "Lantern", "Patient", "Faded", "Quiet", "Sleeping", "Breathing",
            "Slow", "Silken", "Hazel", "Dawnlit", "Velvet", "Waning"],
    "CINE": ["Vast", "Monumental", "Imperial", "Storm", "Obsidian", "Endless", "Ascending", "Colossal", "Ancient", "Fallen",
             "Eternal", "Gravity", "Thunder", "Burning", "Celestial", "Frozen", "Ember", "Last", "Sovereign", "Abyssal", "Solemn",
             "Rising", "Iron", "Titan", "Somber", "Distant", "Mythic", "Shadowed", "Grand", "Votive"],
    "TECHNO": ["Chrome", "Prism", "Pulse", "Laser", "Sunrise", "Mirror", "Hyper", "Midnight", "Signal", "Vector", "Strobe",
               "Afterhours", "Quantum", "Orbital", "Hypnotic", "Electric", "Infinite", "Arc", "Lumen", "Polar", "Solar", "Plasma",
               "Dark", "Silver", "Concrete", "Warehouse", "Tunnel", "Kinetic", "Radiant", "Steel"],
    "HOUSE": ["Deep", "Soul", "Golden", "Warm", "Smoky", "Dusk", "Copper", "Honey", "Sunday", "Late", "Basement", "Silk", "Cocoa",
              "Brass", "Garage", "Rooftop", "Disco", "Mellow", "Blue", "Amber", "Groove", "Lounge", "Night", "Vintage", "Bronze",
              "Swing", "Sapphire", "Caramel", "Saturday", "Balearic"],
    "COLOR": ["Chroma", "Iris", "Opal", "Pastel", "Magenta", "Turquoise", "Saffron", "Lilac", "Coral", "Indigo", "Violet",
              "Aurora", "Crystal", "Pearl", "Peach", "Mint", "Lavender", "Azure", "Rose", "Citrine", "Iridescent", "Petal", "Glass",
              "Prismatic", "Halcyon", "Gilded", "Sunlit", "Rainlit", "Tinted", "Bright"],
    "HYPNO": ["Swaying", "Pastoral", "Meadow", "Wildflower", "Drunken", "Rolling", "Moth", "Lark", "Bramble", "Fern", "Clover",
              "Orchard", "Barley", "Heather", "Thistle", "Dappled", "Lazy", "Mossy", "Twilight", "Blurred", "Hazy", "Wandering",
              "Dizzy", "Sleepy", "Burnt", "Murmuring", "Tidal", "Spiral", "Drifting", "Dreaming"],
}
NOUN = {
    "PAD": ["Veil", "Horizon", "Tide", "Cathedral", "Bloom", "Haze", "Halo", "Mist", "Field", "Garden", "Shore", "Canopy", "Cloud",
            "Vapor", "Lullaby", "Glow", "Nebula", "Reverie", "Dream", "Memory", "Sanctuary", "Hymn", "Lagoon", "Sky", "Breath",
            "Embrace", "Dawn", "Augury", "Templum", "Firmament"],
    "KEYS": ["Keys", "Piano", "Hammers", "Ivories", "Chimes", "Organ", "Tines", "Etude", "Nocturne", "Prelude", "Letters", "Diary",
             "Parlour", "Study", "Waltz", "Chords", "Upright", "Clavier", "Celesta", "Hymnal", "Chapel", "Rhodes Room"],
    "PLUCK": ["Pluck", "Harp", "Drops", "Kalimba", "Strings", "Petals", "Rain", "Seeds", "Sparks", "Pebbles", "Ripples", "Beads",
              "Dew", "Glints", "Raindrops", "Marbles", "Plinks", "Taps", "Feathers", "Crumbs", "Sparrows", "Wrens"],
    "LEAD": ["Lead", "Voice", "Signal", "Siren", "Flare", "Comet", "Arrow", "Ray", "Song", "Call", "Thread", "Line", "Beacon",
             "Kite", "Swan", "Falcon", "Herald", "Whisper", "Swift", "Heron", "Oracle", "Cry"],
    "BASS": ["Bass", "Sub", "Floor", "Undertow", "Engine", "Root", "Ground", "Depth", "Roller", "Rumble", "Current", "Bedrock",
             "Pressure", "Keel", "Anchor", "Cellar", "Gravity", "Foundation", "Basin", "Low Sun"],
    "BRASS": ["Horns", "Brass", "Fanfare", "Herald", "Anthem", "Crown", "Monolith", "Procession", "Section", "Trumpets", "March",
              "Banners", "Sentinels", "Proclamation", "Augurs", "Legion"],
    "ARP": ["Arp", "Cascade", "Spiral", "Carousel", "Clockwork", "Wheel", "Ladder", "Lattice", "Loop", "Gears", "Fireflies",
            "Staircase", "Whirl", "Constellation", "Fountain", "Kaleidoscope", "Swarm", "Flock", "Murmuration", "Starlings"],
    "SEQ": ["Sequence", "Machine", "Motor", "Ritual", "Code", "Circuit", "Mantra", "Cycle", "Pattern", "Ostinato", "Pulse",
            "Rhythm", "Heartbeat", "Orbit", "Algorithm", "Turbine", "Tram", "Omen", "Rite", "Auspice"],
    "TEXTURE": ["Texture", "Dust", "Static", "Grain", "Murmur", "Weather", "Hiss", "Particles", "Rust", "Lichen", "Tape", "Fog",
                "Frost", "Moss", "Embers", "Smoke", "Radio", "Snowfall", "Sediment", "Feathers", "Wingbeats"],
    "DRONE": ["Drone", "Monolith", "Abyss", "Hum", "Void", "Continuum", "Expanse", "Undertone", "Eclipse", "Tundra", "Desert",
              "Glacier", "Mantle", "Firmament", "Stillness", "Solstice", "Meridian", "Zenith"],
    "FX": ["Riser", "Impact", "Sweep", "Transmission", "Fall", "Launch", "Rift", "Collapse", "Flare", "Wind", "Signal", "Blips",
           "Zap", "Descent", "Liftoff", "Shockwave", "Portent", "Sign"],
}
OF = ["Salt", "Glass", "Light", "Dust", "Rain", "Stars", "Embers", "Smoke", "Ash", "Suns", "Water", "Snow", "Silver", "Silence",
      "Echoes", "Tides", "Clouds", "Bells", "Wires", "Paper", "Gold", "Iron", "Mirrors", "Feathers", "Swallows", "Omens", "Wings",
      "Pines", "Saffron", "Larks"]
NEUTRAL = ["Atlas", "Lantern", "Harbor", "Mirror", "Voyage", "Letter", "Window", "River", "Island", "Mosaic", "Satellite",
           "Labyrinth", "Tapestry", "Compass", "Fable", "Sonnet", "Postcard", "Archive", "Observatory", "Pilgrim", "Monsoon",
           "Lighthouse", "Polaroid", "Telegram", "Origami", "Kingdom", "Requiem", "Quarry", "Aviary", "Sundial"]
LABELS = {"Pad", "Keys", "Pluck", "Lead", "Bass", "Brass", "Arp", "Sequence", "Texture", "Drone", "Riser", "Sub", "Voice", "Piano",
          "Organ", "Harp", "Horns", "Strings"}
FORBIDDEN = re.compile(r"prophet|sequential|moog|roland|juno|jupiter|oberheim|korg|yamaha|odyssey|dimension|space echo|rhodes", re.I)


def existing_names():
    names = set()
    for f in (ROOT / "plugin" / "Presets.cpp", ROOT / "plugin" / "PresetsExpansion.inc"):
        names |= {m.lower() for m in re.findall(r'\{ "([^"]+)", \{', f.read_text(encoding="utf-8"))}
    for f in (ROOT / "packs").glob("AUGUR-5 Anthology*/**/*.augur5"):
        names.add(f.stem[3:].lower())
    return names


def name_for(r, cat, family, used):
    for _ in range(600):
        form = r.random()
        adj = r.choice(ADJ[family])
        noun = r.choice(NOUN[cat])
        if form < 0.7:
            n = "%s %s" % (adj, noun)
        elif form < 0.86:
            if noun in LABELS:
                continue
            n = "%s of %s" % (noun, r.choice(OF))
        else:
            n = "%s %s" % (adj, r.choice(NEUTRAL))
        words = n.lower().replace(" of ", " ").split()
        if len(set(words)) != len(words) or len(n) > 24 or n.lower() in used or FORBIDDEN.search(n):
            continue
        used.add(n.lower())
        return n
    raise RuntimeError("names exhausted")


# ============================================================================================== candidates
def design(seed, per_slot):
    r = random.Random(seed)
    cands = []
    for cat, fams in slots().items():
        fn, kinds = ARCHETYPES[cat]
        for family, count in fams.items():
            fav = [k for k in kinds if k in FAMILY_FAVOURITES[family]]
            pool = kinds + fav  # favourites twice as likely
            for i in range(count * per_slot):
                kind = kinds[i % len(kinds)] if i < len(kinds) else pool[r.randrange(len(pool))]
                p = base(r, cat)
                p.update(fn(r, kind))
                p = family_traits(p, family, cat, r)
                cands.append({"cat": cat, "family": family, "kind": kind, "p": p, "id": len(cands)})
    return cands


def xml_for(c, name=None):
    p = finish(c["p"])
    order = c["p"].get("_order")
    attrs = 'formatVersion="1" name=%s' % quoteattr(name or c.get("name", "%04d" % c["id"]))
    if order:
        attrs += ' fxOrder="%s"' % " ".join(str(x) for x in order)
    lines = ['<?xml version="1.0" encoding="UTF-8"?>',
             "<AUGUR5_PRESET %s>" % attrs,
             '  <META role="%s" note="%d" pack=%s category="%s" family="%s" kind="%s" author="TONAL LAB"/>'
             % (ROLE[c["cat"]], NOTE[c["cat"]], quoteattr(PACK), c["cat"], c["family"], c["kind"]),
             "  <PARAMS>"]
    for k in sorted(p):
        v = p[k]
        lines.append('    <P id="%s" value="%s"/>' % (k, ("%d" % v) if float(v).is_integer() else ("%.5g" % v)))
    lines.append("  </PARAMS>")
    lines.append("</AUGUR5_PRESET>")
    return "\n".join(lines) + "\n"


# ============================================================================================== audit
def audit_dir(args):
    folder, dry, reverse = args
    env_ = dict(os.environ)
    if reverse:
        env_["AUGUR_AUDIT_REVERSE"] = "1"
    out = subprocess.run([str(AUDIT), "--pack", str(folder)] + (["--dry"] if dry else []), capture_output=True, text=True,
                         cwd=str(ROOT), env=env_).stdout
    rows = {}
    for row in csv.DictReader(io.StringIO(out), delimiter=";"):
        m = {k: (float(v) if k not in ("path", "role", "fp", "fpdry") else v) for k, v in row.items()}
        m["fpdry"] = timbre.parse(row.get("fpdry", ""))
        m["fp"] = timbre.parse(row.get("fp", ""))
        rows[(folder / row["path"]).resolve()] = m
    return rows


def audit(dirs, dry=False, reverse=False):
    rows = {}
    with concurrent.futures.ThreadPoolExecutor(max_workers=JOBS) as ex:
        for part in ex.map(audit_dir, [(d, dry, reverse) for d in dirs]):
            rows.update(part)
    return rows


# ============================================================================================== gates, fixes, level
def gates(c, m):
    fails = []
    if m["finite"] < 1:
        fails.append("non-finite")
    if m["loud"] < -45:
        fails.append("silent")
    if abs(m["dc"]) > 0.01:
        fails.append("dc")
    if m["high"] > 0.125:
        fails.append("bright")
    if m["peak"] > -0.3:
        fails.append("clip")
    if c["cat"] == "BASS":
        if m["width"] > 0.22:
            fails.append("bass-wide")
        if m["low"] < 0.12:
            fails.append("bass-thin")
        if m["centroid"] > 1600:
            fails.append("bass-bright")
    if m["tail"] > 14.0:
        fails.append("endless")
    if TARGET_LOUD - m["loud"] > SPIKY_DB and PEAK_MAX - m["peak"] < 0.5:
        fails.append("spiky")  # held back by its transient peak, not by its level control
    return fails


def scale(p, key, f, default):
    p[key] = p.get(key, default) * f


def fix(c, fails):
    p = c["p"]
    for f in fails:
        if f in ("bright", "bass-bright"):
            scale(p, "flt_cutoff", 0.75, 2500)
            p["mix_drive"] = p.get("mix_drive", 0.2) * 0.7
            if p.get("fx_reverb_on"):
                p["fx_reverb_damp"] = min(1.0, p.get("fx_reverb_damp", 0.45) + 0.2)
                scale(p, "fx_reverb_shimmer", 0.6, 0.4)
            if p.get("fx_drive_on"):
                scale(p, "fx_drive_amount", 0.7, 12)
                scale(p, "fx_drive_tone", 0.8, 0.6)
            if p.get("fuzz_on"):
                scale(p, "fuzz_tone", 0.7, 0.5)
            if p.get("fx_echo_on"):
                p["fx_echo_treble"] = p.get("fx_echo_treble", 0.0) - 3.0
            if p.get("mix_noise", 0) > 0.1:
                scale(p, "mix_noise", 0.7, 0.0)
        elif f == "bass-wide":
            mono_low_end(p)
            p["voice_detune"] = min(p.get("voice_detune", 0.3), 0.2)
            if p.get("voice_mode") == 1:
                p["voice_mode"] = 0
        elif f == "bass-thin":
            p["mix_sub"] = min(1.0, p.get("mix_sub", 0.0) + 0.25)
            scale(p, "flt_cutoff", 0.85, 400)
            p["hpf_cutoff"] = min(p.get("hpf_cutoff", 10), 20)
        elif f == "endless":
            scale(p, "aenv_r", 0.5, 0.4)
            if p.get("fx_reverb_on"):
                scale(p, "fx_reverb_decay", 0.6, 2.5)
            if p.get("fx_echo_on"):
                scale(p, "fx_echo_intensity", 0.75, 0.45)
            if p.get("delay_on"):
                scale(p, "delay_fb", 0.75, 0.35)
        elif f == "silent":
            scale(p, "flt_cutoff", 2.0, 1000)
            for k in ("mix_osc1", "mix_osc2"):
                p[k] = min(1.0, p.get(k, 0.5) + 0.25)
        elif f == "dc":
            p["hpf_cutoff"] = max(p.get("hpf_cutoff", 10), 25)
        elif f == "spiky":
            # The first few milliseconds of a percussive note peak 20 dB over its body: a fast bus compressor takes
            # the spike, a softer attack keeps it from forming; next time round, tape drive rounds the peaks off.
            p["aenv_a"] = max(p.get("aenv_a", 0.003), 0.0025)
            if not p.get("fx_comp_on"):
                comp(p, threshold=-22.0, ratio=5, attack=0, release=1, makeup=4.0, mix=1.0, schpf=40.0)
            elif not p.get("_tamed"):
                p["fx_comp_threshold"] = p.get("fx_comp_threshold", -14.0) - 6.0
                p["fx_comp_attack"] = 0
                p["fx_comp_ratio"] = max(p.get("fx_comp_ratio", 1), 4)
            elif not p.get("fx_drive_on"):
                drive(p, TAPE, 9.0, tone=0.55, mix=1.0)
            p["_tamed"] = 1


def boost_quiet(p):
    """The level control ran out of gain: take back what the effects cost in loudness, then raise the mixer."""
    if p.get("fx_comp_on"):
        p["fx_comp_makeup"] = p.get("fx_comp_makeup", 0.0) + 3.0
    for k in ("fx_reverb_mix", "fx_echo_mix", "delay_mix", "fx_phaser_mix", "fx_flanger_mix"):
        if k in p:
            scale(p, k, 0.85, 0.3)
    for k in ("mix_osc1", "mix_osc2", "mix_sub", "mix_noise", "mix_ring"):
        if p.get(k, 0) > 0:
            p[k] = min(1.0, p[k] * 1.35)
    if p.get("flt_reso", 0) > 0.35:
        p["flt_reso"] -= 0.06


def level(c, m):
    """Set the preset's LEVEL so it plays at the target (peaks <= -1 dBFS). True when LEVEL ran out of range."""
    delta = min(TARGET_LOUD - m["loud"], PEAK_MAX - m["peak"])
    want = m["level"] + delta
    c["p"]["amp_level"] = max(-30.0, min(6.0, want))
    return want > 6.0


# ============================================================================================== selection
def farthest_points(group, k, already=()):
    """Farthest-point sampling on the dry timbre fingerprint: start from the most typical sound (or from what is already
    chosen), then always add the candidate farthest from everything chosen."""
    if k <= 0 or not group:
        return []
    if len(group) <= k:
        return list(group)
    fps = [g["m"]["fpdry"] for g in group]
    n = len(group)
    if already:
        best = [min(timbre.distance(fps[i], a["m"]["fpdry"]) for a in already) for i in range(n)]
        chosen = []
    else:
        centre = [sum(f[i] for f in fps) / n for i in range(len(fps[0]))]
        first = min(range(n), key=lambda i: timbre.distance(fps[i], centre))
        chosen = [first]
        best = [timbre.distance(fps[i], fps[first]) for i in range(n)]
    while len(chosen) < k:
        nxt = max((i for i in range(n) if i not in chosen), key=lambda i: best[i])
        chosen.append(nxt)
        for i in range(n):
            best[i] = min(best[i], timbre.distance(fps[i], fps[nxt]))
    return [group[i] for i in chosen]


def nearest_neighbour_stats(chosen):
    by_cat = {}
    for c in chosen:
        by_cat.setdefault(c["cat"], []).append(c)
    nn = []
    for group in by_cat.values():
        for a in group:
            nn.append(min((timbre.distance(a["m"]["fpdry"], b["m"]["fpdry"]) for b in group if b is not a), default=99.0))
    nn.sort()
    return nn


# ============================================================================================== main
def stage(cands):
    if STAGE.exists():
        shutil.rmtree(STAGE)
    dirs = set()
    for i, c in enumerate(cands):
        c["stage"] = STAGE / ("%s_%03d" % (c["cat"], i // CHUNK)) / ("%04d.augur5" % c["id"])
        c["stage"].parent.mkdir(parents=True, exist_ok=True)
        dirs.add(c["stage"].parent)
        c["stage"].write_text(xml_for(c), encoding="utf-8")
    return sorted(dirs)


def main():
    global PARAMS
    PARAMS = load_params()
    seed = int(sys.argv[sys.argv.index("--seed") + 1]) if "--seed" in sys.argv else 5
    quick = "--quick" in sys.argv
    cands = design(seed, 1 if quick else CANDIDATES_PER_SLOT)
    if quick:
        cands = cands[::10]
    for c in cands:
        finish(c["p"])  # unknown ids fail here, before anything is rendered
    print("%d candidates (seed %d)" % (len(cands), seed), flush=True)
    dirs = stage(cands)
    if "--no-audit" in sys.argv:
        return

    rounds = 4
    for rnd in range(rounds):
        rows = audit(dirs)
        changed = 0
        for c in cands:
            m = rows.get(c["stage"].resolve())
            if m is None:
                continue
            fails = gates(c, m)
            if fails and rnd < rounds - 1:
                fix(c, [f for f in fails if f != "clip"])
                changed += 1
            if level(c, m) and rnd < rounds - 1:
                boost_quiet(c["p"])
                changed += 1
            c["stage"].write_text(xml_for(c), encoding="utf-8")
        print("round %d: %d corrected" % (rnd + 1, changed), flush=True)

    rows = audit(dirs, dry=True)
    by_cat, dropped = {}, {}
    for c in cands:
        m = rows.get(c["stage"].resolve())
        if m is None or not m["fpdry"]:
            dropped["not rendered"] = dropped.get("not rendered", 0) + 1
            continue
        fails = [f for f in gates(c, m) if f != "spiky"]
        if abs(m["loud"] - TARGET_LOUD) > 2.5 and m["peak"] < -1.5:
            fails.append("level")
        if TARGET_LOUD - m["loud"] > SPIKY_DROP_DB:
            fails.append("spiky")
        if fails:
            dropped[fails[0]] = dropped.get(fails[0], 0) + 1
            continue
        c["m"] = m
        by_cat.setdefault(c["cat"], []).append(c)
    for cat, group in by_cat.items():  # the brightest 6 % of every category goes
        group.sort(key=lambda x: x["m"]["centroid"])
        cut = int(len(group) * 0.06)
        dropped["brightest 6 %"] = dropped.get("brightest 6 %", 0) + cut
        by_cat[cat] = group[: len(group) - cut]
    if quick:
        print("quick: kept %d, dropped %s" % (sum(len(g) for g in by_cat.values()), dropped))
        return

    chosen = []
    for cat, fams in slots().items():
        picked = []
        for family, k in fams.items():
            group = [c for c in by_cat.get(cat, []) if c["family"] == family]
            picked += farthest_points(group, k)
        missing = COUNTS[cat] - len(picked)
        if missing > 0:  # a family short of good candidates: the category's other survivors fill in, as different as possible
            rest = [c for c in by_cat.get(cat, []) if c not in picked]
            picked += farthest_points(rest, missing, already=picked)
        chosen += picked
    print("chosen %d" % len(chosen), flush=True)

    used = existing_names()
    namer = random.Random(seed + 2)
    if OUT.exists():
        shutil.rmtree(OUT)
    for c in chosen:
        c["name"] = name_for(namer, c["cat"], c["family"], used)
        path = OUT / c["cat"] / ("%s.augur5" % c["name"])
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(xml_for(c), encoding="utf-8")
    (OUT / "README.txt").write_text(README, encoding="utf-8")

    # Reproducibility: every preset again, in reverse order, after different neighbours.
    out_dirs = sorted(d for d in OUT.iterdir() if d.is_dir())
    fwd = audit(out_dirs)
    rev = audit(out_dirs, reverse=True)
    diffs = sorted((abs(fwd[k]["loud"] - rev[k]["loud"]), k.name) for k in fwd if k in rev)
    print("order check: %d presets, max %.2f dB" % (len(diffs), diffs[-1][0]), flush=True)
    write_report(seed, cands, chosen, dropped, fwd, diffs)
    if "--install" in sys.argv:
        if INSTALL.exists():
            shutil.rmtree(INSTALL)
        shutil.copytree(OUT, INSTALL)
        print("installed -> %s" % INSTALL, flush=True)


README = """SUN ATLAS - the AUGUR-5 bank (TONAL LAB)

1000 presets in 11 categories: PAD, KEYS, PLUCK, LEAD, BASS, BRASS, ARP, SEQ, TEXTURE, DRONE, FX.
Every sound was played through the instrument the way its category is used and set to the same loudness
(-16 dB short-term, peaks under -1 dBFS), so you can browse without touching the volume.

Mod wheel opens the filter on almost every sound; aftertouch adds brightness where it suits.
ARP and SEQ sounds run the arpeggiator: hold a chord (ARP) or a single key (SEQ); they follow the host tempo.

Install: in AUGUR-5, PRESETS > INSTALL PACK (.ZIP), or unzip anywhere and use ADD PRESETS FOLDER.
"""


def write_report(seed, cands, chosen, dropped, rows, diffs):
    loud = sorted(rows[k]["loud"] for k in rows)
    peak = sorted(rows[k]["peak"] for k in rows)
    res = sorted(finish(c["p"]).get("flt_reso", 0.0) for c in chosen)
    nn = nearest_neighbour_stats(chosen)
    fam, kinds = {}, {}
    for c in chosen:
        fam[c["family"]] = fam.get(c["family"], 0) + 1
        kinds[(c["cat"], c["kind"])] = kinds.get((c["cat"], c["kind"]), 0) + 1
    fx = {k: sum(1 for c in chosen if finish(c["p"]).get(k)) for k in
          ("fx_reverb_on", "fx_echo_on", "delay_on", "fx_chorus_on", "fx_phaser_on", "fx_flanger_on", "fx_drive_on", "fx_comp_on", "fuzz_on")}
    with open(REPORT, "w", encoding="utf-8") as fh:
        fh.write("# AUGUR-5 — %s: el banco de presets\n\n" % PACK)
        fh.write("Generador: [`tools/pack/make_sun_atlas.py`](../tools/pack/make_sun_atlas.py). Resultado: `packs/%s/<CATEGORÍA>/<nombre>.augur5`; "
                 "en el navegador aparece como la colección **%s**, con sus categorías. Zip: `python tools/pack/build_zip.py \"%s\"`.\n\n" % (PACK, PACK, PACK))
        fh.write("Semilla %d. %d candidatos (%d por plaza); %d elegidos.\n\n" % (seed, len(cands), CANDIDATES_PER_SLOT, len(chosen)))
        fh.write("| Categoría | Presets | Arquetipos |\n|---|---|---|\n")
        for cat in COUNTS:
            ks = ", ".join("%s %d" % (k, n) for (cc, k), n in sorted(kinds.items()) if cc == cat)
            fh.write("| %s | %d | %s |\n" % (cat, sum(1 for c in chosen if c["cat"] == cat), ks))
        fh.write("\n| Familia | Presets |\n|---|---|\n" + "".join("| %s | %d |\n" % (f, fam.get(f, 0)) for f in FAMILIES))
        fh.write("\n**Nivel:** mediana %.1f dB (objetivo %.0f, los 400 ms más fuertes), rango %.1f … %.1f; pico máximo %.1f dBFS.\n\n"
                 % (loud[len(loud) // 2], TARGET_LOUD, loud[0], loud[-1], peak[-1]))
        fh.write("**Resonancia:** mediana %.2f, máximo %.2f; %d presets por encima de 0.40.\n\n"
                 % (res[len(res) // 2], res[-1], sum(1 for x in res if x > 0.4)))
        fh.write("**Diferencias entre sonidos** (huella tímbrica sin efectos, distancia al vecino más cercano de su categoría; "
                 "1.0 ≈ audible en monitores): mediana %.2f, el 5 %% más cercano %.2f.\n\n" % (nn[len(nn) // 2], nn[len(nn) // 20]))
        fh.write("**Efectos:** " + ", ".join("%s %d" % (k.replace("fx_", "").replace("_on", "").upper(), v) for k, v in fx.items()) + ".\n\n")
        fh.write("**Reproducibilidad:** cada preset se tocó dos veces, en orden directo e inverso (vecinos distintos). "
                 "Diferencia máxima %.2f dB; mediana %.2f dB.\n\n" % (diffs[-1][0], diffs[len(diffs) // 2][0]))
        fh.write("**Descartados:** " + ", ".join("%s: %d" % (k, v) for k, v in sorted(dropped.items(), key=lambda kv: -kv[1])) + ".\n")


if __name__ == "__main__":
    main()
