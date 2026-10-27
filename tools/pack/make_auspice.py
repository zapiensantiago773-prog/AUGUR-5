"""AUGUR-5 - AUSPICE: melodic & progressive house (TONAL LAB).

155 sounds written one by one, each with its own recipe and name (no random recipes, no candidates to pick from):
rolling basses, hypnotic sequences, plucks with dotted echoes, breathing pads, Prophet brass, keys, leads, textures,
drones and transition FX, voiced for a melodic / progressive house set at 120-124 BPM. The building blocks (oscillators,
mixer, filter, matrix, effects rack) are SUN ATLAS's (tools/pack/make_sun_atlas.py); the envelope times here are written
in AUGUR's own knob units (attack = time to the peak, decay / release = time to fall 60 dB), so a value below is what
the panel shows.

Pipeline:
  1. every sound is played through the real processor the way its category is used (augur_preset_audit --pack:
     rolling sixteenths for basses, a line for plucks and leads, held chords for pads, stabs for keys and brass,
     a held chord or key for the arpeggiator, a held note for drones and FX);
  2. LEVEL: -16 dB short-term loudness (loudest 400 ms) with peaks <= -1 dBFS, three rounds (as the series' packs);
  3. chord headroom: pads, keys, brass, plucks and arps played as chords never pass -1 dBFS (--chord-headroom);
  4. gates: non-finite, silence, DC, clipping, harshness above 6 kHz, endless tails; basses also by
     tools/pack/bass_metrics.py (sub, body, bite, mono lows, punch, evenness, crest) and at four roots E1..G2;
  5. every sound again in reverse order: the same level whatever was played before it.
Report: docs/pack_auspice.md.

Usage:  python tools/pack/make_auspice.py [--no-audit] [--only CAT] [--keep-stage]
"""
import concurrent.futures
import csv
import io
import math
import os
import pathlib
import re
import shutil
import subprocess
import sys
from xml.sax.saxutils import quoteattr

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import bass_metrics  # noqa: E402
import make_sun_atlas as atlas  # noqa: E402
import timbre  # noqa: E402
from make_sun_atlas import (AENV, ARP_RATE, AT, BITE, BP, CASCADE, CRUSH, CUT, DIODE, DOWN, DRV, ENS, F1, F2, FENV, FM, FOLD,  # noqa: E402,F401
                            HALL, HP, JUNO1, JUNO12, JUNO2, KEY, L1, L2, LFO, LFO2, LFORATE, LN, LP, MENV, MULTI, NOISE, ORDER,
                            OSC2, PLATE, PW1, PW2, RAND, RANDOMARP, RES, REV1, REV3, RING, ROOM, SAW1, SH1, SH2, SHIMMER, SINE2,
                            SMOOTH2, SPRING, SQR1, SQR2, SUBL, TAPE, TRI1, TRI2, TUBE, UP, UPDOWN, VEL, WHEEL, AMP, DIM, SAWUP2,
                            SAWDN2, arp, chorus, comp, delay, drive, echo, filt, flanger, fuzz, mixer, osc_a, osc_b, phaser,
                            pitch_amt, poly_mod, reverb, route, voices)

ROOT = atlas.ROOT
PACK = "AUSPICE"
OUT = ROOT / "packs" / PACK
STAGE = ROOT / "build" / "auspice_stage"
WAVS = ROOT / "build" / "auspice_wav"
REPORT = ROOT / "docs" / "pack_auspice.md"
AUDIT = atlas.AUDIT
TARGET_LOUD = -16.0
PEAK_MAX = -1.0
TWIN_DISTANCE = 1.5  # timbre.py: closer than ~1.5 is the same sound to a listener
# Basses all play the same mono rolling pattern and the fingerprint's lowest band starts at 60 Hz, above an A1
# fundamental: only their spectrum from 60 Hz up tells them apart. Each still clears 1.0, "clearly audible on monitors".
TWIN_DISTANCE_BASS = 1.1
JOBS = max(2, (os.cpu_count() or 4) - 1)

CATEGORIES = ["BASS", "ARP", "PLUCK", "LEAD", "PAD", "KEYS", "BRASS", "TEXTURE", "DRONE", "FX"]
ROLE = {"BASS": "bass", "ARP": "arp", "PLUCK": "pluck", "LEAD": "lead", "PAD": "pad", "KEYS": "keys", "BRASS": "brass",
        "TEXTURE": "pad", "DRONE": "drone", "FX": "fx"}
NOTE = {"BASS": 33, "ARP": 57, "PLUCK": 60, "LEAD": 62, "PAD": 57, "KEYS": 57, "BRASS": 50, "TEXTURE": 57, "DRONE": 45, "FX": 60}
BASS_ROOTS = (28, 33, 38, 43)  # E1, A1, D2, G2: the range a progressive bassline lives in

# The LFOs' tempo divisions (lfo_div / lfo2_div).
LDIV = {k: i for i, k in enumerate(["1/32T", "1/32", "1/16T", "1/32.", "1/16", "1/8T", "1/16.", "1/8", "1/4T", "1/8.", "1/4",
                                     "1/2T", "1/4.", "1/2", "1/2.", "1/1", "1/1.", "2/1", "4/1", "8/1"])}
RATIO = {"1.5": 0, "2": 1, "3": 2, "4": 3, "5": 4, "10": 5, "LIMIT": 6}
ATTACK_MS = {0.1: 0, 0.3: 1, 1: 2, 3: 3, 10: 4, 30: 5}
RELEASE_S = {0.1: 0, 0.3: 1, 0.6: 2, 1.2: 3, "AUTO": 4}


# ============================================================================================== helpers (knob units)
def adsr(p, which, a, d, s, r):
    """Envelope in the panel's own units: attack to the peak, decay / release to -60 dB."""
    p.update({which + "_a": a, which + "_d": d, which + "_s": s, which + "_r": r})


def lfo(p, wave=TRI1, rate=None, div=None, delay_s=0.0):
    p.update(lfo_wave=wave, lfo_amount=1.0, lfo_delay=delay_s)
    if div:
        p.update(lfo_sync=1, lfo_div=LDIV[div], lfo_rate=p.get("lfo_rate", 4.0))
    else:
        p.update(lfo_sync=0, lfo_rate=rate)


def lfo2(p, wave=SMOOTH2, rate=None, div=None, retrig=False):
    p.update(lfo2_wave=wave, lfo2_retrig=int(retrig))
    if div:
        p.update(lfo2_sync=1, lfo2_div=LDIV[div], lfo2_rate=p.get("lfo2_rate", 2.0))
    else:
        p.update(lfo2_sync=0, lfo2_rate=rate)


def bus_comp(p, threshold, ratio="4", attack=10, release=0.1, makeup=3.0, mix=1.0, schpf=20.0):
    comp(p, threshold=threshold, ratio=RATIO[ratio], attack=ATTACK_MS[attack], release=RELEASE_S[release], makeup=makeup, mix=mix,
         schpf=schpf)


def glue(p, threshold=-20.0, makeup=4.0):
    """Plucks and stabs: a fast bus compressor takes the first milliseconds' spike, so chords keep their level."""
    bus_comp(p, threshold, "4", 0.3, 0.1, makeup)


def wheel_vibrato(p, rate=5.2):
    """MOD wheel = vibrato through WHEEL-MOD (FREQ A + FREQ B from the triangle LFO), as on the Prophet."""
    assert "lfo_wave" not in p, "the shared LFO already has a job: route the wheel elsewhere"
    lfo(p, TRI1, rate=rate)
    p.update(wm_freq_a=1, wm_freq_b=1, wm_amount=0.0, wm_mix=0.0)


def drift(p, cents=6, rate=0.12):
    """Slow per-voice pitch drift from LFO 2 (smooth random), the two oscillators apart."""
    lfo2(p, SMOOTH2, rate=rate)
    route(p, LFO2, F1, pitch_amt(cents / 100.0))
    route(p, LFO2, F2, -pitch_amt(cents * 0.7 / 100.0))


SOUNDS = []


def sound(cat, name, kind, family, role=None, note=None):
    def register(fn):
        SOUNDS.append({"cat": cat, "name": name, "kind": kind, "family": family, "role": role or ROLE[cat],
                       "note": note or NOTE[cat], "fn": fn})
        return fn
    return register


def base(cat):
    p = dict(analog_age=0.3, voice_detune=0.25, voice_spread=0.6, amp_velocity=0.4, at_amount=0.2)
    if cat == "BASS":
        p.update(analog_age=0.18, voice_detune=0.12, voice_spread=0.0, amp_velocity=0.28, at_amount=0.15, hpf_cutoff=22.0)
    if cat not in ("LEAD", "BRASS"):
        route(p, WHEEL, CUT, 0.22)  # the mod wheel opens the filter by about 1.3 octaves
    return p


# ============================================================================================== BASS (20)
# Mono and dry, fundamental at the played note (A1 = 55 Hz in the audit); nothing below it. Body from saws / pulses
# through the Rev 3 (or Rev 1) filter with a short envelope; grit from the mixer's DRIVE and the rack's tube; the bus
# compressor (10 ms attack) lets the attack through and holds the body: hard, powerful, fat.
def bass_voice(p, legato=False, glide=0.0):
    voices(p, 1, legato=legato, glide=glide, spread=0.0, detune=0.12)


# Cutoffs are at C4 with FULL keytrack: at A1 the filter sits 2.25 octaves lower (x 0.21), so 1500 here is ~315 Hz
# under the note, and the envelope opens it from there on every attack.
@sound("BASS", "Keel of Dusk", "rolling", "PROGRESSIVE")
def _(p):
    bass_voice(p)
    osc_a(p)
    osc_b(p, saw=False, pulse=True, pw=50, octave=1)
    mixer(p, 0.9, 0.2, drive=0.45)
    filt(p, 2000, 0.34, 0.35, REV3, key=2, vel=0.2)
    adsr(p, "fenv", 0.001, 0.3, 0.2, 0.1)
    adsr(p, "aenv", 0.001, 0.6, 0.9, 0.07)
    bus_comp(p, -18, "4", 10, 0.1, 3.0)


@sound("BASS", "Templum Sub", "sub", "DEEP")
def _(p):
    bass_voice(p)
    osc_a(p, octave=1)
    osc_b(p, saw=False, tri=True)
    mixer(p, 0.8, 0.3, sub=0.45, drive=0.35)
    filt(p, 2600, 0.12, 0.3, REV3, key=2)
    adsr(p, "fenv", 0.001, 0.3, 0.3, 0.1)
    adsr(p, "aenv", 0.001, 0.8, 0.95, 0.08)
    bus_comp(p, -16, "3", 10, 0.1, 2.0)


@sound("BASS", "Night Current", "rolling", "DEEP")
def _(p):
    bass_voice(p)
    p["osc_model"] = 1
    osc_a(p)
    osc_b(p, octave=1, fine=-4)
    mixer(p, 0.85, 0.45, drive=0.4)
    filt(p, 2000, 0.3, 0.42, REV1, key=2, vel=0.25)
    adsr(p, "fenv", 0.001, 0.25, 0.12, 0.1)
    adsr(p, "aenv", 0.001, 0.55, 0.85, 0.07)
    drive(p, TUBE, 8.0, tone=0.5, mix=0.3)
    bus_comp(p, -18, "4", 10, 0.1, 3.0)


@sound("BASS", "Iron Wingbeat", "driving", "AFTERHOURS")
def _(p):
    bass_voice(p)
    osc_a(p)
    osc_b(p, octave=1)
    mixer(p, 0.8, 0.45, drive=0.75)
    filt(p, 3400, 0.3, 0.25, REV3, key=2, vel=0.25)
    adsr(p, "fenv", 0.001, 0.25, 0.1, 0.08)
    adsr(p, "aenv", 0.003, 0.45, 0.55, 0.06)
    bus_comp(p, -24, "4", 3, 0.1, 6.0)


@sound("BASS", "Augury Rubber", "rubber", "PROGRESSIVE")
def _(p):
    bass_voice(p, legato=True, glide=0.04)
    osc_a(p, saw=False, pulse=True, pw=38)
    osc_b(p, octave=1)
    mixer(p, 0.8, 0.7, drive=0.45)
    filt(p, 2100, 0.45, 0.6, REV3, key=2, vel=0.3)
    adsr(p, "fenv", 0.002, 0.2, 0.0, 0.1)
    adsr(p, "aenv", 0.001, 0.6, 0.9, 0.08)
    bus_comp(p, -16, "3", 10, 0.1, 2.0)


@sound("BASS", "Sunset Engine", "layered", "SUNRISE")
def _(p):
    bass_voice(p)
    osc_a(p, octave=1)
    osc_b(p)
    mixer(p, 0.6, 0.45, sub=0.4, drive=0.35)
    filt(p, 2200, 0.1, 0.3, REV3, key=2, vel=0.2)
    adsr(p, "fenv", 0.001, 0.35, 0.5, 0.1)
    adsr(p, "aenv", 0.001, 0.7, 0.9, 0.08)
    bus_comp(p, -18, "4", 10, 0.1, 3.0)


@sound("BASS", "Pressure Lattice", "sync", "AFTERHOURS")
def _(p):
    bass_voice(p)
    osc_a(p, semi=7, sync=True)
    osc_b(p, saw=False, tri=True)
    mixer(p, 0.8, 0.7, drive=0.4)
    adsr(p, "menv", 0.001, 0.2, 0.0, 0.1)
    route(p, MENV, F1, pitch_amt(12))
    filt(p, 2600, 0.3, 0.3, REV3, key=2, vel=0.2)
    adsr(p, "fenv", 0.001, 0.2, 0.15, 0.1)
    adsr(p, "aenv", 0.001, 0.5, 0.8, 0.07)
    bus_comp(p, -18, "4", 10, 0.1, 3.0)


@sound("BASS", "Shoreline Roller", "round", "BALEARIC")
def _(p):
    bass_voice(p)
    p["osc_model"] = 1
    osc_a(p)
    osc_b(p, saw=False, pulse=True, pw=30, octave=1)
    mixer(p, 0.85, 0.42, drive=0.35)
    filt(p, 2200, 0.15, 0.55, REV1, key=2, vel=0.2)
    adsr(p, "fenv", 0.001, 0.15, 0.35, 0.12)
    adsr(p, "aenv", 0.001, 0.4, 0.6, 0.12)
    bus_comp(p, -14, "3", 10, 0.3, 2.0)


@sound("BASS", "Black Feather", "reese", "DEEP")
def _(p):
    bass_voice(p)
    osc_a(p, octave=1)
    osc_b(p, octave=1, fine=11)
    mixer(p, 0.6, 0.6, sub=0.55, drive=0.6)
    filt(p, 2600, 0.25, 0.25, REV3, key=2)
    adsr(p, "fenv", 0.002, 0.5, 0.4, 0.1)
    adsr(p, "aenv", 0.001, 0.8, 0.95, 0.08)
    lfo2(p, SMOOTH2, rate=0.3)
    route(p, LFO2, CUT, 0.04)
    bus_comp(p, -18, "4", 10, 0.1, 3.0)


@sound("BASS", "Velvet Undertow", "deep_pluck", "DEEP")
def _(p):
    bass_voice(p)
    osc_a(p, saw=False, pulse=True, pw=35, octave=1)
    osc_b(p, saw=False, tri=True)
    mixer(p, 0.58, 0.82, drive=0.4)
    filt(p, 3200, 0.15, 0.45, REV3, key=2, vel=0.25)
    adsr(p, "fenv", 0.001, 0.12, 0.0, 0.1)
    adsr(p, "aenv", 0.001, 0.3, 0.3, 0.08)
    p["amp_velocity"] = 0.2
    bus_comp(p, -16, "3", 10, 0.1, 2.0)


@sound("BASS", "Comet Knock", "fm", "AFTERHOURS")
def _(p):
    bass_voice(p)
    osc_a(p, saw=False, pulse=True, pw=50, octave=1)
    osc_b(p, saw=False, tri=True, octave=1)
    mixer(p, 0.7, 0.0, sub=0.75, drive=0.35)
    adsr(p, "menv", 0.001, 0.06, 0.0, 0.1)
    route(p, MENV, FM, 0.75)
    filt(p, 3500, 0.1, 0.3, REV3, key=2, vel=0.2)
    adsr(p, "fenv", 0.001, 0.18, 0.2, 0.1)
    adsr(p, "aenv", 0.001, 0.6, 0.85, 0.07)
    bus_comp(p, -18, "4", 3, 0.1, 3.0)


@sound("BASS", "Low Sun Arc", "wah", "PROGRESSIVE")
def _(p):
    bass_voice(p, legato=True, glide=0.03)
    osc_a(p)
    osc_b(p, octave=1, fine=3)
    mixer(p, 0.85, 0.35, drive=0.55)
    filt(p, 350, 0.36, 0.5, REV3, key=2, vel=0.3)
    adsr(p, "fenv", 0.002, 0.7, 0.0, 0.15)
    adsr(p, "aenv", 0.002, 0.8, 0.9, 0.08)
    bus_comp(p, -24, "4", 3, 0.1, 8.0)


@sound("BASS", "Driver of Omens", "driving", "AFTERHOURS")
def _(p):
    bass_voice(p)
    osc_a(p)
    osc_b(p, saw=False, pulse=True, pw=25, semi=12)
    mixer(p, 0.9, 0.2, drive=0.55)
    filt(p, 1100, 0.42, 0.45, BITE, key=2, vel=0.25)
    adsr(p, "fenv", 0.001, 0.15, 0.15, 0.08)
    adsr(p, "aenv", 0.001, 0.5, 0.8, 0.06)
    drive(p, DIODE, 8.0, tone=0.5, mix=0.3)
    bus_comp(p, -24, "4", 3, 0.1, 8.0)


@sound("BASS", "Marrow", "sub_click", "DEEP")
def _(p):
    bass_voice(p)
    osc_a(p, octave=1)
    osc_b(p, saw=False, tri=True)
    mixer(p, 0.75, 0.75, noise=0.15, drive=0.4)
    filt(p, 2000, 0.1, 0.65, REV3, key=2, vel=0.2)
    adsr(p, "fenv", 0.001, 0.09, 0.15, 0.05)
    adsr(p, "aenv", 0.001, 0.35, 0.65, 0.08)
    bus_comp(p, -16, "3", 10, 0.1, 2.0)


@sound("BASS", "Gilded Groove", "melodic", "SUNRISE")
def _(p):
    bass_voice(p, legato=True, glide=0.05)
    osc_a(p)
    osc_b(p, saw=False, pulse=True, pw=40, semi=7, octave=1)
    mixer(p, 0.85, 0.15, drive=0.4)
    filt(p, 1800, 0.2, 0.3, REV3, key=2, vel=0.35)
    adsr(p, "fenv", 0.002, 0.5, 0.35, 0.12)
    adsr(p, "aenv", 0.001, 0.7, 0.9, 0.09)
    bus_comp(p, -18, "4", 10, 0.1, 3.0)


@sound("BASS", "Abyssal Pulse", "pwm", "DEEP")
def _(p):
    bass_voice(p)
    osc_a(p, saw=False, pulse=True, pw=26)
    osc_b(p, saw=False, pulse=True, pw=50, octave=1)
    mixer(p, 0.8, 0.25, drive=0.45)
    lfo2(p, TRI2, rate=0.25)
    route(p, LFO2, PW1, 0.12)
    filt(p, 3500, 0.2, 0.4, REV3, key=2, vel=0.25)
    adsr(p, "fenv", 0.001, 0.25, 0.2, 0.1)
    adsr(p, "aenv", 0.001, 0.6, 0.85, 0.07)
    bus_comp(p, -18, "4", 10, 0.1, 3.0)


@sound("BASS", "Granite Wing", "distorted", "AFTERHOURS")
def _(p):
    bass_voice(p)
    osc_a(p)
    osc_b(p, saw=False, tri=True)
    mixer(p, 0.85, 0.25, drive=0.45)
    filt(p, 1900, 0.22, 0.45, REV3, key=2, vel=0.25)
    adsr(p, "fenv", 0.001, 0.2, 0.1, 0.08)
    adsr(p, "aenv", 0.001, 0.55, 0.8, 0.06)
    fuzz(p, 0.45, 0.45, 0.35)
    bus_comp(p, -24, "5", 1, 0.1, 10.0)


@sound("BASS", "Ember Keel", "short", "PROGRESSIVE")
def _(p):
    bass_voice(p)
    osc_a(p, octave=1)
    osc_b(p, saw=False, pulse=True, pw=20)
    mixer(p, 0.6, 0.5, sub=0.45, drive=0.4)
    filt(p, 2200, 0.3, 0.15, REV3, key=2, vel=0.3)
    adsr(p, "fenv", 0.001, 0.16, 0.1, 0.08)
    adsr(p, "aenv", 0.002, 0.4, 0.1, 0.06)
    bus_comp(p, -22, "5", 3, 0.1, 9.0)


@sound("BASS", "Tidal Root", "moving", "BALEARIC")
def _(p):
    bass_voice(p)
    osc_a(p)
    osc_b(p, octave=1)
    mixer(p, 0.8, 0.3, drive=0.35)
    filt(p, 2400, 0.18, 0.55, REV3, key=2, vel=0.25)
    adsr(p, "fenv", 0.002, 0.2, 0.3, 0.12)
    adsr(p, "aenv", 0.001, 0.7, 0.9, 0.09)
    lfo2(p, TRI2, div="1/1")
    route(p, LFO2, CUT, 0.15)
    bus_comp(p, -16, "3", 10, 0.1, 2.0)


@sound("BASS", "Monolith Floor", "tape", "DEEP")
def _(p):
    bass_voice(p)
    osc_a(p, octave=1)
    osc_b(p, saw=False, tri=True)
    mixer(p, 0.7, 0.7, drive=0.4)
    filt(p, 1000, 0.3, 0.45, REV3, key=2, vel=0.2)
    adsr(p, "fenv", 0.001, 0.5, 0.15, 0.1)
    adsr(p, "aenv", 0.001, 0.7, 0.9, 0.08)
    drive(p, TUBE, 10.0, tone=0.45, mix=0.5)
    bus_comp(p, -18, "4", 10, 0.1, 3.0)


# ============================================================================================== ARP (15)
# The arpeggiator follows the host's tempo. ARP sounds: hold a chord; SEQ-style ones (role seq): hold one key.
def arp_voice(p, n=6, spread=0.6):
    voices(p, n, detune=0.22, spread=spread)


@sound("ARP", "Starling Cascade", "up16", "SUNRISE")
def _(p):
    arp_voice(p)
    osc_a(p)
    osc_b(p, fine=7)
    mixer(p, 0.75, 0.5)
    filt(p, 900, 0.2, 0.45, REV3, key=2, vel=0.25)
    adsr(p, "fenv", 0.001, 0.5, 0.0, 0.3)
    adsr(p, "aenv", 0.001, 0.9, 0.0, 0.6)
    arp(p, UP, "1/16", 2, 0.5)
    delay(p, "1/8D", 0.38, 0.22, pingpong=True)
    reverb(p, HALL, 1.4, 3.0, 0.2, predelay=25)


@sound("ARP", "Murmuration", "updown", "PROGRESSIVE")
def _(p):
    arp_voice(p)
    osc_a(p, saw=False, pulse=True, pw=30)
    osc_b(p, saw=False, tri=True, semi=12)
    mixer(p, 0.7, 0.45)
    filt(p, 1100, 0.22, 0.35, REV3, key=2, vel=0.2)
    adsr(p, "fenv", 0.001, 0.45, 0.1, 0.3)
    adsr(p, "aenv", 0.001, 0.8, 0.0, 0.5)
    arp(p, UPDOWN, "1/16", 3, 0.45)
    lfo2(p, TRI2, div="4/1")
    route(p, LFO2, CUT, 0.12)
    echo(p, 3, 0.22, intensity=0.45, sync="1/8.", wow=0.15, flutter=0.1, treble=-3)
    reverb(p, HALL, 1.3, 2.8, 0.18)


@sound("ARP", "Ritual of Larks", "random", "SUNRISE")
def _(p):
    arp_voice(p)
    osc_a(p, saw=False, pulse=True, pw=20)
    osc_b(p, saw=False, tri=True, semi=19)
    mixer(p, 0.4, 0.6)
    poly_mod(p, oscb=0.08)
    filt(p, 3500, 0.08, 0.2, REV3, key=2, hpf=160)
    adsr(p, "fenv", 0.001, 0.4, 0.0, 0.3)
    adsr(p, "aenv", 0.002, 1.2, 0.1, 0.8)
    arp(p, RANDOMARP, "1/16", 2, 0.4)
    delay(p, "1/8D", 0.35, 0.2, pingpong=True)
    reverb(p, SHIMMER, 1.5, 4.5, 0.22, shimmer=0.3, pitch=3)


@sound("ARP", "Hypnotic Templum", "seq_rolling", "DEEP", role="seq", note=45)
def _(p):
    arp_voice(p, 4, 0.35)
    osc_a(p)
    osc_b(p, octave=-1, fine=4)
    mixer(p, 0.8, 0.5, drive=0.35)
    filt(p, 520, 0.32, 0.5, REV1, key=2, vel=0.25)
    adsr(p, "fenv", 0.001, 0.3, 0.0, 0.1)
    adsr(p, "aenv", 0.002, 0.6, 0.2, 0.15)
    arp(p, UPDOWN, "1/16", 2, 0.45)
    route(p, RAND, CUT, 0.05)
    lfo2(p, TRI2, div="2/1")
    route(p, LFO2, CUT, 0.09)
    delay(p, "1/8D", 0.3, 0.14)


@sound("ARP", "Night Flight", "triplet", "PROGRESSIVE")
def _(p):
    arp_voice(p)
    osc_a(p)
    osc_b(p, saw=False, tri=True, octave=1)
    mixer(p, 0.7, 0.4)
    filt(p, 1000, 0.18, 0.35, REV3, key=2, vel=0.2)
    adsr(p, "fenv", 0.001, 0.4, 0.1, 0.3)
    adsr(p, "aenv", 0.001, 0.8, 0.1, 0.5)
    arp(p, UP, "1/16T", 2, 0.45)
    reverb(p, HALL, 1.5, 3.5, 0.24, predelay=30)


@sound("ARP", "Ascending Omen", "updown8", "SUNRISE")
def _(p):
    arp_voice(p)
    osc_a(p, semi=12, sync=True)
    osc_b(p)
    mixer(p, 0.65, 0.4)
    poly_mod(p, fenv=0.25)
    filt(p, 2200, 0.12, 0.3, REV3, key=2)
    adsr(p, "fenv", 0.001, 0.35, 0.0, 0.3)
    adsr(p, "aenv", 0.002, 1.2, 0.25, 0.5)
    arp(p, UPDOWN, "1/8", 3, 0.55)
    delay(p, "1/4D", 0.35, 0.2, pingpong=True)
    reverb(p, HALL, 1.4, 3.0, 0.2)


@sound("ARP", "Swallow Spiral", "octaves", "SUNRISE")
def _(p):
    arp_voice(p)
    osc_a(p, saw=False, pulse=True, pw=22)
    osc_b(p, octave=1, fine=5)
    mixer(p, 0.7, 0.35)
    filt(p, 1600, 0.15, 0.3, REV3, key=2, vel=0.2)
    adsr(p, "fenv", 0.001, 0.3, 0.0, 0.2)
    adsr(p, "aenv", 0.002, 0.8, 0.2, 0.4)
    arp(p, UP, "1/16", 4, 0.3)
    delay(p, "1/16D", 0.3, 0.18, pingpong=True)
    reverb(p, PLATE, 1.1, 2.2, 0.18)


@sound("ARP", "Chain of Lanterns", "order_swing", "BALEARIC")
def _(p):
    arp_voice(p)
    osc_a(p, saw=False, pulse=True, pw=45)
    osc_b(p, saw=False, tri=True, semi=12)
    mixer(p, 0.45, 0.6)
    poly_mod(p, oscb=0.08)
    filt(p, 1800, 0.08, 0.3, REV3, key=2, vel=0.35)
    adsr(p, "fenv", 0.002, 0.6, 0.1, 0.4)
    adsr(p, "aenv", 0.002, 1.2, 0.0, 0.6)
    arp(p, ORDER, "1/16", 2, 0.55, swing=0.12)
    chorus(p, JUNO1, 0.35)
    reverb(p, PLATE, 1.0, 2.0, 0.18)


@sound("ARP", "Afterglow Ladder", "down16", "DEEP")
def _(p):
    arp_voice(p)
    p["osc_model"] = 1
    osc_a(p)
    osc_b(p, fine=-7)
    mixer(p, 0.75, 0.55)
    filt(p, 750, 0.28, 0.45, REV1, key=2, vel=0.25)
    adsr(p, "fenv", 0.001, 0.4, 0.0, 0.25)
    adsr(p, "aenv", 0.001, 0.8, 0.0, 0.4)
    arp(p, DOWN, "1/16", 2, 0.5)
    lfo2(p, SINE2, div="2/1")
    route(p, LFO2, CUT, 0.1)
    echo(p, 5, 0.2, intensity=0.4, sync="1/8.", wow=0.2, flutter=0.15, treble=-4)
    reverb(p, HALL, 1.3, 2.6, 0.16)


@sound("ARP", "Deep Water Sequence", "seq_deep", "DEEP", role="seq", note=45)
def _(p):
    arp_voice(p, 4, 0.3)
    osc_a(p)
    osc_b(p, saw=False, pulse=True, pw=35, fine=5)
    mixer(p, 0.8, 0.45, drive=0.3)
    filt(p, 600, 0.35, 0.45, REV1, key=2, vel=0.35)
    adsr(p, "fenv", 0.001, 0.35, 0.0, 0.15)
    adsr(p, "aenv", 0.001, 0.55, 0.0, 0.15)
    arp(p, UPDOWN, "1/16", 2, 0.4)
    route(p, RAND, CUT, 0.06)
    delay(p, "1/8D", 0.3, 0.12)
    reverb(p, ROOM, 1.0, 1.4, 0.12)


@sound("ARP", "Feather Engine", "dotted16", "PROGRESSIVE")
def _(p):
    arp_voice(p)
    osc_a(p)
    osc_b(p, saw=False, tri=True, semi=7)
    mixer(p, 0.9, 0.55, drive=0.3)
    filt(p, 1500, 0.2, 0.35, REV3, key=2, vel=0.2)
    adsr(p, "fenv", 0.001, 0.35, 0.0, 0.25)
    adsr(p, "aenv", 0.002, 1.0, 0.25, 0.45)
    arp(p, UP, "1/16D", 2, 0.45)
    delay(p, "1/8", 0.3, 0.16, pingpong=True)
    reverb(p, HALL, 1.3, 2.8, 0.18)


@sound("ARP", "Silver Augury", "pwm16", "SUNRISE")
def _(p):
    arp_voice(p)
    osc_a(p, saw=False, pulse=True, pw=20)
    osc_b(p, saw=False, pulse=True, pw=40, octave=1)
    mixer(p, 0.7, 0.35)
    lfo2(p, TRI2, rate=0.4)
    route(p, LFO2, PW1, 0.25)
    filt(p, 1500, 0.15, 0.3, REV3, key=2, vel=0.2)
    adsr(p, "fenv", 0.001, 0.3, 0.0, 0.25)
    adsr(p, "aenv", 0.001, 0.6, 0.0, 0.45)
    arp(p, UP, "1/16", 3, 0.4)
    delay(p, "1/8D", 0.4, 0.2, pingpong=True)
    reverb(p, PLATE, 1.2, 2.5, 0.18)


@sound("ARP", "Morning Thermals", "slow8", "BALEARIC")
def _(p):
    arp_voice(p, 8, 0.75)
    osc_a(p)
    osc_b(p, fine=9)
    mixer(p, 0.7, 0.6)
    filt(p, 1300, 0.12, 0.25, REV3, key=2)
    adsr(p, "fenv", 0.02, 1.2, 0.3, 0.8)
    adsr(p, "aenv", 0.02, 2.0, 0.4, 1.2)
    arp(p, UP, "1/8", 2, 0.85)
    chorus(p, JUNO2, 0.35)
    reverb(p, HALL, 1.7, 5.0, 0.28, predelay=40)


@sound("ARP", "Rotating Sky", "random_triplet", "SUNRISE")
def _(p):
    arp_voice(p)
    osc_a(p, saw=False, pulse=True, pw=12)
    osc_b(p, saw=False, tri=True, semi=24)
    mixer(p, 0.4, 0.6)
    poly_mod(p, oscb=0.18)
    filt(p, 4200, 0.06, 0.15, REV3, key=2, hpf=200)
    adsr(p, "aenv", 0.001, 1.2, 0.0, 0.9)
    arp(p, RANDOMARP, "1/16T", 3, 0.35)
    reverb(p, SHIMMER, 1.6, 5.0, 0.26, shimmer=0.35, pitch=3)


@sound("ARP", "Pulse of the Flock", "seq_pulse", "AFTERHOURS", role="seq", note=45)
def _(p):
    arp_voice(p, 4, 0.3)
    osc_a(p)
    osc_b(p, saw=False, tri=True, semi=12)
    mixer(p, 0.9, 0.4, drive=0.35)
    filt(p, 650, 0.35, 0.5, REV3, key=2)
    adsr(p, "fenv", 0.001, 0.25, 0.0, 0.1)
    adsr(p, "aenv", 0.002, 0.6, 0.3, 0.12)
    arp(p, UP, "1/16", 1, 0.4)
    route(p, RAND, CUT, 0.07)
    lfo2(p, TRI2, div="1/1")
    route(p, LFO2, CUT, 0.08)
    delay(p, "1/16D", 0.25, 0.1)


# ============================================================================================== PLUCK (15)
def pluck_voice(p, n=6, spread=0.6):
    voices(p, n, detune=0.2, spread=spread)


@sound("PLUCK", "Dew Over Templum", "melodic", "PROGRESSIVE")
def _(p):
    pluck_voice(p)
    osc_a(p)
    osc_b(p, fine=6)
    mixer(p, 0.75, 0.55)
    filt(p, 700, 0.18, 0.5, REV3, key=2, vel=0.3)
    adsr(p, "fenv", 0.001, 0.9, 0.0, 0.5)
    adsr(p, "aenv", 0.001, 2.2, 0.0, 0.8)
    delay(p, "1/8D", 0.38, 0.22, pingpong=True)
    reverb(p, HALL, 1.4, 3.5, 0.22, predelay=25)
    glue(p, -20, 8)


@sound("PLUCK", "Glass Swallows", "bell", "SUNRISE")
def _(p):
    pluck_voice(p)
    osc_a(p, saw=False, pulse=True, pw=15)
    osc_b(p, saw=False, tri=True, semi=19)
    mixer(p, 0.4, 0.6)
    poly_mod(p, oscb=0.12)
    filt(p, 3800, 0.06, 0.2, REV3, key=2, hpf=180)
    adsr(p, "fenv", 0.001, 0.5, 0.0, 0.4)
    adsr(p, "aenv", 0.001, 1.8, 0.0, 1.2)
    delay(p, "1/4D", 0.3, 0.16, pingpong=True)
    reverb(p, SHIMMER, 1.4, 4.0, 0.2, shimmer=0.3, pitch=3)


@sound("PLUCK", "Marimba Dusk", "wood", "BALEARIC")
def _(p):
    pluck_voice(p)
    osc_a(p, saw=False, pulse=True, pw=50)
    osc_b(p, saw=False, tri=True, semi=12)
    mixer(p, 0.6, 0.4)
    filt(p, 1200, 0.12, 0.3, REV3, key=2, vel=0.3)
    adsr(p, "fenv", 0.001, 0.45, 0.0, 0.2)
    adsr(p, "aenv", 0.002, 1.8, 0.0, 0.6)
    reverb(p, PLATE, 1.0, 1.8, 0.2)
    glue(p, -24, 8)


@sound("PLUCK", "Sync Petals", "sync", "SUNRISE")
def _(p):
    pluck_voice(p)
    osc_a(p, semi=12, sync=True)
    osc_b(p, fine=4)
    mixer(p, 0.7, 0.35)
    adsr(p, "menv", 0.001, 0.4, 0.0, 0.2)
    route(p, MENV, F1, pitch_amt(9))
    filt(p, 2600, 0.1, 0.3, REV3, key=2)
    adsr(p, "fenv", 0.001, 0.6, 0.0, 0.4)
    adsr(p, "aenv", 0.001, 1.6, 0.0, 0.6)
    echo(p, 3, 0.22, intensity=0.45, sync="1/8.", wow=0.15, flutter=0.1, treble=-3)
    reverb(p, HALL, 1.3, 2.8, 0.18)
    glue(p, -20, 8)


@sound("PLUCK", "Soft Wings", "felt", "BALEARIC")
def _(p):
    pluck_voice(p, 8, 0.5)
    osc_a(p)
    osc_b(p, saw=False, tri=True, fine=3)
    mixer(p, 0.3, 0.75)
    filt(p, 600, 0.06, 0.35, REV3, key=2, vel=0.5)
    adsr(p, "fenv", 0.001, 1.5, 0.0, 0.8)
    adsr(p, "aenv", 0.002, 2.6, 0.0, 1.2)
    reverb(p, ROOM, 1.0, 1.6, 0.2)


@sound("PLUCK", "Ember Rain", "classic", "PROGRESSIVE")
def _(p):
    pluck_voice(p)
    p["osc_model"] = 1
    osc_a(p)
    osc_b(p, octave=1, fine=5)
    mixer(p, 0.75, 0.4, drive=0.3)
    filt(p, 650, 0.22, 0.55, REV1, key=2, vel=0.3)
    adsr(p, "fenv", 0.001, 0.7, 0.0, 0.4)
    adsr(p, "aenv", 0.001, 1.3, 0.0, 0.7)
    delay(p, "1/4D", 0.35, 0.2, pingpong=True)
    reverb(p, HALL, 1.3, 3.0, 0.2)
    glue(p, -20, 7)


@sound("PLUCK", "Harbor Lights", "pwm", "BALEARIC")
def _(p):
    pluck_voice(p)
    osc_a(p, saw=False, pulse=True, pw=35)
    osc_b(p, saw=False, pulse=True, pw=45, fine=6)
    mixer(p, 0.7, 0.55)
    lfo2(p, TRI2, rate=0.6)
    route(p, LFO2, PW1, 0.25)
    route(p, LFO2, PW2, -0.2)
    filt(p, 1200, 0.15, 0.4, REV3, key=2, vel=0.3)
    adsr(p, "fenv", 0.001, 0.6, 0.0, 0.4)
    adsr(p, "aenv", 0.001, 1.2, 0.0, 0.6)
    chorus(p, JUNO2, 0.35)
    reverb(p, PLATE, 1.1, 2.2, 0.18)
    glue(p, -20, 6)


@sound("PLUCK", "Ivory Harp", "harp", "SUNRISE")
def _(p):
    pluck_voice(p, 8)
    osc_a(p)
    osc_b(p, saw=False, tri=True, semi=12)
    mixer(p, 0.5, 0.6)
    filt(p, 1500, 0.06, 0.4, REV3, key=2, vel=0.3)
    adsr(p, "fenv", 0.001, 0.9, 0.1, 1.0)
    adsr(p, "aenv", 0.001, 3.2, 0.0, 2.2)
    reverb(p, HALL, 1.5, 4.0, 0.24, predelay=30)


@sound("PLUCK", "Kalimba of Omens", "kalimba", "BALEARIC")
def _(p):
    pluck_voice(p)
    osc_a(p, saw=False, pulse=True, pw=12)
    osc_b(p, saw=False, tri=True)
    mixer(p, 0.25, 0.75)
    poly_mod(p, oscb=0.1)
    filt(p, 2600, 0.05, 0.3, REV3, key=2, hpf=200)
    adsr(p, "fenv", 0.001, 0.4, 0.0, 0.3)
    adsr(p, "aenv", 0.001, 1.5, 0.0, 1.0)
    reverb(p, SPRING, 1.0, 2.0, 0.2)


@sound("PLUCK", "Polymod Sparrow", "polymod", "PROGRESSIVE")
def _(p):
    pluck_voice(p)
    osc_a(p, saw=True)
    osc_b(p, saw=False, tri=True, semi=7)
    mixer(p, 0.75, 0.25)
    poly_mod(p, fenv=0.35)
    filt(p, 1700, 0.12, 0.4, REV3, key=2)
    adsr(p, "fenv", 0.001, 0.25, 0.0, 0.2)
    adsr(p, "aenv", 0.001, 1.0, 0.0, 0.6)
    delay(p, "1/8D", 0.35, 0.2, pingpong=True)
    reverb(p, HALL, 1.3, 2.8, 0.18)
    glue(p, -22, 10)


@sound("PLUCK", "Midnight Drops", "deep", "DEEP")
def _(p):
    pluck_voice(p)
    p["osc_model"] = 1
    osc_a(p)
    osc_b(p, fine=-6)
    mixer(p, 0.75, 0.55)
    filt(p, 480, 0.25, 0.55, REV1, key=2, vel=0.35)
    adsr(p, "fenv", 0.004, 0.6, 0.0, 0.4)
    adsr(p, "aenv", 0.003, 1.8, 0.0, 0.8)
    delay(p, "1/8D", 0.45, 0.24, pingpong=True)
    reverb(p, HALL, 1.6, 4.5, 0.24, predelay=35)
    glue(p, -22, 6)


@sound("PLUCK", "Sunlit Marbles", "bright", "SUNRISE")
def _(p):
    pluck_voice(p)
    osc_a(p, saw=False, pulse=True, pw=25)
    osc_b(p, octave=1, fine=4)
    mixer(p, 0.7, 0.35)
    filt(p, 1500, 0.12, 0.4, REV3, key=2, vel=0.25)
    adsr(p, "fenv", 0.001, 0.35, 0.0, 0.3)
    adsr(p, "aenv", 0.001, 1.2, 0.0, 0.5)
    chorus(p, DIM, 0.3)
    delay(p, "1/16D", 0.3, 0.16, pingpong=True)
    reverb(p, PLATE, 1.1, 2.0, 0.16)
    glue(p, -22, 8)


@sound("PLUCK", "Copper Taps", "percussive", "AFTERHOURS")
def _(p):
    pluck_voice(p)
    osc_a(p)
    osc_b(p, saw=False, pulse=True, pw=30, octave=1)
    mixer(p, 0.65, 0.3, noise=0.05)
    filt(p, 1600, 0.2, 0.35, MULTI, key=2, mode=LP, slope=1, vel=0.25)
    adsr(p, "fenv", 0.001, 0.2, 0.0, 0.15)
    adsr(p, "aenv", 0.002, 1.2, 0.0, 0.3)
    delay(p, "1/8D", 0.3, 0.16, pingpong=True)
    reverb(p, ROOM, 1.0, 1.4, 0.16)
    glue(p, -24, 8)


@sound("PLUCK", "Long Shadow", "pluck_pad", "DEEP")
def _(p):
    pluck_voice(p, 8, 0.7)
    osc_a(p)
    osc_b(p, fine=8)
    mixer(p, 0.7, 0.6)
    filt(p, 750, 0.15, 0.45, REV3, key=2, vel=0.25)
    adsr(p, "fenv", 0.003, 1.8, 0.1, 1.2)
    adsr(p, "aenv", 0.005, 4.0, 0.15, 2.0)
    chorus(p, JUNO1, 0.3)
    reverb(p, HALL, 1.6, 5.0, 0.26, predelay=40)


@sound("PLUCK", "Wingtip Stab", "stab", "PROGRESSIVE")
def _(p):
    pluck_voice(p)
    osc_a(p)
    osc_b(p, saw=False, pulse=True, pw=40, fine=7)
    mixer(p, 0.75, 0.55, drive=0.25)
    filt(p, 900, 0.15, 0.45, REV3, key=2, vel=0.3)
    adsr(p, "fenv", 0.001, 0.5, 0.0, 0.3)
    adsr(p, "aenv", 0.002, 1.2, 0.0, 0.4)
    delay(p, "1/8D", 0.42, 0.22, pingpong=True)
    reverb(p, PLATE, 1.2, 2.4, 0.18)
    glue(p, -22, 6)


# ============================================================================================== LEAD (15)
def mono_lead(p, glide=0.06):
    voices(p, 1, legato=True, glide=glide, spread=0.0, detune=0.25)


@sound("LEAD", "Herald of Dawn", "saw", "SUNRISE")
def _(p):
    mono_lead(p)
    osc_a(p)
    osc_b(p, fine=8)
    mixer(p, 0.75, 0.6)
    filt(p, 1800, 0.15, 0.25, REV3, key=2, vel=0.2)
    adsr(p, "fenv", 0.01, 1.2, 0.5, 0.6)
    adsr(p, "aenv", 0.006, 1.0, 0.9, 0.7)
    wheel_vibrato(p)
    delay(p, "1/4D", 0.35, 0.18, pingpong=True)
    reverb(p, HALL, 1.4, 3.2, 0.2, predelay=30)


@sound("LEAD", "Falcon Sync", "sync", "PROGRESSIVE")
def _(p):
    mono_lead(p, 0.05)
    osc_a(p, semi=7, sync=True)
    osc_b(p, fine=4)
    mixer(p, 0.75, 0.35)
    poly_mod(p, fenv=0.4)
    filt(p, 3200, 0.12, 0.25, REV3, key=2)
    adsr(p, "fenv", 0.005, 0.8, 0.35, 0.5)
    adsr(p, "aenv", 0.004, 1.0, 0.9, 0.5)
    wheel_vibrato(p)
    delay(p, "1/8D", 0.35, 0.16, pingpong=True)
    reverb(p, HALL, 1.3, 2.6, 0.16)


@sound("LEAD", "Lonely Kite", "soft", "BALEARIC")
def _(p):
    mono_lead(p, 0.08)
    osc_a(p, saw=False, pulse=True, pw=42)
    osc_b(p, saw=False, tri=True, octave=1)
    mixer(p, 0.4, 0.7)
    filt(p, 2200, 0.08, 0.2, REV3, key=2)
    adsr(p, "fenv", 0.02, 1.0, 0.6, 0.6)
    adsr(p, "aenv", 0.015, 1.0, 0.9, 0.9)
    wheel_vibrato(p, 5.0)
    delay(p, "1/8D", 0.4, 0.22, pingpong=True)
    reverb(p, HALL, 1.6, 4.5, 0.26, predelay=40)


@sound("LEAD", "Swan Unison", "unison", "SUNRISE")
def _(p):
    voices(p, 4, unison=True, detune=0.4, spread=0.5, legato=True, glide=0.05)
    osc_a(p)
    osc_b(p, fine=7)
    mixer(p, 0.7, 0.55)
    filt(p, 2400, 0.12, 0.25, REV3, key=2)
    adsr(p, "fenv", 0.01, 1.0, 0.5, 0.6)
    adsr(p, "aenv", 0.008, 1.0, 0.9, 0.7)
    wheel_vibrato(p)
    delay(p, "1/4D", 0.3, 0.16, pingpong=True)
    reverb(p, HALL, 1.4, 3.0, 0.2)


@sound("LEAD", "Copper Horn", "brass_lead", "PROGRESSIVE")
def _(p):
    mono_lead(p, 0.04)
    p["osc_model"] = 1
    osc_a(p)
    osc_b(p, saw=False, pulse=True, pw=38, fine=6)
    mixer(p, 0.75, 0.55, drive=0.35)
    filt(p, 900, 0.15, 0.5, REV1, key=2, vel=0.3)
    adsr(p, "fenv", 0.05, 0.9, 0.5, 0.5)
    adsr(p, "aenv", 0.01, 1.0, 0.9, 0.5)
    wheel_vibrato(p)
    reverb(p, HALL, 1.3, 2.6, 0.18)


@sound("LEAD", "Thread of Light", "whistle", "BALEARIC")
def _(p):
    mono_lead(p, 0.12)
    osc_a(p, saw=False, pulse=True, pw=50)
    osc_b(p, saw=False, tri=True, octave=1)
    mixer(p, 0.15, 0.85, noise=0.03)
    filt(p, 4500, 0.05, 0.1, REV3, key=2)
    adsr(p, "aenv", 0.02, 1.0, 0.95, 0.8)
    wheel_vibrato(p, 5.4)
    echo(p, 3, 0.24, intensity=0.45, sync="1/4.", wow=0.2, flutter=0.15, treble=-4)
    reverb(p, HALL, 1.5, 4.0, 0.24)


@sound("LEAD", "Oracle Voice", "vocal", "DEEP")
def _(p):
    mono_lead(p, 0.07)
    osc_a(p, saw=False, pulse=True, pw=30)
    osc_b(p, fine=6)
    mixer(p, 0.7, 0.4, noise=0.04)
    filt(p, 1100, 0.3, 0.2, MULTI, key=1, mode=BP, slope=1)
    lfo2(p, SMOOTH2, rate=0.8)
    route(p, LFO2, CUT, 0.08)
    adsr(p, "fenv", 0.03, 1.0, 0.6, 0.5)
    adsr(p, "aenv", 0.02, 1.0, 0.9, 0.7)
    wheel_vibrato(p, 5.0)
    delay(p, "1/4D", 0.3, 0.16, pingpong=True)
    reverb(p, HALL, 1.5, 3.5, 0.22)


@sound("LEAD", "Ember Flare", "fuzz", "AFTERHOURS")
def _(p):
    mono_lead(p, 0.04)
    osc_a(p)
    osc_b(p, saw=False, pulse=True, pw=40, fine=5)
    mixer(p, 0.75, 0.4)
    filt(p, 2200, 0.12, 0.25, REV3, key=2)
    adsr(p, "fenv", 0.005, 0.8, 0.5, 0.4)
    adsr(p, "aenv", 0.004, 1.0, 0.9, 0.4)
    fuzz(p, 0.6, 0.4, 0.55)
    wheel_vibrato(p)
    delay(p, "1/8D", 0.3, 0.14, pingpong=True)
    reverb(p, ROOM, 1.0, 1.6, 0.14)


@sound("LEAD", "Arrow of Gold", "poly_lead", "SUNRISE")
def _(p):
    voices(p, 6, detune=0.3, spread=0.5, glide=0.0)
    osc_a(p)
    osc_b(p, saw=False, pulse=True, pw=45, fine=7)
    mixer(p, 0.75, 0.55)
    filt(p, 1600, 0.12, 0.3, REV3, key=2, vel=0.25)
    adsr(p, "fenv", 0.005, 1.0, 0.45, 0.6)
    adsr(p, "aenv", 0.005, 1.0, 0.85, 0.8)
    wheel_vibrato(p)
    chorus(p, JUNO1, 0.3)
    delay(p, "1/4D", 0.3, 0.16, pingpong=True)
    reverb(p, HALL, 1.4, 3.0, 0.2)


@sound("LEAD", "Heron at Dusk", "pwm", "BALEARIC")
def _(p):
    mono_lead(p, 0.06)
    osc_a(p, saw=False, pulse=True, pw=35)
    osc_b(p, saw=False, pulse=True, pw=42, fine=6)
    mixer(p, 0.7, 0.55)
    lfo2(p, TRI2, rate=0.7)
    route(p, LFO2, PW1, 0.3)
    route(p, LFO2, PW2, -0.25)
    filt(p, 1900, 0.12, 0.2, REV3, key=2)
    adsr(p, "fenv", 0.01, 1.0, 0.6, 0.5)
    adsr(p, "aenv", 0.01, 1.0, 0.9, 0.7)
    wheel_vibrato(p)
    chorus(p, JUNO2, 0.3)
    reverb(p, HALL, 1.4, 3.2, 0.2)


@sound("LEAD", "Signal Beyond", "fm", "AFTERHOURS")
def _(p):
    mono_lead(p, 0.05)
    osc_a(p, saw=False, pulse=True, pw=50)
    osc_b(p, saw=False, tri=True, semi=12)
    mixer(p, 0.8, 0.0, xmod=0.2)
    route(p, WHEEL, FM, 0.35)
    filt(p, 3600, 0.06, 0.15, REV3, key=2)
    adsr(p, "fenv", 0.005, 0.8, 0.6, 0.4)
    adsr(p, "aenv", 0.005, 1.0, 0.9, 0.5)
    delay(p, "1/8D", 0.35, 0.16, pingpong=True)
    reverb(p, PLATE, 1.1, 2.2, 0.16)


@sound("LEAD", "Siren of the Sea", "glide", "DEEP")
def _(p):
    mono_lead(p, 0.16)
    osc_a(p, saw=False, pulse=True, pw=12)
    osc_b(p, saw=False, tri=True, octave=1, fine=3)
    mixer(p, 0.55, 0.6)
    filt(p, 1800, 0.18, 0.15, REV3, key=2)
    adsr(p, "aenv", 0.03, 1.0, 0.95, 1.0)
    wheel_vibrato(p, 4.8)
    echo(p, 5, 0.26, intensity=0.5, sync="1/4.", wow=0.3, flutter=0.2, treble=-5)
    reverb(p, HALL, 1.7, 5.0, 0.26, predelay=45)


@sound("LEAD", "Night Beacon", "bite", "AFTERHOURS")
def _(p):
    mono_lead(p, 0.05)
    osc_a(p)
    osc_b(p, fine=5)
    mixer(p, 0.8, 0.45, drive=0.35)
    filt(p, 800, 0.4, 0.45, BITE, key=2, vel=0.4)
    adsr(p, "fenv", 0.002, 0.6, 0.3, 0.3)
    adsr(p, "aenv", 0.003, 1.0, 0.9, 0.4)
    wheel_vibrato(p)
    delay(p, "1/8D", 0.4, 0.18, pingpong=True)
    reverb(p, ROOM, 1.0, 1.6, 0.14)


@sound("LEAD", "Swift Comet", "pluck_lead", "PROGRESSIVE")
def _(p):
    mono_lead(p, 0.03)
    osc_a(p)
    osc_b(p, octave=1, fine=5)
    mixer(p, 0.75, 0.4)
    filt(p, 1000, 0.18, 0.5, REV3, key=2, vel=0.3)
    adsr(p, "fenv", 0.001, 0.6, 0.15, 0.4)
    adsr(p, "aenv", 0.002, 1.4, 0.35, 0.6)
    wheel_vibrato(p)
    delay(p, "1/8D", 0.5, 0.26, pingpong=True)
    reverb(p, HALL, 1.5, 3.5, 0.22)
    bus_comp(p, -18, "3", 10, 0.3, 5.0)


@sound("LEAD", "Solar Hymn", "big_saw", "SUNRISE")
def _(p):
    voices(p, 3, unison=True, detune=0.5, spread=0.7, legato=True, glide=0.05)
    osc_a(p)
    osc_b(p, octave=-1, fine=9)
    mixer(p, 0.7, 0.6)
    filt(p, 2000, 0.12, 0.3, REV3, key=2)
    adsr(p, "fenv", 0.02, 1.2, 0.5, 0.7)
    adsr(p, "aenv", 0.01, 1.0, 0.9, 0.9)
    wheel_vibrato(p)
    phaser(p, 0.3, rate=0.12, stages=1, fb=0.3)
    reverb(p, HALL, 1.6, 4.0, 0.24, predelay=35)


# ============================================================================================== PAD (15)
def pad_voice(p, n=8, detune=0.4, spread=0.8):
    voices(p, n, detune=detune, spread=spread)


@sound("PAD", "Firmament", "strings", "SUNRISE")
def _(p):
    pad_voice(p)
    osc_a(p)
    osc_b(p, fine=9)
    mixer(p, 0.7, 0.6)
    filt(p, 1600, 0.1, 0.2, REV3, key=1)
    adsr(p, "fenv", 1.2, 6.0, 0.6, 5.0)
    adsr(p, "aenv", 1.2, 4.0, 0.9, 4.5)
    chorus(p, ENS, 0.4, rate=0.8)
    reverb(p, HALL, 1.6, 6.0, 0.32, predelay=40)


@sound("PAD", "Augur's Veil", "pwm", "PROGRESSIVE")
def _(p):
    pad_voice(p)
    osc_a(p, saw=False, pulse=True, pw=42)
    osc_b(p, saw=False, pulse=True, pw=46, fine=6)
    mixer(p, 0.7, 0.55)
    lfo2(p, TRI2, rate=0.3)
    route(p, LFO2, PW1, 0.3)
    route(p, LFO2, PW2, -0.3)
    filt(p, 1500, 0.12, 0.25, REV3, key=1)
    adsr(p, "fenv", 0.8, 5.0, 0.5, 4.0)
    adsr(p, "aenv", 0.7, 3.5, 0.9, 4.0)
    chorus(p, JUNO2, 0.4)
    reverb(p, HALL, 1.5, 5.0, 0.3)


@sound("PAD", "Sunset Cathedral", "fifths", "SUNRISE")
def _(p):
    pad_voice(p, 8, 0.35, 0.85)
    osc_a(p)
    osc_b(p, saw=False, tri=True, semi=7, fine=3)
    mixer(p, 0.65, 0.5)
    filt(p, 1300, 0.15, 0.2, REV3, key=1)
    adsr(p, "fenv", 1.5, 6.0, 0.6, 5.0)
    adsr(p, "aenv", 1.5, 4.0, 0.9, 5.0)
    reverb(p, SHIMMER, 1.7, 7.0, 0.32, shimmer=0.35, pitch=3, predelay=40)


@sound("PAD", "Dark Horizon", "dark", "DEEP")
def _(p):
    pad_voice(p)
    p.update(osc_model=1, analog_age=0.5)
    osc_a(p)
    osc_b(p, octave=-1, fine=10)
    mixer(p, 0.7, 0.55, sub=0.15)
    filt(p, 650, 0.2, 0.25, REV1, key=1)
    lfo(p, TRI1, rate=0.08)
    route(p, LFO, CUT, 0.05)
    adsr(p, "fenv", 1.0, 6.0, 0.5, 4.5)
    adsr(p, "aenv", 1.0, 4.0, 0.9, 5.0)
    reverb(p, HALL, 1.7, 7.0, 0.32, predelay=50)


@sound("PAD", "Breath of Feathers", "breath", "BALEARIC")
def _(p):
    pad_voice(p)
    osc_a(p)
    osc_b(p, saw=False, tri=True, octave=-1)
    mixer(p, 0.45, 0.45, noise=0.15)
    filt(p, 1300, 0.2, 0.15, REV3, key=1)
    lfo2(p, SMOOTH2, rate=0.25)
    route(p, LFO2, CUT, 0.05)
    adsr(p, "aenv", 1.4, 4.0, 0.9, 5.0)
    reverb(p, HALL, 1.6, 6.0, 0.34)


@sound("PAD", "Slow Thermals", "sync_swell", "PROGRESSIVE")
def _(p):
    pad_voice(p)
    osc_a(p, semi=7, sync=True)
    osc_b(p, fine=5)
    mixer(p, 0.6, 0.5)
    adsr(p, "menv", 2.5, 8.0, 0.6, 5.0)
    route(p, MENV, F1, pitch_amt(6))
    filt(p, 2200, 0.1, 0.2, REV3, key=1)
    adsr(p, "fenv", 1.0, 6.0, 0.6, 4.0)
    adsr(p, "aenv", 1.2, 4.0, 0.9, 4.5)
    chorus(p, JUNO1, 0.35)
    reverb(p, HALL, 1.5, 5.0, 0.3)


@sound("PAD", "Choir of Omens", "choir", "DEEP")
def _(p):
    pad_voice(p)
    osc_a(p, saw=False, pulse=True, pw=30)
    osc_b(p, octave=1, fine=6)
    mixer(p, 0.7, 0.3, noise=0.04)
    filt(p, 1300, 0.2, 0.1, REV3, key=1)
    lfo2(p, SMOOTH2, rate=0.4)
    route(p, LFO2, CUT, 0.03)
    adsr(p, "aenv", 1.0, 4.0, 0.9, 4.5)
    chorus(p, ENS, 0.45, rate=0.7)
    reverb(p, HALL, 1.6, 6.0, 0.32)


@sound("PAD", "Glass Sky", "glass", "SUNRISE")
def _(p):
    pad_voice(p)
    osc_a(p, saw=False, pulse=True, pw=15)
    osc_b(p, saw=False, tri=True, semi=12)
    mixer(p, 0.4, 0.6)
    poly_mod(p, oscb=0.1)
    filt(p, 3800, 0.08, 0.15, MULTI, key=2, slope=1, hpf=180)
    adsr(p, "aenv", 0.8, 4.0, 0.8, 5.0)
    reverb(p, SHIMMER, 1.6, 6.0, 0.3, shimmer=0.4, pitch=3)


@sound("PAD", "Tidal Bloom", "sweep", "PROGRESSIVE")
def _(p):
    pad_voice(p)
    osc_a(p)
    osc_b(p, saw=True, fine=8, octave=-1)
    mixer(p, 0.7, 0.55)
    filt(p, 900, 0.22, 0.15, REV3, key=1)
    lfo2(p, SINE2, div="4/1")
    route(p, LFO2, CUT, 0.25)
    adsr(p, "aenv", 1.0, 4.0, 0.9, 4.0)
    chorus(p, JUNO12, 0.35)
    reverb(p, HALL, 1.6, 6.0, 0.3, predelay=40)


@sound("PAD", "Lagoon Haze", "soft", "BALEARIC")
def _(p):
    pad_voice(p, 8, 0.3, 0.7)
    osc_a(p, saw=False, pulse=True, pw=45)
    osc_b(p, saw=False, tri=True, fine=4)
    mixer(p, 0.45, 0.7)
    filt(p, 1000, 0.08, 0.15, REV3, key=1)
    adsr(p, "aenv", 1.2, 4.0, 0.9, 4.5)
    drift(p, 7)
    reverb(p, PLATE, 1.4, 4.0, 0.3, damp=0.6)


@sound("PAD", "Golden Hour Strings", "strings_vib", "SUNRISE")
def _(p):
    pad_voice(p)
    osc_a(p)
    osc_b(p, fine=11)
    mixer(p, 0.7, 0.6)
    filt(p, 2000, 0.08, 0.15, REV3, key=1)
    lfo(p, TRI1, rate=5.0, delay_s=0.9)
    route(p, LFO, F1, pitch_amt(0.06))
    route(p, LFO, F2, pitch_amt(0.06))
    adsr(p, "fenv", 0.6, 5.0, 0.7, 4.0)
    adsr(p, "aenv", 0.6, 3.0, 0.9, 3.5)
    chorus(p, ENS, 0.4, rate=0.9)
    reverb(p, HALL, 1.5, 5.0, 0.3)


@sound("PAD", "Night Canopy", "phased", "DEEP")
def _(p):
    pad_voice(p)
    osc_a(p, saw=False, pulse=True, pw=30)
    osc_b(p, saw=False, tri=True, octave=1, fine=7)
    mixer(p, 0.7, 0.5)
    filt(p, 1100, 0.2, 0.2, REV3, key=1)
    adsr(p, "fenv", 1.5, 6.0, 0.5, 5.0)
    adsr(p, "aenv", 1.2, 4.0, 0.9, 5.0)
    phaser(p, 0.5, rate=0.06, stages=3, fb=0.6)
    reverb(p, HALL, 1.7, 7.0, 0.3, predelay=50)


@sound("PAD", "Silk Halo", "airy", "SUNRISE")
def _(p):
    pad_voice(p)
    osc_a(p, saw=False, pulse=True, pw=20)
    osc_b(p, saw=False, tri=True, octave=1)
    mixer(p, 0.3, 0.6, noise=0.06)
    filt(p, 4500, 0.08, 0.1, REV3, key=2, hpf=280)
    adsr(p, "aenv", 1.5, 4.0, 0.9, 5.0)
    reverb(p, SHIMMER, 1.7, 7.0, 0.34, shimmer=0.45, pitch=3)


@sound("PAD", "Endless Field", "evolving", "PROGRESSIVE")
def _(p):
    pad_voice(p)
    osc_a(p, saw=True, pulse=True, pw=40)
    osc_b(p, fine=8)
    mixer(p, 0.65, 0.55)
    filt(p, 1200, 0.18, 0.15, REV3, key=1)
    lfo2(p, SMOOTH2, rate=0.15)
    route(p, LFO2, CUT, 0.1)
    route(p, LFO2, PW1, 0.2)
    adsr(p, "aenv", 1.5, 4.0, 0.9, 5.0)
    echo(p, 8, 0.2, intensity=0.4, time_ms=380, wow=0.4, flutter=0.25, sat=0.45, treble=-5)
    reverb(p, HALL, 1.6, 6.0, 0.3)


@sound("PAD", "Warm Embrace", "classic_poly", "BALEARIC")
def _(p):
    pad_voice(p, 8, 0.3, 0.7)
    osc_a(p)
    osc_b(p, saw=False, pulse=True, pw=40, fine=6)
    mixer(p, 0.7, 0.55)
    filt(p, 1200, 0.12, 0.3, REV3, key=1, vel=0.2)
    adsr(p, "fenv", 0.4, 4.0, 0.5, 3.0)
    adsr(p, "aenv", 0.4, 3.0, 0.9, 3.0)
    chorus(p, JUNO1, 0.4)
    reverb(p, PLATE, 1.3, 3.5, 0.26)


# ============================================================================================== KEYS (15)
def keys_voice(p, n=8, spread=0.5):
    voices(p, n, detune=0.2, spread=spread)


@sound("KEYS", "Deep Chord Ritual", "dub_chord", "DEEP")
def _(p):
    keys_voice(p)
    osc_a(p)
    osc_b(p, fine=7)
    mixer(p, 0.75, 0.6)
    filt(p, 800, 0.15, 0.35, REV3, key=2, vel=0.3)
    adsr(p, "fenv", 0.001, 0.6, 0.0, 0.3)
    adsr(p, "aenv", 0.002, 1.4, 0.15, 0.4)
    delay(p, "1/8D", 0.5, 0.25, pingpong=True)
    reverb(p, HALL, 1.3, 3.0, 0.2)


@sound("KEYS", "Tine Lantern", "epiano", "BALEARIC")
def _(p):
    keys_voice(p)
    osc_a(p, saw=False, pulse=True, pw=45)
    osc_b(p, saw=False, tri=True, semi=12)
    mixer(p, 0.35, 0.6)
    poly_mod(p, oscb=0.18)
    filt(p, 2800, 0.05, 0.25, REV3, key=2, vel=0.4)
    adsr(p, "fenv", 0.002, 1.8, 0.2, 0.9)
    adsr(p, "aenv", 0.002, 4.5, 0.1, 1.2)
    lfo(p, TRI1, rate=4.5)
    route(p, LFO, AMP, -0.12)
    chorus(p, JUNO1, 0.3)
    reverb(p, PLATE, 1.1, 2.2, 0.18)


@sound("KEYS", "Velvet Organ", "organ", "BALEARIC")
def _(p):
    keys_voice(p)
    osc_a(p, saw=False, pulse=True, pw=50)
    osc_b(p, saw=False, pulse=True, pw=50, semi=12, fine=2)
    mixer(p, 0.7, 0.45, sub=0.3)
    filt(p, 4000, 0.0, 0.0, REV3, key=2)
    adsr(p, "aenv", 0.004, 0.3, 1.0, 0.25)
    p["amp_velocity"] = 0.1
    phaser(p, 0.35, rate=0.6, stages=0, fb=0.2)
    reverb(p, ROOM, 1.0, 1.4, 0.16)


@sound("KEYS", "Felt Hammers", "felt", "BALEARIC")
def _(p):
    keys_voice(p)
    osc_a(p)
    osc_b(p, saw=False, tri=True, fine=3)
    mixer(p, 0.25, 0.75)
    filt(p, 900, 0.05, 0.3, REV3, key=2, vel=0.5)
    adsr(p, "fenv", 0.001, 3.0, 0.0, 1.2)
    adsr(p, "aenv", 0.002, 6.0, 0.0, 1.5)
    reverb(p, ROOM, 1.0, 1.6, 0.2, damp=0.6)


@sound("KEYS", "Glass Keys", "bell", "SUNRISE")
def _(p):
    keys_voice(p)
    osc_a(p, saw=False, pulse=True, pw=18)
    osc_b(p, saw=False, tri=True, semi=19)
    mixer(p, 0.4, 0.5)
    poly_mod(p, oscb=0.3)
    filt(p, 5000, 0.05, 0.15, REV3, key=2, hpf=200)
    adsr(p, "fenv", 0.001, 3.0, 0.3, 3.0)
    adsr(p, "aenv", 0.001, 6.0, 0.0, 3.5)
    reverb(p, HALL, 1.4, 3.5, 0.22)


@sound("KEYS", "Warm Poly Keys", "poly_keys", "PROGRESSIVE")
def _(p):
    keys_voice(p)
    osc_a(p)
    osc_b(p, fine=7)
    mixer(p, 0.75, 0.55)
    filt(p, 1300, 0.15, 0.4, REV3, key=2, vel=0.3)
    adsr(p, "fenv", 0.002, 1.8, 0.3, 0.9)
    adsr(p, "aenv", 0.002, 3.0, 0.5, 0.9)
    chorus(p, JUNO1, 0.3)
    reverb(p, PLATE, 1.1, 2.2, 0.18)


@sound("KEYS", "Vibraphone Dusk", "vibes", "BALEARIC")
def _(p):
    keys_voice(p)
    osc_a(p, saw=False, pulse=True, pw=50)
    osc_b(p, saw=False, tri=True, semi=12)
    mixer(p, 0.2, 0.8)
    filt(p, 3200, 0.05, 0.1, REV3, key=2)
    adsr(p, "aenv", 0.001, 5.5, 0.0, 2.8)
    lfo(p, TRI1, rate=5.2)
    route(p, LFO, AMP, -0.2)
    reverb(p, PLATE, 1.2, 2.5, 0.2)


@sound("KEYS", "Nocturne Clavier", "clav", "AFTERHOURS")
def _(p):
    keys_voice(p)
    osc_a(p, saw=False, pulse=True, pw=18)
    osc_b(p, pulse=True, pw=30, semi=12)
    mixer(p, 0.75, 0.25)
    filt(p, 1500, 0.25, 0.4, MULTI, key=2, mode=LP, slope=1, vel=0.3)
    adsr(p, "fenv", 0.003, 0.6, 0.0, 0.3)
    adsr(p, "aenv", 0.003, 2.2, 0.5, 0.3)
    delay(p, "1/16D", 0.3, 0.14, pingpong=True)
    reverb(p, ROOM, 1.0, 1.4, 0.14)


@sound("KEYS", "Smoky Upright", "piano", "DEEP")
def _(p):
    keys_voice(p)
    osc_a(p)
    osc_b(p, saw=False, tri=True, semi=12, fine=2)
    mixer(p, 0.5, 0.55)
    filt(p, 1100, 0.05, 0.4, REV3, key=2, vel=0.5)
    adsr(p, "fenv", 0.001, 1.2, 0.1, 0.8)
    adsr(p, "aenv", 0.001, 4.0, 0.0, 0.9)
    p["analog_age"] = 0.45
    reverb(p, ROOM, 1.1, 1.8, 0.2)


@sound("KEYS", "Celestial Chimes", "chimes", "SUNRISE")
def _(p):
    keys_voice(p)
    osc_a(p, saw=False, pulse=True, pw=10)
    osc_b(p, saw=False, tri=True, semi=24)
    mixer(p, 0.35, 0.6)
    poly_mod(p, oscb=0.25)
    filt(p, 5500, 0.05, 0.1, REV3, key=2, hpf=300)
    adsr(p, "aenv", 0.001, 7.0, 0.0, 4.0)
    reverb(p, SHIMMER, 1.5, 5.0, 0.26, shimmer=0.35, pitch=3)


@sound("KEYS", "Sunday Stab", "organ_stab", "PROGRESSIVE")
def _(p):
    keys_voice(p)
    osc_a(p, saw=False, pulse=True, pw=45)
    osc_b(p, saw=False, pulse=True, pw=35, semi=12)
    mixer(p, 0.7, 0.5, sub=0.15)
    filt(p, 2500, 0.1, 0.3, REV3, key=2, vel=0.2)
    adsr(p, "fenv", 0.004, 0.5, 0.2, 0.2)
    adsr(p, "aenv", 0.003, 2.5, 0.6, 0.3)
    delay(p, "1/8D", 0.35, 0.18, pingpong=True)
    reverb(p, PLATE, 1.0, 1.8, 0.16)


@sound("KEYS", "Prism Chords", "sync_chord", "SUNRISE")
def _(p):
    keys_voice(p)
    osc_a(p, semi=12, sync=True)
    osc_b(p, fine=5)
    mixer(p, 0.55, 0.55)
    poly_mod(p, fenv=0.15)
    filt(p, 1800, 0.1, 0.35, REV3, key=2, vel=0.3)
    adsr(p, "fenv", 0.001, 1.2, 0.2, 0.6)
    adsr(p, "aenv", 0.002, 2.2, 0.3, 0.8)
    chorus(p, DIM, 0.3)
    reverb(p, HALL, 1.3, 2.8, 0.2)


@sound("KEYS", "Hushed Tines", "soft_ep", "DEEP")
def _(p):
    keys_voice(p)
    osc_a(p, saw=False, pulse=True, pw=28)
    osc_b(p, saw=False, tri=True)
    mixer(p, 0.55, 0.5)
    filt(p, 1400, 0.22, 0.35, REV3, key=2, vel=0.5)
    adsr(p, "fenv", 0.002, 2.0, 0.1, 1.0)
    adsr(p, "aenv", 0.003, 3.0, 0.15, 1.2)
    lfo(p, TRI1, rate=3.8)
    route(p, LFO, AMP, -0.1)
    chorus(p, JUNO1, 0.35, hiss=0.2)
    reverb(p, PLATE, 1.2, 2.5, 0.22, damp=0.6)


@sound("KEYS", "Late Hour Keys", "tape_keys", "DEEP")
def _(p):
    keys_voice(p)
    p["osc_model"] = 1
    osc_a(p)
    osc_b(p, saw=False, pulse=True, pw=40, fine=6)
    mixer(p, 0.7, 0.5)
    filt(p, 900, 0.18, 0.35, REV1, key=2, vel=0.35)
    adsr(p, "fenv", 0.002, 1.4, 0.2, 0.8)
    adsr(p, "aenv", 0.002, 2.5, 0.3, 0.9)
    echo(p, 8, 0.22, intensity=0.45, sync="1/8.", wow=0.35, flutter=0.25, sat=0.45, treble=-5)
    reverb(p, HALL, 1.3, 2.8, 0.18)


@sound("KEYS", "Harbor Chords", "pwm_keys", "PROGRESSIVE")
def _(p):
    keys_voice(p)
    osc_a(p, saw=False, pulse=True, pw=38)
    osc_b(p, saw=False, pulse=True, pw=44, fine=6)
    mixer(p, 0.7, 0.55)
    lfo2(p, TRI2, rate=0.5)
    route(p, LFO2, PW1, 0.25)
    filt(p, 1500, 0.12, 0.35, REV3, key=2, vel=0.3)
    adsr(p, "fenv", 0.002, 1.2, 0.25, 0.6)
    adsr(p, "aenv", 0.002, 1.8, 0.4, 0.6)
    chorus(p, JUNO2, 0.35)
    delay(p, "1/4D", 0.3, 0.14, pingpong=True)
    reverb(p, PLATE, 1.1, 2.2, 0.16)


# ============================================================================================== BRASS (15)
def brass_voice(p, n=8, detune=0.35, spread=0.6):
    voices(p, n, detune=detune, spread=spread)


@sound("BRASS", "Herald Brass", "section", "PROGRESSIVE")
def _(p):
    brass_voice(p)
    osc_a(p)
    osc_b(p, fine=7)
    mixer(p, 0.75, 0.6)
    filt(p, 850, 0.12, 0.5, REV3, key=2, vel=0.3)
    adsr(p, "fenv", 0.09, 2.0, 0.5, 0.8)
    adsr(p, "aenv", 0.04, 2.0, 0.9, 0.6)
    wheel_vibrato(p)
    reverb(p, HALL, 1.3, 2.6, 0.18)


@sound("BRASS", "Sunrise Fanfare", "bright", "SUNRISE")
def _(p):
    brass_voice(p)
    osc_a(p)
    osc_b(p, saw=True, pulse=True, pw=40, fine=6)
    mixer(p, 0.75, 0.55)
    filt(p, 1500, 0.1, 0.6, REV3, key=2, vel=0.3)
    adsr(p, "fenv", 0.02, 0.9, 0.4, 0.6)
    adsr(p, "aenv", 0.02, 2.0, 0.9, 0.5)
    wheel_vibrato(p)
    reverb(p, HALL, 1.3, 2.6, 0.18)


@sound("BRASS", "Velvet Horns", "soft_horn", "BALEARIC")
def _(p):
    brass_voice(p)
    p["osc_model"] = 1
    osc_a(p)
    osc_b(p, saw=False, pulse=True, pw=40, fine=6)
    mixer(p, 0.75, 0.55)
    filt(p, 600, 0.1, 0.4, REV1, key=2, vel=0.3)
    adsr(p, "fenv", 0.2, 2.6, 0.5, 1.2)
    adsr(p, "aenv", 0.12, 2.5, 0.85, 1.0)
    wheel_vibrato(p)
    reverb(p, HALL, 1.4, 3.2, 0.22)


@sound("BRASS", "Augur Stab", "stab", "PROGRESSIVE")
def _(p):
    brass_voice(p)
    osc_a(p)
    osc_b(p, fine=8)
    mixer(p, 0.75, 0.6, drive=0.35)
    filt(p, 1000, 0.12, 0.55, REV3, key=2, vel=0.3)
    adsr(p, "fenv", 0.005, 0.8, 0.2, 0.4)
    adsr(p, "aenv", 0.003, 1.2, 0.4, 0.5)
    wheel_vibrato(p)
    reverb(p, PLATE, 1.1, 2.0, 0.16)


@sound("BRASS", "Colossal Procession", "braam", "DEEP")
def _(p):
    brass_voice(p, 8, 0.4, 0.7)
    osc_a(p, octave=-1)
    osc_b(p, fine=8)
    mixer(p, 0.8, 0.7, sub=0.25, drive=0.5)
    filt(p, 380, 0.18, 0.55, REV3, key=1, vel=0.2)
    adsr(p, "fenv", 0.4, 5.0, 0.4, 3.0)
    adsr(p, "aenv", 0.2, 4.0, 1.0, 3.5)
    wheel_vibrato(p)
    reverb(p, HALL, 1.7, 6.0, 0.28, predelay=40)


@sound("BRASS", "Slow Banners", "swell", "SUNRISE")
def _(p):
    brass_voice(p)
    osc_a(p)
    osc_b(p, fine=8)
    mixer(p, 0.75, 0.6)
    filt(p, 700, 0.12, 0.5, REV3, key=2)
    adsr(p, "fenv", 1.2, 4.0, 0.6, 2.5)
    adsr(p, "aenv", 0.6, 3.0, 0.95, 2.5)
    wheel_vibrato(p)
    reverb(p, HALL, 1.5, 4.0, 0.24)


@sound("BRASS", "Gilded Section", "chorused", "BALEARIC")
def _(p):
    brass_voice(p)
    osc_a(p)
    osc_b(p, saw=False, pulse=True, pw=36, fine=8)
    mixer(p, 0.75, 0.6)
    filt(p, 950, 0.12, 0.45, REV3, key=2, vel=0.3)
    adsr(p, "fenv", 0.08, 2.2, 0.5, 0.9)
    adsr(p, "aenv", 0.05, 2.0, 0.9, 0.8)
    wheel_vibrato(p)
    chorus(p, JUNO12, 0.3)
    reverb(p, PLATE, 1.2, 2.4, 0.18)


@sound("BRASS", "Iron Crown", "dark_drive", "AFTERHOURS")
def _(p):
    brass_voice(p)
    p["osc_model"] = 1
    osc_a(p)
    osc_b(p, fine=-7)
    mixer(p, 0.8, 0.65, drive=0.65)
    filt(p, 650, 0.18, 0.45, REV1, key=2, vel=0.3)
    adsr(p, "fenv", 0.06, 2.0, 0.45, 0.8)
    adsr(p, "aenv", 0.03, 2.0, 0.9, 0.6)
    wheel_vibrato(p)
    reverb(p, ROOM, 1.1, 1.8, 0.16)


@sound("BRASS", "Amber March", "pwm_brass", "PROGRESSIVE")
def _(p):
    brass_voice(p)
    osc_a(p, saw=False, pulse=True, pw=35)
    osc_b(p, fine=7)
    mixer(p, 0.75, 0.6)
    lfo2(p, TRI2, rate=0.5)
    route(p, LFO2, PW1, 0.25)
    filt(p, 900, 0.12, 0.45, REV3, key=2, vel=0.3)
    adsr(p, "fenv", 0.07, 2.0, 0.5, 0.8)
    adsr(p, "aenv", 0.04, 2.0, 0.9, 0.7)
    wheel_vibrato(p)
    reverb(p, HALL, 1.3, 2.6, 0.18)


@sound("BRASS", "Distant Legion", "hall_brass", "DEEP")
def _(p):
    brass_voice(p)
    osc_a(p)
    osc_b(p, fine=9)
    mixer(p, 0.75, 0.6)
    filt(p, 700, 0.12, 0.45, REV3, key=2, vel=0.2)
    adsr(p, "fenv", 0.25, 3.0, 0.5, 2.5)
    adsr(p, "aenv", 0.15, 3.0, 0.9, 3.0)
    wheel_vibrato(p)
    reverb(p, HALL, 1.8, 7.0, 0.34, predelay=60)


@sound("BRASS", "Sentinel Stab", "delay_stab", "AFTERHOURS")
def _(p):
    brass_voice(p)
    osc_a(p)
    osc_b(p, saw=True, pulse=True, pw=45, fine=6)
    mixer(p, 0.75, 0.55, drive=0.3)
    filt(p, 1100, 0.12, 0.5, REV3, key=2, vel=0.3)
    adsr(p, "fenv", 0.004, 0.7, 0.15, 0.3)
    adsr(p, "aenv", 0.003, 1.0, 0.3, 0.4)
    wheel_vibrato(p)
    delay(p, "1/8D", 0.45, 0.22, pingpong=True)
    reverb(p, PLATE, 1.1, 2.0, 0.16)


@sound("BRASS", "Copper Anthem", "polymod_brass", "SUNRISE")
def _(p):
    brass_voice(p)
    osc_a(p, saw=False, pulse=True, pw=45)
    osc_b(p, fine=7)
    mixer(p, 0.75, 0.6)
    poly_mod(p, fenv=0.3, freq=False, pw=True)
    filt(p, 1300, 0.25, 0.5, REV3, key=2, vel=0.3)
    adsr(p, "fenv", 0.08, 2.0, 0.5, 0.8)
    adsr(p, "aenv", 0.04, 2.0, 0.9, 0.7)
    wheel_vibrato(p)
    reverb(p, HALL, 1.3, 2.6, 0.18)


@sound("BRASS", "Saffron Horns", "sync_brass", "PROGRESSIVE")
def _(p):
    brass_voice(p)
    osc_a(p, semi=5, sync=True)
    osc_b(p, fine=6)
    mixer(p, 0.65, 0.6)
    poly_mod(p, fenv=0.2)
    filt(p, 1300, 0.1, 0.4, REV3, key=2, vel=0.3)
    adsr(p, "fenv", 0.06, 1.8, 0.45, 0.7)
    adsr(p, "aenv", 0.03, 2.0, 0.9, 0.6)
    wheel_vibrato(p)
    reverb(p, HALL, 1.3, 2.6, 0.18)


@sound("BRASS", "Evening Proclamation", "soft_pad_brass", "BALEARIC")
def _(p):
    brass_voice(p, 8, 0.4, 0.7)
    osc_a(p)
    osc_b(p, saw=False, pulse=True, pw=42, fine=8)
    mixer(p, 0.7, 0.6)
    filt(p, 600, 0.1, 0.4, REV3, key=2)
    adsr(p, "fenv", 0.5, 3.5, 0.55, 2.0)
    adsr(p, "aenv", 0.3, 3.0, 0.9, 2.0)
    wheel_vibrato(p)
    chorus(p, JUNO1, 0.3)
    reverb(p, HALL, 1.5, 4.0, 0.26)


@sound("BRASS", "Monolith Brass", "octave_brass", "AFTERHOURS")
def _(p):
    brass_voice(p)
    osc_a(p)
    osc_b(p, octave=-1, fine=6)
    mixer(p, 0.75, 0.6, drive=0.35)
    filt(p, 800, 0.14, 0.5, REV3, key=2, vel=0.3)
    adsr(p, "fenv", 0.06, 2.0, 0.45, 0.8)
    adsr(p, "aenv", 0.03, 2.0, 0.9, 0.7)
    wheel_vibrato(p)
    reverb(p, HALL, 1.3, 2.6, 0.18)


# ============================================================================================== TEXTURE (15)
def texture_voice(p, n=8, detune=0.45, spread=0.85):
    voices(p, n, detune=detune, spread=spread)


@sound("TEXTURE", "Wind Over Templum", "noise_wash", "DEEP")
def _(p):
    texture_voice(p)
    osc_a(p)
    osc_b(p, saw=False, tri=True, octave=-1)
    mixer(p, 0.2, 0.3, noise=0.45)
    filt(p, 750, 0.28, 0.1, MULTI, key=1, mode=BP, slope=1)
    lfo2(p, SMOOTH2, rate=0.2)
    route(p, LFO2, CUT, 0.08)
    adsr(p, "aenv", 1.0, 4.0, 0.9, 4.0)
    phaser(p, 0.35, rate=0.08, stages=2, fb=0.4)
    reverb(p, HALL, 1.6, 6.0, 0.34)


@sound("TEXTURE", "Tape Feathers", "tape_dust", "BALEARIC")
def _(p):
    texture_voice(p)
    osc_a(p)
    osc_b(p, fine=14)
    mixer(p, 0.6, 0.5, noise=0.07)
    filt(p, 1300, 0.15, 0.15, REV3, key=1)
    drift(p, 12)
    adsr(p, "aenv", 0.8, 4.0, 0.85, 4.0)
    echo(p, 8, 0.3, intensity=0.5, time_ms=330, wow=0.6, flutter=0.4, sat=0.5, age=0.6, treble=-5)
    chorus(p, JUNO1, 0.35, hiss=0.45)
    reverb(p, HALL, 1.5, 5.0, 0.28)


@sound("TEXTURE", "Distant Radio", "crushed", "AFTERHOURS")
def _(p):
    texture_voice(p)
    osc_a(p, saw=False, pulse=True, pw=30)
    osc_b(p, saw=False, pulse=True, pw=50, semi=7)
    mixer(p, 0.55, 0.45, noise=0.05)
    filt(p, 2000, 0.15, 0.15, MULTI, key=1, mode=BP, slope=1)
    lfo2(p, SMOOTH2, rate=0.5)
    route(p, LFO2, CUT, 0.06)
    adsr(p, "aenv", 0.6, 4.0, 0.85, 3.5)
    drive(p, CRUSH, 22.0, tone=0.45, mix=0.28)
    reverb(p, PLATE, 1.3, 3.5, 0.28)


@sound("TEXTURE", "Rain on Wings", "sh_bubbles", "PROGRESSIVE")
def _(p):
    texture_voice(p)
    osc_a(p, saw=False, pulse=True, pw=30)
    osc_b(p, saw=False, tri=True, semi=12)
    mixer(p, 0.55, 0.5)
    filt(p, 1100, 0.32, 0.15, REV3, key=1)
    lfo2(p, SH2, div="1/16")
    route(p, LFO2, CUT, 0.12)
    adsr(p, "aenv", 0.5, 4.0, 0.85, 3.5)
    delay(p, "1/16", 0.38, 0.22, pingpong=True)
    reverb(p, HALL, 1.5, 4.5, 0.28)


@sound("TEXTURE", "Sediment", "flutter", "DEEP")
def _(p):
    texture_voice(p)
    osc_a(p, saw=False, pulse=True, pw=15)
    osc_b(p, saw=False, tri=True, octave=1)
    mixer(p, 0.4, 0.4, noise=0.3)
    filt(p, 1500, 0.2, 0.15, MULTI, key=1, mode=BP, slope=1)
    lfo2(p, SH2, rate=14.0)
    route(p, LFO2, AMP, -0.7)
    route(p, LFO2, CUT, 0.06)
    adsr(p, "aenv", 0.6, 4.0, 0.85, 3.5)
    delay(p, "1/16", 0.4, 0.22, pingpong=True)
    reverb(p, HALL, 1.6, 5.0, 0.3)


@sound("TEXTURE", "Phase Wind", "phase_wind", "BALEARIC")
def _(p):
    texture_voice(p)
    osc_a(p, saw=False, pulse=True, pw=20)
    osc_b(p, saw=False, tri=True, octave=-1)
    mixer(p, 0.3, 0.3, noise=0.45)
    filt(p, 2200, 0.2, 0.1, REV3, key=1)
    adsr(p, "aenv", 1.2, 4.0, 0.9, 4.5)
    phaser(p, 0.5, rate=0.06, stages=3, fb=0.6)
    reverb(p, HALL, 1.6, 6.0, 0.32)


@sound("TEXTURE", "Spring Drips", "spring_drips", "BALEARIC")
def _(p):
    texture_voice(p, 6)
    osc_a(p, saw=False, pulse=True, pw=15)
    osc_b(p, saw=False, tri=True, semi=19)
    mixer(p, 0.4, 0.5)
    filt(p, 2800, 0.15, 0.35, REV3, key=2)
    adsr(p, "fenv", 0.001, 0.4, 0.0, 0.5)
    adsr(p, "aenv", 0.001, 1.0, 0.0, 1.2)
    lfo2(p, SH2, div="1/8")
    route(p, LFO2, F2, pitch_amt(7))
    reverb(p, SPRING, 1.2, 3.0, 0.38)
    echo(p, 9, 0.24, intensity=0.45, sync="1/8.", spring=0.5)


@sound("TEXTURE", "Murmuring Sky", "formant", "DEEP")
def _(p):
    texture_voice(p)
    osc_a(p, saw=False, pulse=True, pw=25)
    osc_b(p, fine=10)
    mixer(p, 0.6, 0.45, noise=0.12)
    filt(p, 900, 0.3, 0.1, MULTI, key=1, mode=BP, slope=1)
    lfo2(p, SMOOTH2, rate=0.6)
    route(p, LFO2, CUT, 0.1)
    adsr(p, "aenv", 1.0, 4.0, 0.9, 4.0)
    chorus(p, ENS, 0.4, rate=0.6)
    reverb(p, HALL, 1.6, 6.0, 0.32)


@sound("TEXTURE", "Static Halo", "ring_metal", "AFTERHOURS")
def _(p):
    texture_voice(p)
    osc_a(p)
    osc_b(p, saw=False, tri=True, semi=11, fine=18)
    mixer(p, 0.15, 0.2, ring=0.6)
    filt(p, 3000, 0.1, 0.1, REV3, key=2, hpf=200)
    adsr(p, "aenv", 0.8, 4.0, 0.85, 4.0)
    reverb(p, SPRING, 1.0, 2.5, 0.3)


@sound("TEXTURE", "Frost Particles", "sh_high", "SUNRISE")
def _(p):
    texture_voice(p)
    osc_a(p, saw=False, pulse=True, pw=12)
    osc_b(p, saw=False, tri=True, semi=24)
    mixer(p, 0.35, 0.5, noise=0.05)
    filt(p, 3200, 0.25, 0.1, REV3, key=2, hpf=300)
    lfo2(p, SH2, rate=7.0)
    route(p, LFO2, CUT, 0.14)
    adsr(p, "aenv", 0.8, 4.0, 0.85, 4.0)
    delay(p, "1/16", 0.35, 0.2, pingpong=True)
    reverb(p, SHIMMER, 1.7, 6.0, 0.32, shimmer=0.4, pitch=3)


@sound("TEXTURE", "Dust of Suns", "hiss_pad", "BALEARIC")
def _(p):
    texture_voice(p)
    osc_a(p, saw=False, pulse=True, pw=40)
    osc_b(p, saw=False, tri=True, fine=6)
    mixer(p, 0.45, 0.6, noise=0.1)
    filt(p, 1000, 0.1, 0.1, REV3, key=1)
    drift(p, 9)
    adsr(p, "aenv", 1.0, 4.0, 0.9, 4.0)
    chorus(p, JUNO2, 0.4, hiss=0.6)
    reverb(p, PLATE, 1.5, 4.5, 0.3, damp=0.6)


@sound("TEXTURE", "Engine Hum", "hum", "AFTERHOURS")
def _(p):
    texture_voice(p, 6, 0.3, 0.6)
    osc_a(p, octave=-1)
    osc_b(p, saw=False, pulse=True, pw=20, fine=4)
    mixer(p, 0.55, 0.45, noise=0.06, drive=0.35)
    filt(p, 500, 0.3, 0.1, REV1, key=1)
    lfo2(p, SINE2, div="1/2")
    route(p, LFO2, CUT, 0.08)
    adsr(p, "aenv", 0.8, 4.0, 0.9, 3.0)
    phaser(p, 0.3, rate=0.1, stages=1, fb=0.4)
    reverb(p, HALL, 1.4, 4.0, 0.24)


@sound("TEXTURE", "Lichen", "polymod_drift", "DEEP")
def _(p):
    texture_voice(p)
    osc_a(p, saw=True, pulse=True, pw=40)
    osc_b(p, saw=False, tri=True, semi=7)
    mixer(p, 0.65, 0.3)
    poly_mod(p, oscb=0.14, pw=True)
    filt(p, 1500, 0.15, 0.1, REV3, key=1)
    drift(p, 10, 0.08)
    adsr(p, "aenv", 1.2, 4.0, 0.9, 4.5)
    reverb(p, HALL, 1.6, 6.0, 0.32)


@sound("TEXTURE", "Evening Insects", "rhythmic_sh", "PROGRESSIVE")
def _(p):
    texture_voice(p, 6)
    osc_a(p, saw=False, pulse=True, pw=20)
    osc_b(p, saw=False, tri=True, semi=12)
    mixer(p, 0.5, 0.45, noise=0.08)
    filt(p, 1800, 0.3, 0.1, MULTI, key=1, mode=BP, slope=1)
    lfo2(p, SH2, div="1/16")
    route(p, LFO2, CUT, 0.16)
    route(p, LFO2, AMP, -0.35)
    adsr(p, "aenv", 0.5, 4.0, 0.85, 3.0)
    delay(p, "1/8D", 0.35, 0.18, pingpong=True)
    reverb(p, HALL, 1.4, 4.0, 0.26)


@sound("TEXTURE", "Ocean Grain", "waves", "BALEARIC")
def _(p):
    texture_voice(p)
    osc_a(p)
    osc_b(p, saw=False, tri=True, octave=-1)
    mixer(p, 0.15, 0.25, noise=0.6)
    filt(p, 1400, 0.12, 0.1, REV3, key=0)
    lfo(p, TRI1, rate=0.12)
    route(p, LFO, AMP, -0.45)
    route(p, LFO, CUT, 0.12)
    adsr(p, "aenv", 1.5, 4.0, 0.9, 5.0)
    reverb(p, HALL, 1.7, 7.0, 0.34)


# ============================================================================================== DRONE (15)
def drone_voice(p, n=6, detune=0.45, spread=0.8):
    voices(p, n, detune=detune, spread=spread)


@sound("DRONE", "Monolith Drone", "dark_saw", "DEEP")
def _(p):
    drone_voice(p)
    p.update(osc_model=1, analog_age=0.5)
    osc_a(p)
    osc_b(p, octave=-1, fine=8)
    mixer(p, 0.7, 0.6, sub=0.3, drive=0.35)
    filt(p, 420, 0.25, 0.15, REV1, key=1)
    lfo2(p, SMOOTH2, rate=0.08)
    route(p, LFO2, CUT, 0.06)
    adsr(p, "aenv", 2.0, 4.0, 1.0, 5.0)
    reverb(p, HALL, 1.8, 8.0, 0.32, predelay=50)


@sound("DRONE", "Fifth of Dusk", "fifth", "PROGRESSIVE")
def _(p):
    drone_voice(p)
    osc_a(p)
    osc_b(p, semi=7, fine=3)
    mixer(p, 0.7, 0.55)
    filt(p, 900, 0.2, 0.15, REV3, key=1)
    lfo(p, TRI1, rate=0.07)
    route(p, LFO, CUT, 0.07)
    drift(p, 6)
    adsr(p, "aenv", 2.0, 4.0, 1.0, 5.0)
    reverb(p, HALL, 1.7, 8.0, 0.32)


@sound("DRONE", "Shimmer Continuum", "shimmer", "SUNRISE")
def _(p):
    drone_voice(p)
    osc_a(p, saw=False, pulse=True, pw=22)
    osc_b(p, saw=False, tri=True, octave=1)
    mixer(p, 0.45, 0.55)
    filt(p, 2600, 0.08, 0.1, REV3, key=1, hpf=150)
    drift(p, 5)
    adsr(p, "aenv", 2.5, 4.0, 1.0, 6.0)
    reverb(p, SHIMMER, 1.9, 10.0, 0.42, shimmer=0.5, pitch=3)


@sound("DRONE", "Abyssal Hum", "sub_noise", "DEEP")
def _(p):
    drone_voice(p, 4, 0.3, 0.6)
    osc_a(p)
    osc_b(p, saw=False, tri=True, octave=-1)
    mixer(p, 0.4, 0.8, noise=0.15, drive=0.3)
    filt(p, 320, 0.2, 0.1, REV3, key=1)
    lfo2(p, SMOOTH2, rate=0.06)
    route(p, LFO2, CUT, 0.05)
    adsr(p, "aenv", 2.0, 4.0, 1.0, 5.0)
    reverb(p, HALL, 1.6, 7.0, 0.28, lowcut=200)


@sound("DRONE", "Beating Stillness", "beating", "BALEARIC")
def _(p):
    drone_voice(p)
    osc_a(p, saw=True, pulse=True, pw=45)
    osc_b(p, saw=False, tri=True, fine=3)
    mixer(p, 0.6, 0.6)
    filt(p, 1600, 0.1, 0.1, REV3, key=1)
    adsr(p, "aenv", 2.0, 4.0, 1.0, 5.0)
    chorus(p, ENS, 0.42, rate=0.5)
    reverb(p, HALL, 1.7, 8.0, 0.32)


@sound("DRONE", "Solstice", "evolving", "PROGRESSIVE")
def _(p):
    drone_voice(p)
    osc_a(p)
    osc_b(p, saw=False, pulse=True, pw=40, fine=6)
    mixer(p, 0.65, 0.55)
    filt(p, 800, 0.22, 0.1, REV3, key=1)
    lfo2(p, SINE2, div="8/1")
    route(p, LFO2, CUT, 0.2)
    route(p, LFO2, PW2, 0.2)
    adsr(p, "aenv", 2.0, 4.0, 1.0, 5.0)
    reverb(p, HALL, 1.8, 8.0, 0.34, predelay=45)


@sound("DRONE", "Glacier", "cold_high", "SUNRISE")
def _(p):
    drone_voice(p)
    osc_a(p, saw=False, pulse=True, pw=12)
    osc_b(p, saw=False, tri=True, semi=12, fine=4)
    mixer(p, 0.35, 0.6)
    filt(p, 3500, 0.12, 0.1, MULTI, key=1, slope=1, hpf=250)
    drift(p, 4)
    adsr(p, "aenv", 2.5, 4.0, 1.0, 6.0)
    reverb(p, HALL, 2.0, 12.0, 0.4, damp=0.3)


@sound("DRONE", "Meridian Organ", "organ", "BALEARIC")
def _(p):
    drone_voice(p, 6, 0.25, 0.6)
    osc_a(p, saw=False, pulse=True, pw=50)
    osc_b(p, saw=False, pulse=True, pw=50, semi=12, fine=3)
    mixer(p, 0.6, 0.45, sub=0.3)
    filt(p, 2400, 0.05, 0.0, REV3, key=1)
    adsr(p, "aenv", 1.5, 4.0, 1.0, 4.0)
    phaser(p, 0.3, rate=0.15, stages=1, fb=0.3)
    reverb(p, HALL, 1.7, 7.0, 0.32)


@sound("DRONE", "Desert Wind", "noise_drone", "DEEP")
def _(p):
    drone_voice(p)
    osc_a(p)
    osc_b(p, saw=False, tri=True, octave=-1)
    mixer(p, 0.7, 0.7, noise=0.35, drive=0.35)
    filt(p, 900, 0.3, 0.1, MULTI, key=1, mode=BP, slope=1)
    lfo(p, TRI1, rate=0.05)
    route(p, LFO, CUT, 0.13)
    adsr(p, "aenv", 2.0, 4.0, 1.0, 5.0)
    phaser(p, 0.35, rate=0.05, stages=2, fb=0.5)
    reverb(p, HALL, 1.7, 8.0, 0.34)


@sound("DRONE", "Iron Undertone", "gritty", "AFTERHOURS")
def _(p):
    drone_voice(p, 4, 0.35, 0.6)
    osc_a(p)
    osc_b(p, fine=-9)
    mixer(p, 0.75, 0.65, drive=0.55)
    filt(p, 600, 0.28, 0.1, REV3, key=1)
    lfo2(p, TRI2, div="2/1")
    route(p, LFO2, CUT, 0.08)
    adsr(p, "aenv", 1.5, 4.0, 1.0, 4.0)
    drive(p, TUBE, 12.0, tone=0.5, mix=0.4)
    reverb(p, HALL, 1.5, 5.0, 0.26)


@sound("DRONE", "Eclipse", "phased_dark", "DEEP")
def _(p):
    drone_voice(p)
    p["osc_model"] = 1
    osc_a(p, saw=True, pulse=True, pw=35)
    osc_b(p, octave=-1, fine=7)
    mixer(p, 0.65, 0.55)
    filt(p, 550, 0.25, 0.1, REV1, key=1)
    adsr(p, "aenv", 2.0, 4.0, 1.0, 5.0)
    phaser(p, 0.45, rate=0.04, stages=3, fb=0.55)
    reverb(p, HALL, 1.8, 9.0, 0.34, predelay=60)


@sound("DRONE", "Tundra Choir", "choir", "SUNRISE")
def _(p):
    drone_voice(p)
    osc_a(p, saw=False, pulse=True, pw=28)
    osc_b(p, octave=1, fine=7)
    mixer(p, 0.7, 0.35, noise=0.05)
    filt(p, 1200, 0.22, 0.1, REV3, key=1)
    lfo2(p, SMOOTH2, rate=0.3)
    route(p, LFO2, CUT, 0.04)
    adsr(p, "aenv", 2.0, 4.0, 1.0, 5.0)
    chorus(p, ENS, 0.45, rate=0.6)
    reverb(p, HALL, 1.8, 9.0, 0.36)


@sound("DRONE", "Zenith Pulse", "pulsing", "PROGRESSIVE")
def _(p):
    drone_voice(p)
    osc_a(p)
    osc_b(p, fine=7)
    mixer(p, 0.7, 0.55)
    filt(p, 1000, 0.2, 0.1, REV3, key=1)
    lfo(p, SQR1, div="1/8")
    route(p, LFO, AMP, -0.4)
    route(p, LFO, CUT, 0.06)
    adsr(p, "aenv", 1.5, 4.0, 1.0, 4.0)
    delay(p, "1/8D", 0.3, 0.16, pingpong=True)
    reverb(p, HALL, 1.6, 6.0, 0.3)


@sound("DRONE", "Expanse", "wide_unison", "SUNRISE")
def _(p):
    voices(p, 6, unison=True, detune=0.55, spread=0.9)
    osc_a(p)
    osc_b(p, saw=False, tri=True, octave=1)
    mixer(p, 0.65, 0.5)
    filt(p, 1400, 0.12, 0.1, REV3, key=1)
    adsr(p, "aenv", 2.5, 4.0, 1.0, 6.0)
    reverb(p, HALL, 1.9, 10.0, 0.38)


@sound("DRONE", "Votive Flame", "tape_drone", "BALEARIC")
def _(p):
    drone_voice(p)
    osc_a(p, saw=False, pulse=True, pw=40)
    osc_b(p, fine=7)
    mixer(p, 0.6, 0.55, noise=0.04)
    filt(p, 1100, 0.15, 0.1, REV3, key=1)
    drift(p, 9)
    adsr(p, "aenv", 2.0, 4.0, 1.0, 5.0)
    echo(p, 8, 0.24, intensity=0.45, time_ms=420, wow=0.5, flutter=0.3, sat=0.5, age=0.6, treble=-6)
    reverb(p, HALL, 1.6, 6.0, 0.3)


# ============================================================================================== FX (15)
def fx_voice(p, n=4):
    voices(p, n, detune=0.4, spread=0.8)


@sound("FX", "Rising Augury", "riser", "PROGRESSIVE")
def _(p):
    fx_voice(p)
    osc_a(p)
    osc_b(p, fine=10)
    mixer(p, 0.5, 0.5, noise=0.35)
    adsr(p, "menv", 3.0, 1.0, 1.0, 1.0)
    route(p, MENV, F1, pitch_amt(12))
    route(p, MENV, F2, pitch_amt(12))
    route(p, MENV, CUT, 0.45)
    filt(p, 600, 0.3, 0.0, REV3, key=1)
    adsr(p, "aenv", 0.5, 4.0, 1.0, 2.5)
    reverb(p, HALL, 1.7, 5.0, 0.32)


@sound("FX", "Downlifter of Omens", "fall", "DEEP")
def _(p):
    fx_voice(p)
    osc_a(p)
    osc_b(p, saw=False, pulse=True, pw=40, fine=8)
    mixer(p, 0.55, 0.5, noise=0.15)
    adsr(p, "menv", 0.001, 3.0, 0.0, 1.0)
    route(p, MENV, F1, pitch_amt(19))
    route(p, MENV, F2, pitch_amt(19))
    route(p, MENV, CUT, 0.3)
    filt(p, 1200, 0.2, 0.0, REV3, key=2)
    adsr(p, "aenv", 0.005, 4.0, 1.0, 2.5)
    reverb(p, HALL, 1.7, 6.0, 0.34)


@sound("FX", "Wing Sweep", "noise_sweep", "PROGRESSIVE")
def _(p):
    fx_voice(p)
    osc_a(p)
    osc_b(p)
    mixer(p, 0.0, 0.0, noise=1.0)
    adsr(p, "menv", 2.5, 1.0, 1.0, 2.0)
    route(p, MENV, CUT, 0.45)
    filt(p, 350, 0.45, 0.0, MULTI, key=0, mode=BP, slope=1)
    adsr(p, "aenv", 0.5, 4.0, 1.0, 2.5)
    phaser(p, 0.35, rate=0.2, stages=2, fb=0.4)
    reverb(p, HALL, 1.6, 5.0, 0.3)


@sound("FX", "Impact of Dawn", "impact", "SUNRISE")
def _(p):
    fx_voice(p)
    osc_a(p, octave=-2)
    osc_b(p, octave=-1, fine=12)
    mixer(p, 0.6, 0.6, noise=0.55, drive=0.5)
    adsr(p, "aenv", 0.001, 3.0, 0.0, 2.0)
    adsr(p, "fenv", 0.001, 1.5, 0.0, 1.0)
    filt(p, 700, 0.2, 0.55, REV3, key=1)
    reverb(p, HALL, 2.0, 8.0, 0.4)


@sound("FX", "Laser Feather", "laser", "AFTERHOURS")
def _(p):
    fx_voice(p)
    osc_a(p, semi=12, sync=True)
    osc_b(p)
    mixer(p, 0.75, 0.2)
    adsr(p, "menv", 0.001, 0.7, 0.0, 0.3)
    route(p, MENV, F1, pitch_amt(20))
    adsr(p, "aenv", 0.001, 1.6, 0.0, 1.0)
    filt(p, 3500, 0.15, 0.1, REV3, key=2)
    echo(p, 3, 0.3, intensity=0.55, sync="1/8.")
    reverb(p, HALL, 1.4, 3.0, 0.22)
    bus_comp(p, -20, "4", 3, 0.3, 8.0)


@sound("FX", "Distant Siren", "siren", "DEEP")
def _(p):
    fx_voice(p)
    osc_a(p, saw=False, pulse=True, pw=50)
    osc_b(p, saw=False, tri=True, semi=12)
    mixer(p, 0.55, 0.45)
    lfo(p, TRI1, rate=0.6)
    route(p, LFO, F1, pitch_amt(5))
    route(p, LFO, F2, pitch_amt(5))
    filt(p, 2200, 0.2, 0.1, REV3, key=2)
    adsr(p, "aenv", 0.4, 4.0, 1.0, 2.5)
    delay(p, "1/4D", 0.4, 0.22, pingpong=True)
    reverb(p, HALL, 1.7, 6.0, 0.34)


@sound("FX", "Transmission", "sci", "AFTERHOURS")
def _(p):
    fx_voice(p)
    osc_a(p, saw=False, pulse=True, pw=25)
    osc_b(p, saw=False, tri=True, semi=7)
    mixer(p, 0.3, 0.2, ring=0.6)
    lfo2(p, SH2, div="1/16")
    route(p, LFO2, F2, pitch_amt(10))
    filt(p, 2600, 0.2, 0.1, REV3, key=2)
    adsr(p, "aenv", 0.01, 4.0, 1.0, 2.0)
    delay(p, "1/16", 0.45, 0.28, pingpong=True)
    reverb(p, PLATE, 1.3, 3.0, 0.24)


@sound("FX", "White Horizon", "white_riser", "SUNRISE")
def _(p):
    fx_voice(p)
    osc_a(p)
    osc_b(p)
    mixer(p, 0.0, 0.0, noise=1.0)
    adsr(p, "menv", 3.0, 1.0, 1.0, 1.5)
    route(p, MENV, CUT, 0.7)
    filt(p, 300, 0.35, 0.0, MULTI, key=0, mode=LP, slope=1)
    adsr(p, "aenv", 2.5, 4.0, 1.0, 2.0)
    reverb(p, HALL, 1.7, 5.0, 0.32)


@sound("FX", "Shockwave", "sub_drop", "DEEP")
def _(p):
    fx_voice(p, 2)
    osc_a(p, saw=False, pulse=True, pw=50)
    osc_b(p, saw=False, tri=True, octave=-1)
    mixer(p, 0.3, 1.0, noise=0.2, drive=0.4)
    adsr(p, "menv", 0.001, 1.6, 0.0, 1.0)
    route(p, MENV, F1, pitch_amt(14))
    route(p, MENV, F2, pitch_amt(14))
    filt(p, 600, 0.12, 0.3, REV3, key=1)
    adsr(p, "fenv", 0.001, 0.6, 0.0, 0.5)
    adsr(p, "aenv", 0.001, 3.5, 0.0, 2.0)
    reverb(p, HALL, 1.8, 6.0, 0.28, lowcut=150)


@sound("FX", "Portent Swell", "swell", "PROGRESSIVE")
def _(p):
    fx_voice(p, 6)
    osc_a(p)
    osc_b(p, saw=False, tri=True, semi=7, fine=6)
    mixer(p, 0.6, 0.5, noise=0.1)
    filt(p, 900, 0.2, 0.4, REV3, key=1)
    adsr(p, "fenv", 2.8, 2.0, 0.6, 3.0)
    adsr(p, "aenv", 2.8, 4.0, 1.0, 4.0)
    reverb(p, SHIMMER, 1.8, 8.0, 0.36, shimmer=0.45, pitch=3)
    bus_comp(p, -18, "3", 10, 0.6, 5.0)


@sound("FX", "Comet Pass", "whoosh", "AFTERHOURS")
def _(p):
    fx_voice(p)
    osc_a(p)
    osc_b(p, fine=15)
    mixer(p, 0.3, 0.3, noise=0.9)
    lfo(p, TRI1, rate=0.3)
    route(p, LFO, CUT, 0.2)
    filt(p, 1500, 0.25, 0.0, REV3, key=0)
    adsr(p, "aenv", 1.0, 4.0, 1.0, 2.5)
    flanger(p, 0.5, rate=0.15, depth=0.8, fb=0.6)
    reverb(p, HALL, 1.6, 5.0, 0.3)
    bus_comp(p, -18, "3", 10, 0.3, 6.0)


@sound("FX", "Thunder Wings", "rumble", "DEEP")
def _(p):
    fx_voice(p)
    osc_a(p, octave=-2)
    osc_b(p, saw=False, tri=True, octave=-1, fine=20)
    mixer(p, 0.4, 0.5, noise=0.5, drive=0.45)
    filt(p, 300, 0.25, 0.0, REV1, key=0)
    lfo2(p, SMOOTH2, rate=0.4)
    route(p, LFO2, CUT, 0.12)
    route(p, LFO2, AMP, -0.3)
    adsr(p, "aenv", 0.3, 4.0, 1.0, 3.0)
    reverb(p, HALL, 1.9, 8.0, 0.34, lowcut=120)


@sound("FX", "Signal Lost", "glitch", "AFTERHOURS")
def _(p):
    fx_voice(p)
    osc_a(p, saw=False, pulse=True, pw=30)
    osc_b(p, saw=False, pulse=True, pw=50, semi=5)
    mixer(p, 0.55, 0.45)
    lfo2(p, SH2, div="1/32")
    route(p, LFO2, F1, pitch_amt(12))
    route(p, LFO2, AMP, -0.4)
    filt(p, 2800, 0.15, 0.1, REV3, key=2)
    adsr(p, "aenv", 0.005, 4.0, 1.0, 1.5)
    drive(p, CRUSH, 20.0, tone=0.45, mix=0.3)
    delay(p, "1/16", 0.4, 0.22, pingpong=True)
    reverb(p, PLATE, 1.2, 2.5, 0.2)


@sound("FX", "Lift of Thermals", "accelerating", "SUNRISE")
def _(p):
    fx_voice(p)
    osc_a(p)
    osc_b(p, saw=False, tri=True, semi=12)
    mixer(p, 0.5, 0.45, noise=0.25)
    lfo(p, SQR1, rate=2.0)
    route(p, LFO, AMP, -0.5)
    adsr(p, "menv", 3.0, 1.0, 1.0, 1.0)
    route(p, MENV, LFORATE, 0.6)
    route(p, MENV, CUT, 0.4)
    route(p, MENV, F1, pitch_amt(7))
    filt(p, 700, 0.25, 0.0, REV3, key=1)
    adsr(p, "aenv", 0.3, 4.0, 1.0, 2.0)
    reverb(p, HALL, 1.6, 5.0, 0.3)


@sound("FX", "Sky Collapse", "long_fall", "DEEP")
def _(p):
    fx_voice(p, 6)
    osc_a(p)
    osc_b(p, fine=-12)
    mixer(p, 0.4, 0.4, noise=0.4)
    adsr(p, "menv", 0.001, 4.0, 0.0, 2.0)
    route(p, MENV, CUT, 0.5)
    route(p, MENV, F1, pitch_amt(7))
    route(p, MENV, F2, pitch_amt(7))
    filt(p, 400, 0.25, 0.0, REV3, key=1)
    adsr(p, "aenv", 0.005, 4.0, 1.0, 3.0)
    reverb(p, HALL, 2.0, 10.0, 0.4)
    bus_comp(p, -18, "3", 10, 0.6, 4.0)


# ============================================================================================== files
def build(entry):
    p = base(entry["cat"])
    entry["fn"](p)
    if entry["cat"] == "BASS":
        spatial = [k for k in atlas.SPATIAL if p.get(k)]
        assert not spatial and p["voice_spread"] == 0.0, entry["name"] + ": a bass stays mono and dry"
    wheel_used = any(p.get("mm%d_src" % i) == WHEEL and p.get("mm%d_amt" % i) for i in range(1, 9))
    if entry["cat"] in ("LEAD", "BRASS") and not p.get("wm_freq_a") and not wheel_used:
        route(p, WHEEL, CUT, 0.22)
    return p


def xml_for(entry, p):
    v = atlas.finish(p)
    lines = ['<?xml version="1.0" encoding="UTF-8"?>',
             '<AUGUR5_PRESET formatVersion="1" name=%s>' % quoteattr(entry["name"]),
             '  <META role="%s" note="%d" pack=%s category="%s" family="%s" kind="%s" author="TONAL LAB"/>'
             % (entry["role"], entry["note"], quoteattr(PACK), entry["cat"], entry["family"], entry["kind"]),
             "  <PARAMS>"]
    for k in sorted(v):
        x = v[k]
        lines.append('    <P id="%s" value="%s"/>' % (k, ("%d" % x) if float(x).is_integer() else ("%.5g" % x)))
    lines += ["  </PARAMS>", "</AUGUR5_PRESET>"]
    return "\n".join(lines) + "\n"


def write(entry, folder, note=None):
    e = dict(entry, note=note or entry["note"])
    path = folder / entry["cat"] / ("%s.augur5" % entry["name"])
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(xml_for(e, entry["p"]), encoding="utf-8")
    return path


# ============================================================================================== audit
def audit_dir(args):
    folder, wav, reverse, dry = args
    env_ = dict(os.environ)
    if reverse:
        env_["AUGUR_AUDIT_REVERSE"] = "1"
    cmd = [str(AUDIT), "--pack", str(folder)] + (["--wav", str(wav)] if wav else []) + (["--dry"] if dry else [])
    out = subprocess.run(cmd, capture_output=True, text=True, cwd=str(ROOT), env=env_).stdout
    rows = {}
    for row in csv.DictReader(io.StringIO(out), delimiter=";"):
        m = {k: (float(v) if k not in ("path", "role", "fp", "fpdry") else v) for k, v in row.items()}
        m["fp"] = timbre.parse(row.get("fp", ""))
        m["fpdry"] = timbre.parse(row.get("fpdry", ""))
        rows[(folder / row["path"]).resolve()] = m
    return rows


def audit(dirs, wav_root=None, reverse=False, dry=False):
    rows = {}
    jobs = [(d, (wav_root / d.name) if wav_root else None, reverse, dry) for d in dirs]
    with concurrent.futures.ThreadPoolExecutor(max_workers=JOBS) as ex:
        for part in ex.map(audit_dir, jobs):
            rows.update(part)
    return rows


def level(entry, m):
    """LEVEL so the sound plays at the target loudness with its peaks under the ceiling."""
    delta = min(TARGET_LOUD - m["loud"], PEAK_MAX - m["peak"])
    entry["p"]["amp_level"] = round(max(-30.0, min(6.0, m["level"] + delta)), 2)
    return abs(delta) > 0.15


def gates(entry, m):
    fails = []
    if m["finite"] < 1:
        fails.append("non-finite")
    if m["loud"] < -45:
        fails.append("silent")
    if abs(m["dc"]) > 0.01:
        fails.append("dc")
    if m["high"] > (0.25 if entry["cat"] == "FX" else 0.125):  # noise risers and sweeps live on air
        fails.append("bright")
    if m["peak"] > -0.5:
        fails.append("clip")
    if m["tail"] > 14.0:
        fails.append("endless")
    if abs(m["loud"] - TARGET_LOUD) > 1.5 and PEAK_MAX - m["peak"] > 0.3:
        fails.append("level %.1f" % m["loud"])
    if TARGET_LOUD - m["loud"] > 1.0:
        fails.append("under target %.1f" % m["loud"])
    return fails


BASS_TARGETS = {  # (min, max) of tools/pack/bass_metrics.py, A1
    "sub": (0.40, 0.65), "body": (0.20, 1.0), "bite": (0.06, 0.25), "small": (0.18, 1.0), "loww": (0.0, 0.01),
    "punch": (6.0, 12.0), "even": (0.0, 3.0), "crest": (0.0, 14.0),
}


def bass_gates(bm):
    fails = []
    for k, (lo, hi) in BASS_TARGETS.items():
        if not lo <= bm[k] <= hi:
            fails.append("%s %.3g" % (k, bm[k]))
    return fails


# ============================================================================================== main
def main():
    atlas.PARAMS = atlas.load_params()
    only = sys.argv[sys.argv.index("--only") + 1] if "--only" in sys.argv else None
    entries = [e for e in SOUNDS if only is None or e["cat"] == only]
    names = [e["name"].lower() for e in SOUNDS]
    assert len(set(names)) == len(names), "duplicate names"
    for e in SOUNDS:
        assert not atlas.FORBIDDEN.search(e["name"]), e["name"]
        assert len(e["name"]) <= 24, e["name"]
    for e in entries:
        e["p"] = build(e)
        atlas.finish(e["p"])  # unknown ids fail here, before anything is rendered
    counts = {c: sum(1 for e in SOUNDS if e["cat"] == c) for c in CATEGORIES}
    print("%d sounds: %s" % (len(SOUNDS), ", ".join("%s %d" % kv for kv in counts.items())), flush=True)

    if STAGE.exists():
        shutil.rmtree(STAGE)
    for e in entries:
        e["stage"] = write(e, STAGE)
    dirs = sorted({e["stage"].parent for e in entries})
    if "--no-audit" in sys.argv:
        return

    for rnd in range(3):
        rows = audit(dirs)
        moved = 0
        for e in entries:
            m = rows[e["stage"].resolve()]
            moved += level(e, m)
            write(e, STAGE)
        print("level round %d: %d moved" % (rnd + 1, moved), flush=True)

    # Chord headroom: the polyphonic sounds played as chords must not pass the ceiling either.
    subprocess.run([str(AUDIT), "--chord-headroom", str(STAGE)], capture_output=True, text=True, cwd=str(ROOT))
    for e in entries:
        x = re.search(r'id="amp_level" value="([-0-9.e]+)"', e["stage"].read_text(encoding="utf-8"))
        e["p"]["amp_level"] = float(x.group(1))

    if WAVS.exists():
        shutil.rmtree(WAVS)
    rows = audit(dirs, WAVS, dry=True)
    report_rows = []
    for e in entries:
        m = rows[e["stage"].resolve()]
        e["m"] = m
        e["fails"] = gates(e, m)
        if e["cat"] == "BASS":
            e["bm"] = bass_metrics.measure(WAVS / "BASS" / (e["name"] + ".wav"))
            e["fails"] += bass_gates(e["bm"])
        report_rows.append(e)
    with open(ROOT / "build" / "auspice_fp.csv", "w", encoding="utf-8") as fh:  # for tools/pack/timbre.py
        fh.write("path;fp\n")
        for e in entries:
            fh.write("%s/%s;%s\n" % (e["cat"], e["name"], "|".join("%.2f" % x for x in e["m"]["fpdry"])))
    # No twins: within a category, every sound at least TWIN_DISTANCE from its nearest neighbour, effects off.
    for e in entries:
        others = [o for o in entries if o["cat"] == e["cat"] and o is not e]
        e["nn"] = min(((timbre.distance(e["m"]["fpdry"], o["m"]["fpdry"]), o["name"]) for o in others), default=(99.0, ""))
        if e["nn"][0] < (TWIN_DISTANCE_BASS if e["cat"] == "BASS" else TWIN_DISTANCE):
            e["fails"].append("twin of %s %.2f" % (e["nn"][1], e["nn"][0]))

    # Basses across the range: four roots, the same preset.
    basses = [e for e in entries if e["cat"] == "BASS"]
    if basses:
        roots_dir = ROOT / "build" / "auspice_roots"
        if roots_dir.exists():
            shutil.rmtree(roots_dir)
        rdirs = []
        for r in BASS_ROOTS:
            for e in basses:
                write(e, roots_dir / str(r), note=r)
            rdirs.append(roots_dir / str(r) / "BASS")
        rr = audit(rdirs)
        for e in basses:
            louds = [rr[(roots_dir / str(r) / "BASS" / (e["name"] + ".augur5")).resolve()]["loud"] for r in BASS_ROOTS]
            peaks = [rr[(roots_dir / str(r) / "BASS" / (e["name"] + ".augur5")).resolve()]["peak"] for r in BASS_ROOTS]
            e["roots"] = louds
            e["root_peaks"] = peaks
            if max(louds) - min(louds) > 3.0:
                e["fails"].append("range %.1f dB" % (max(louds) - min(louds)))
            if max(peaks) > -0.3:
                e["fails"].append("range peak %.1f" % max(peaks))

    bad = [e for e in report_rows if e["fails"]]
    for e in report_rows:
        m = e["m"]
        extra = ""
        if e["cat"] == "BASS":
            b = e["bm"]
            extra = " sub %.2f body %.2f bite %.2f small %.2f punch %.1f even %.1f crest %.1f range %.1f" % (
                b["sub"], b["body"], b["bite"], b["small"], b["punch"], b["even"], b["crest"], max(e["roots"]) - min(e["roots"]))
        print("%-8s %-24s loud %6.1f peak %5.1f lvl %5.1f hi %.3f tail %4.1f%s %s" % (
            e["cat"], e["name"], m["loud"], m["peak"], e["p"]["amp_level"], m["high"], m["tail"], extra,
            ("  <-- " + ", ".join(e["fails"])) if e["fails"] else ""))
    print("%d of %d need attention" % (len(bad), len(report_rows)), flush=True)
    if only or bad and "--force" not in sys.argv:
        return

    # Reproducibility: the whole library again in reverse order.
    rev = audit(dirs, reverse=True)
    diffs = sorted(abs(rev[e["stage"].resolve()]["loud"] - e["m"]["loud"]) for e in entries)
    print("order check: max %.2f dB" % diffs[-1], flush=True)

    if OUT.exists():
        shutil.rmtree(OUT)
    for e in entries:
        write(e, OUT)
    (OUT / "README.txt").write_text(README, encoding="utf-8")
    write_report(entries, diffs)
    if "--keep-stage" not in sys.argv:
        shutil.rmtree(STAGE, ignore_errors=True)
    print("written: %s" % OUT, flush=True)


README = """AUSPICE - melodic & progressive house for AUGUR-5 (TONAL LAB)

155 presets in 10 categories: BASS 20, ARP 15, PLUCK 15, LEAD 15, PAD 15, KEYS 15, BRASS 15, TEXTURE 15, DRONE 15, FX 15.
Written one by one for long, melodic sets at 120-124 BPM: rolling basses, hypnotic sequences, plucks with dotted
echoes, breathing pads, brass, keys, leads, textures, drones and transitions. Every sound was played through the
instrument the way its category is used and set to the same loudness (-16 dB short-term, peaks under -1 dBFS; pads,
keys, brass, plucks and arps also played as chords), so you can browse without touching the volume.

Basses are mono and dry, with their fundamental at the note you play (nothing below it), measured for weight, body,
bite and punch from E1 to G2. Put your own space on them in the mix.

Mod wheel: vibrato on leads and brass (WHEEL-MOD); on the other sounds it opens the filter. Aftertouch adds brightness.
ARP sounds run the arpeggiator in time with the host: hold a chord. Hypnotic Templum, Deep Water Sequence and Pulse of
the Flock are sequences: hold a single key.

Install: in AUGUR-5, PRESETS > INSTALL PACK (.ZIP), or unzip anywhere and use ADD PRESETS FOLDER.
"""


def write_report(entries, diffs):
    loud = sorted(e["m"]["loud"] for e in entries)
    peak = sorted(e["m"]["peak"] for e in entries)
    basses = [e for e in entries if e["cat"] == "BASS"]
    with open(REPORT, "w", encoding="utf-8") as fh:
        fh.write("# AUGUR-5 — AUSPICE: melodic & progressive house\n\n")
        fh.write("Generador: [`tools/pack/make_auspice.py`](../tools/pack/make_auspice.py). Resultado: `packs/AUSPICE/<CATEGORÍA>/<nombre>.augur5`; "
                 "zip: `python tools/pack/build_zip.py AUSPICE`. Cada sonido está escrito a mano (sin recetas al azar).\n\n")
        fh.write("| Categoría | Sonidos |\n|---|---|\n")
        for c in CATEGORIES:
            fh.write("| %s | %d |\n" % (c, sum(1 for e in entries if e["cat"] == c)))
        fh.write("\n**Nivel:** mediana %.1f dB (objetivo %.0f, los 400 ms más fuertes), rango %.1f … %.1f; pico máximo %.1f dBFS. "
                 "Los acordes (pads, keys, brass, plucks, arps) tampoco pasan de −1 dBFS.\n\n"
                 % (loud[len(loud) // 2], TARGET_LOUD, loud[0], loud[-1], peak[-1]))
        fh.write("**Reproducibilidad:** toda la librería otra vez en orden inverso: diferencia máxima %.2f dB.\n\n" % diffs[-1])
        nn = sorted(e["nn"][0] for e in entries)
        closest = min(entries, key=lambda e: e["nn"][0])
        fh.write("**Diferencias entre sonidos** (huella tímbrica sin efectos, distancia al vecino más cercano de su categoría; "
                 "1.0 ≈ audible en monitores, menos de %.1f = el mismo sonido): mediana %.2f; el par más cercano, %s ~ %s, %.2f. "
                 "Mínimo exigido: %.1f; en BASS, %.1f: los veinte tocan el mismo patrón mono y la huella empieza en 60 Hz, "
                 "por encima del fundamental de A1, así que solo los separa el espectro de 60 Hz para arriba.\n\n"
                 % (TWIN_DISTANCE, nn[len(nn) // 2], closest["name"], closest["nn"][1], closest["nn"][0], TWIN_DISTANCE, TWIN_DISTANCE_BASS))
        if basses:
            fh.write("## Bajos\n\n[`tools/pack/bass_metrics.py`](../tools/pack/bass_metrics.py), patrón rodante de semicorcheas a 122 BPM en A1. "
                     "SUB 30–90 Hz, BODY 90–300, BITE 300–2000, SMALL > 150 Hz (lo que suena en un parlante chico); "
                     "LOWW = estéreo bajo 150 Hz; PUNCH = pico de los primeros 12 ms sobre el cuerpo; EVEN = variación entre notas; "
                     "RANGO = diferencia de nivel entre E1, A1, D2 y G2.\n\n")
            fh.write("| Bajo | SUB | BODY | BITE | SMALL | LOWW | PUNCH dB | EVEN dB | CREST dB | RANGO dB |\n|---|---|---|---|---|---|---|---|---|---|\n")
            for e in basses:
                b = e["bm"]
                fh.write("| %s | %.0f %% | %.0f %% | %.0f %% | %.0f %% | %.3f | %.1f | %.1f | %.1f | %.1f |\n" % (
                    e["name"], 100 * b["sub"], 100 * b["body"], 100 * b["bite"], 100 * b["small"], b["loww"], b["punch"], b["even"],
                    b["crest"], max(e["roots"]) - min(e["roots"])))
            med = {k: sorted(e["bm"][k] for e in basses)[len(basses) // 2] for k in bass_metrics.FIELDS}
            fh.write("| **mediana** | %.0f %% | %.0f %% | %.0f %% | %.0f %% | %.3f | %.1f | %.1f | %.1f | |\n\n" % (
                100 * med["sub"], 100 * med["body"], 100 * med["bite"], 100 * med["small"], med["loww"], med["punch"], med["even"], med["crest"]))
            fh.write("Metas: " + ", ".join("%s %s…%s" % (k.upper(), lo, hi) for k, (lo, hi) in BASS_TARGETS.items()) + "; RANGO ≤ 3 dB.\n")


if __name__ == "__main__":
    main()
