"""AUGUR-5 Anthology Vol.1 - TONAL LAB expansion pack generator (500 presets, 20 genres x 25).

Rule-based sound design: every preset comes from a role archetype (bass, lead, pad, ...) chosen and
shaped by a genre profile (darkness, analog age, effects, delay grid, reverb type, fuzz, tape echo...),
with seeded variation so the pack is reproducible. Every file is then played through the real
instrument and loudness-matched by:  augur_preset_audit --level-pack "<pack folder>"

Usage:  python tools/pack/make_anthology.py        (writes packs/AUGUR-5 Anthology Vol.1)
"""
import pathlib
import random
import shutil
from xml.sax.saxutils import quoteattr

ROOT = pathlib.Path(__file__).resolve().parents[2]
PACK = "AUGUR-5 Anthology Vol.1"
OUT = ROOT / "packs" / PACK

# Parameter ranges (real units) - every value is clamped into these before it is written.
RANGES = {
    "osc1_freq": (-24, 24), "osc2_freq": (-24, 24), "osc1_fine": (-50, 50), "osc2_fine": (-50, 50),
    "osc1_pw": (5, 95), "osc2_pw": (5, 95), "osc1_oct": (0, 4), "osc2_oct": (0, 4),
    "flt_cutoff": (20, 20000), "hpf_cutoff": (10, 2000), "glide": (0, 5), "amp_level": (-40, 6),
    "lfo_rate": (0.05, 30), "lfo2_rate": (0.05, 30), "voice_count": (1, 16), "reverb_decay": (0.2, 20),
    "delay_time": (0.01, 2), "phaser_rate": (0.02, 10), "chorus_rate": (0.05, 5), "lfo_delay": (0, 5),
    "flt_env_amt": (-1, 1), "echo_bass": (-1, 1), "echo_treble": (-1, 1), "voice_pan": (-1, 1),
}
for _i in range(1, 9):
    RANGES["mm%d_amt" % _i] = (-1, 1)
SECONDS = ("fenv_a", "fenv_d", "fenv_r", "aenv_a", "aenv_d", "aenv_r", "menv_a", "menv_d", "menv_r")
for _k in SECONDS:
    RANGES[_k] = (0.001, 10)

# Matrix sources / destinations (indices of the parameter choice lists).
FENV, AENV, OSC2, LFO, WHEEL, VEL, AT, NOISE, MODENV, LFO2, KEY, RANDOM = range(12)
F1, F2, PW1, PW2, CUT, RES, AMP, LFORATE, FM, RING, SUB, DRIVE, L1, L2, NOISELVL, LFO2RATE = range(16)

ROLE_PREFIX = {"Bass": "BA", "Lead": "LD", "Pad": "PD", "Pluck": "PL", "Stab": "CH", "Keys": "KY", "Arp": "AR",
               "Atmos & FX": "FX", "Drums": "DR"}
DEFAULT_COUNTS = {"Bass": 5, "Lead": 3, "Pad": 3, "Pluck": 3, "Stab": 3, "Keys": 2, "Arp": 3, "Atmos & FX": 2, "Drums": 1}


class Preset(dict):
    def mm(self, slot, src, dst, amt):
        self["mm%d_src" % slot] = src
        self["mm%d_dst" % slot] = dst
        self["mm%d_amt" % slot] = amt

    def env(self, which, a, d, s, r):
        self[which + "_a"], self[which + "_d"], self[which + "_s"], self[which + "_r"] = a, d, s, r


def u(rng, lo, hi):
    return rng.uniform(lo, hi)


# ---------------------------------------------------------------------------------------------- roles
def bass(rng, g, kind):
    p = Preset(voice_count=1, legato=1, mix_osc1=0.9, mix_osc2=0.6, flt_keytrack=1, amp_velocity=0.35)
    p.env("aenv", 0.002, 0.6, 0.85, u(rng, 0.06, 0.16))
    if kind == "sub":
        p.update(osc1_saw=0, osc1_pulse=1, mix_osc1=u(rng, 0.25, 0.5), mix_osc2=0, mix_sub=u(rng, 0.8, 1.0),
                 flt_cutoff=u(rng, 140, 300), flt_env_amt=u(rng, 0.1, 0.25))
        p.env("fenv", 0.001, u(rng, 0.15, 0.3), 0.1, 0.1)
    elif kind == "saw":
        p.update(osc2_freq=rng.choice([-12, 0]), osc2_fine=u(rng, 3, 9), flt_cutoff=u(rng, 180, 450), flt_reso=u(rng, 0.2, 0.5),
                 flt_env_amt=u(rng, 0.35, 0.6), flt_velocity=u(rng, 0.2, 0.5))
        p.env("fenv", 0.001, u(rng, 0.15, 0.4), u(rng, 0.0, 0.2), 0.1)
    elif kind == "rolling":
        p.update(flt_model=rng.choice([0, 2]), osc2_freq=-12, osc2_fine=u(rng, 3, 7), flt_cutoff=u(rng, 200, 320),
                 flt_reso=u(rng, 0.35, 0.5), flt_env_amt=u(rng, 0.45, 0.6), legato=0)
        p.env("fenv", 0.001, u(rng, 0.12, 0.22), 0.0, 0.1)
        p.env("aenv", 0.001, u(rng, 0.2, 0.3), 0.0, 0.08)
    elif kind == "reese":
        p.update(voice_mode=1, voice_count=2, voice_detune=u(rng, 0.6, 0.95), osc2_fine=rng.choice([-1, 1]) * u(rng, 10, 18),
                 flt_cutoff=u(rng, 280, 650), flt_reso=u(rng, 0.15, 0.3), mix_drive=u(rng, 0.4, 0.65), lfo2_rate=u(rng, 0.1, 0.4))
        p.mm(5, LFO2, CUT, u(rng, 0.1, 0.2))
    elif kind == "acid":
        p.update(flt_model=rng.choice([4, 4, 0]), osc1_saw=rng.choice([1, 0]), osc1_pulse=rng.choice([0, 1]), mix_osc2=0,
                 flt_cutoff=u(rng, 240, 420), flt_reso=u(rng, 0.72, 0.9), flt_env_amt=u(rng, 0.5, 0.72), glide=u(rng, 0.03, 0.07),
                 flt_velocity=0.5, mix_drive=u(rng, 0.35, 0.6))
        p["osc1_saw"] = 1 if p["osc1_pulse"] == 0 else 0
        p.env("fenv", 0.001, u(rng, 0.14, 0.3), u(rng, 0.0, 0.1), 0.08)
    elif kind == "fm":
        p.update(osc2_saw=0, osc2_tri=1, osc2_freq=rng.choice([12, 19, 24]), mix_osc2=0, osc1_saw=0, osc1_pulse=1,
                 mix_sub=u(rng, 0.3, 0.55), flt_cutoff=u(rng, 900, 1800), flt_env_amt=0.2)
        p.env("menv", 0.001, u(rng, 0.08, 0.2), 0.0, 0.1)
        p.mm(5, MODENV, FM, u(rng, 0.5, 0.8))
        p.env("fenv", 0.001, 0.15, 0.0, 0.1)
        p.env("aenv", 0.001, u(rng, 0.3, 0.5), u(rng, 0.0, 0.4), 0.1)
    elif kind == "wobble":
        p.update(osc2_saw=0, osc2_pulse=1, osc2_fine=u(rng, 4, 9), flt_cutoff=u(rng, 250, 450), flt_reso=u(rng, 0.4, 0.6),
                 mix_drive=u(rng, 0.55, 0.8), lfo2_sync=1, lfo2_rate=u(rng, 0.3, 1.2), lfo2_wave=rng.choice([0, 1, 3]),
                 lfo2_retrig=1, mix_sub=0.4)
        p.mm(5, LFO2, CUT, u(rng, 0.35, 0.55))
        if rng.random() < 0.5:
            p.update(fuzz_on=1, fuzz_sustain=u(rng, 0.3, 0.6), fuzz_tone=0.45, fuzz_volume=0.6, fuzz_mix=u(rng, 0.25, 0.5))
    elif kind == "808":
        p.update(osc1_saw=0, mix_osc1=0, osc2_saw=0, osc2_tri=1, mix_osc2=1, mix_sub=u(rng, 0.4, 0.6), flt_cutoff=u(rng, 350, 700),
                 glide=u(rng, 0.05, 0.12), mix_drive=u(rng, 0.3, 0.6))
        p.env("menv", 0.001, u(rng, 0.05, 0.1), 0.0, 0.1)
        p.mm(5, MODENV, F2, u(rng, 0.3, 0.5))
        p.env("aenv", 0.001, u(rng, 1.4, 2.4), 0.0, u(rng, 0.3, 0.6))
    else:  # pluck bass
        p.update(osc2_freq=12, mix_osc2=0.4, mix_sub=u(rng, 0.3, 0.6), flt_cutoff=u(rng, 250, 450), flt_env_amt=u(rng, 0.45, 0.6),
                 legato=0)
        p.env("fenv", 0.001, u(rng, 0.1, 0.2), 0.0, 0.1)
        p.env("aenv", 0.001, u(rng, 0.2, 0.4), 0.0, 0.1)
    return p


def lead(rng, g, kind):
    p = Preset(voice_count=1, legato=1, glide=u(rng, 0.03, 0.09), mix_osc2=0.7, osc2_fine=u(rng, 4, 9),
               lfo_rate=u(rng, 4.8, 6.0), lfo_delay=u(rng, 0.3, 0.8))
    p.mm(4, LFO, F1, u(rng, 0.06, 0.1))
    p.env("aenv", u(rng, 0.005, 0.03), 0.5, 0.9, u(rng, 0.3, 0.7))
    if kind == "sync":
        p.update(osc1_sync=1, osc1_freq=rng.choice([5, 7, 12]), mix_osc2=0.2, flt_cutoff=u(rng, 2500, 4500), flt_reso=0.2)
        p.env("menv", 0.001, u(rng, 0.4, 0.9), 0.2, 0.3)
        p.mm(5, MODENV, F1, u(rng, 0.3, 0.5))
    elif kind == "supersaw":
        p.update(voice_mode=1, voice_count=4, voice_detune=u(rng, 0.45, 0.7), osc2_freq=rng.choice([0, 12]), mix_osc2=0.8,
                 flt_cutoff=u(rng, 2000, 3500), flt_env_amt=0.2, legato=0, chorus_on=1, chorus_mode=rng.choice([1, 2]), chorus_mix=0.35)
        p.env("fenv", 0.01, 0.6, 0.6, 0.4)
    elif kind == "fuzz":
        p.update(flt_cutoff=u(rng, 2200, 3500), fuzz_on=1, fuzz_sustain=u(rng, 0.65, 0.95), fuzz_tone=u(rng, 0.4, 0.6),
                 fuzz_volume=0.6, fuzz_mix=u(rng, 0.7, 1.0))
    elif kind == "pwm":
        p.update(osc1_saw=0, osc1_pulse=1, osc2_saw=0, osc2_pulse=1, osc1_pw=u(rng, 30, 50), flt_cutoff=u(rng, 1500, 3000),
                 lfo2_rate=u(rng, 0.3, 1.0), lfo2_retrig=0)
        p.mm(5, LFO2, PW1, u(rng, 0.2, 0.4))
        p.mm(6, LFO2, PW2, -u(rng, 0.2, 0.4))
    elif kind == "fm":
        p.update(osc2_saw=0, osc2_tri=1, osc2_freq=rng.choice([12, 19, 24]), mix_osc2=0, osc_xmod=u(rng, 0.2, 0.4), osc1_saw=0,
                 osc1_pulse=1, flt_cutoff=6000)
        p.mm(5, WHEEL, FM, 0.4)
    elif kind == "flute":
        p.update(osc1_saw=0, mix_osc1=0, osc2_saw=0, osc2_tri=1, mix_osc2=1, mix_noise=0.05, flt_cutoff=3000)
        p.env("menv", u(rng, 0.05, 0.1), 0.3, 0.2, 0.3)
        p.mm(5, MODENV, NOISELVL, u(rng, 0.2, 0.3))
        p.env("aenv", u(rng, 0.04, 0.08), 0.5, 1.0, 0.3)
    elif kind == "ring":
        p.update(osc2_freq=rng.choice([3, 5, 7]), osc2_fine=u(rng, 10, 25), mix_osc1=0.4, mix_osc2=0.2, mix_ring=u(rng, 0.6, 0.9),
                 flt_cutoff=u(rng, 2500, 4000))
    elif kind == "horn":
        # Big trumpeting brass: detuned saws, a pitch scoop into every note, filter swell, legato slides.
        p.update(unison=1, voice_count=4, voice_detune=u(rng, 0.3, 0.45), mix_osc1=0.9, mix_osc2=0.8, osc2_fine=-u(rng, 7, 11),
                 flt_cutoff=u(rng, 600, 900), flt_reso=u(rng, 0.2, 0.3), flt_env_amt=u(rng, 0.4, 0.55), flt_keytrack=1,
                 mix_drive=u(rng, 0.4, 0.55), glide=u(rng, 0.07, 0.12))
        p.env("fenv", u(rng, 0.05, 0.09), u(rng, 0.5, 0.8), 0.45, 0.4)
        p.env("menv", 0.001, u(rng, 0.1, 0.16), 0.0, 0.1)
        p.mm(5, MODENV, F1, -u(rng, 0.3, 0.4))
        p.mm(6, MODENV, F2, -u(rng, 0.3, 0.4))
        p.update(chorus_on=1, chorus_mode=1, chorus_mix=0.3)
    else:  # acid lead
        p.update(flt_model=4, mix_osc2=0.3, flt_cutoff=u(rng, 600, 1200), flt_reso=u(rng, 0.75, 0.88), flt_env_amt=0.5)
        p.env("fenv", 0.001, 0.3, 0.2, 0.2)
    return p


def pad(rng, g, kind):
    p = Preset(mix_osc2=0.8, osc2_fine=u(rng, 6, 12), voice_spread=u(rng, 0.7, 1.0), flt_cutoff=u(rng, 900, 2200), flt_reso=u(rng, 0.1, 0.3))
    p.env("aenv", u(rng, 0.6, 2.0), 1.5, 1.0, u(rng, 2.5, 5.0))
    p.env("fenv", u(rng, 0.8, 2.5), u(rng, 2.0, 4.0), u(rng, 0.4, 0.7), 3.0)
    p["flt_env_amt"] = u(rng, 0.1, 0.35)
    if kind == "strings":
        p.update(flt_slope=1, hpf_cutoff=u(rng, 120, 250), chorus_on=1, chorus_mode=3, chorus_mix=0.5, lfo_rate=5.0)
        p.mm(3, LFO, F1, 0.04)
    elif kind == "plate":
        p.update(reverb_on=1, reverb_type=1, reverb_size=1.0, reverb_decay=u(rng, 7, 11), reverb_mix=0.5)
    elif kind == "choir":
        p.update(flt_model=3, flt_mode=1, flt_cutoff=u(rng, 800, 1300), flt_reso=u(rng, 0.45, 0.6), mix_noise=0.1)
    elif kind == "wind":
        p.update(flt_model=3, flt_mode=2, flt_cutoff=u(rng, 1200, 2000), flt_reso=0.4, mix_noise=u(rng, 0.3, 0.5), mix_osc1=0.5, mix_osc2=0.5)
        p.mm(5, LFO2, CUT, 0.3)
        p.update(lfo2_wave=6, lfo2_rate=u(rng, 0.1, 0.3))
    elif kind == "shoegaze":
        p.update(voice_mode=1, voice_count=10, voice_detune=u(rng, 0.6, 0.85), fuzz_on=1, fuzz_sustain=u(rng, 0.6, 0.85),
                 fuzz_tone=u(rng, 0.35, 0.5), fuzz_volume=0.55, fuzz_mix=u(rng, 0.4, 0.6), phaser_on=1, phaser_rate=u(rng, 0.05, 0.15),
                 phaser_mix=0.35)
    elif kind == "tape":
        p.update(analog_age=u(rng, 0.7, 0.95), echo_on=1, echo_mode=rng.choice([8, 10]), echo_rate=u(rng, 0.15, 0.35),
                 echo_intensity=u(rng, 0.45, 0.6), echo_wow=u(rng, 0.6, 0.9), echo_treble=-0.4, echo_volume=0.45, echo_reverb=0.5)
    elif kind == "fm_glass":
        p.update(osc2_saw=0, osc2_tri=1, osc2_freq=12, mix_osc2=0.4, osc_xmod=u(rng, 0.08, 0.15), flt_cutoff=3000, flt_slope=1)
    elif kind == "evolving":
        p.update(lfo2_wave=rng.choice([0, 6]), lfo2_rate=u(rng, 0.05, 0.15), lfo2_retrig=0)
        p.mm(5, LFO2, CUT, u(rng, 0.15, 0.35))
        p.env("menv", u(rng, 2, 5), 4.0, 0.6, 4.0)
        p.mm(6, MODENV, PW1, 0.3)
        p.update(osc1_saw=0, osc1_pulse=1, osc1_pw=35)
    # warm = the base
    return p


def pluck(rng, g, kind):
    p = Preset(mix_osc2=u(rng, 0.5, 0.75), osc2_fine=u(rng, 4, 10), flt_cutoff=u(rng, 280, 500), flt_reso=u(rng, 0.25, 0.45),
               flt_env_amt=u(rng, 0.5, 0.65), flt_velocity=u(rng, 0.3, 0.5), voice_spread=0.7)
    p.env("fenv", 0.001, u(rng, 0.2, 0.4), 0.0, 0.3)
    p.env("aenv", 0.001, u(rng, 0.6, 1.4), 0.0, u(rng, 0.4, 0.9))
    if kind == "fm":
        p.update(osc2_saw=0, osc2_tri=1, osc2_freq=rng.choice([12, 24]), mix_osc2=0, osc1_saw=0, osc1_pulse=1, flt_cutoff=u(rng, 2500, 4500))
        p.env("menv", 0.001, u(rng, 0.15, 0.35), 0.0, 0.2)
        p.mm(5, MODENV, FM, u(rng, 0.3, 0.45))
    elif kind == "bite":
        p.update(flt_model=4, flt_reso=u(rng, 0.5, 0.65), flt_cutoff=u(rng, 350, 500))
    elif kind == "pingpong":
        p.update(delay_on=1, delay_sync=1, delay_div=6, delay_pingpong=1, delay_fb=u(rng, 0.45, 0.6), delay_mix=0.35)
    elif kind == "sub":
        p.update(mix_sub=u(rng, 0.5, 0.7), mix_osc2=0.3)
    elif kind == "random":
        p.mm(5, RANDOM, CUT, u(rng, 0.25, 0.4))
        p.mm(6, RANDOM, PW1, 0.3)
        p.update(osc1_saw=0, osc1_pulse=1)
    elif kind == "bp":
        # Band-pass pluck: a static band (little envelope), or the sweep through the band makes a needle-sharp attack.
        p.update(flt_model=3, flt_mode=1, flt_cutoff=u(rng, 700, 1100), flt_reso=u(rng, 0.3, 0.36), flt_env_amt=0.1,
                 flt_velocity=0.15, aenv_a=0.003)
    elif kind == "cascade":
        p.update(flt_model=2)
    return p


def stab(rng, g, kind):
    p = Preset(mix_osc2=u(rng, 0.6, 0.8), osc2_fine=u(rng, 6, 10), flt_cutoff=u(rng, 800, 1500), flt_reso=u(rng, 0.15, 0.35),
               flt_env_amt=u(rng, 0.3, 0.45))
    p.env("fenv", 0.001, u(rng, 0.15, 0.3), 0.0, 0.2)
    p.env("aenv", 0.001, u(rng, 0.25, 0.45), 0.0, u(rng, 0.15, 0.3))
    if kind == "house":
        p.update(flt_slope=1, reverb_on=1, reverb_type=1, reverb_decay=2.0, reverb_mix=0.3, osc2_freq=rng.choice([0, 7]))
    elif kind == "techno":
        p.update(delay_on=1, delay_sync=1, delay_div=6, delay_pingpong=1, delay_fb=u(rng, 0.5, 0.65), delay_mix=0.35)
    elif kind == "dub":
        p.update(flt_model=3, flt_mode=1, flt_cutoff=u(rng, 600, 900), flt_reso=u(rng, 0.45, 0.6), osc2_freq=7,
                 echo_on=1, echo_mode=rng.choice([8, 9, 10]), echo_rate=u(rng, 0.3, 0.45), echo_intensity=u(rng, 0.55, 0.7),
                 echo_bass=-0.3, echo_volume=0.6, echo_reverb=0.45)
    elif kind == "rave":
        p.update(osc2_freq=12, fuzz_on=1, fuzz_sustain=u(rng, 0.6, 0.85), fuzz_tone=0.6, fuzz_volume=0.6, fuzz_mix=0.7)
    elif kind == "brass":
        p.update(voice_mode=1, voice_count=10, voice_detune=0.35, flt_cutoff=u(rng, 500, 700), flt_env_amt=0.55)
        p.env("fenv", 0.03, 0.3, 0.3, 0.2)
        p.env("aenv", 0.01, 0.4, 0.5, 0.2)
    elif kind == "organ":
        p.update(osc1_saw=0, osc1_pulse=1, osc2_saw=0, osc2_pulse=1, osc2_freq=12, mix_sub=0.4, flt_cutoff=2500, flt_env_amt=0.1)
        p.env("aenv", 0.003, 0.3, 0.0, 0.15)
    return p


def keys(rng, g, kind):
    p = Preset(flt_velocity=0.4)
    p.env("aenv", 0.001, u(rng, 1.6, 2.6), u(rng, 0.15, 0.3), u(rng, 0.5, 0.9))
    if kind == "epiano":
        p.update(osc2_saw=0, osc2_tri=1, osc2_freq=12, mix_osc2=0.5, osc1_saw=0, osc1_pulse=1, osc_xmod=u(rng, 0.05, 0.1),
                 flt_cutoff=u(rng, 1500, 2500), flt_env_amt=0.3, chorus_on=1, chorus_mode=1, chorus_mix=0.3)
        p.env("fenv", 0.001, 0.8, 0.2, 0.5)
    elif kind == "organ":
        p.update(osc1_saw=0, osc1_pulse=1, osc2_saw=0, osc2_pulse=1, osc2_freq=12, mix_osc2=0.5, mix_sub=0.5, flt_cutoff=2500,
                 flt_slope=1, chorus_on=1, chorus_mode=3, chorus_mix=0.4)
        p.env("aenv", 0.005, 0.5, 1.0, 0.1)
    elif kind == "clav":
        p.update(flt_model=4, osc1_saw=0, osc1_pulse=1, osc1_pw=u(rng, 15, 22), mix_osc2=0, flt_cutoff=700, flt_reso=0.45,
                 flt_env_amt=0.4, phaser_on=1, phaser_mix=0.3)
        p.env("fenv", 0.001, 0.18, 0.1, 0.1)
        p.env("aenv", 0.001, 0.5, 0.3, 0.1)
    elif kind == "spring":
        p.update(osc2_saw=0, osc2_tri=1, osc2_freq=12, mix_osc2=0.5, osc_xmod=0.08, flt_cutoff=2000, flt_env_amt=0.3,
                 reverb_on=1, reverb_type=2, reverb_size=0.5, reverb_decay=u(rng, 2.0, 3.0), reverb_mix=0.4)
    elif kind == "vibes":
        p.update(osc1_saw=0, mix_osc1=0, osc2_saw=0, osc2_tri=1, mix_osc2=1, osc_xmod=0.05, flt_cutoff=5000, lfo2_rate=5.5, lfo2_retrig=0)
        p.mm(5, LFO2, AMP, -0.3)
    else:  # bell
        p.update(osc2_freq=rng.choice([17, 19]), osc2_saw=0, osc2_tri=1, mix_osc1=0.3, mix_osc2=0.2, mix_ring=u(rng, 0.5, 0.7),
                 flt_cutoff=5000, reverb_on=1, reverb_type=1, reverb_mix=0.3)
        p.env("aenv", 0.001, 2.0, 0.0, 1.5)
    return p


def arp(rng, g, kind):
    base = pluck(rng, g, rng.choice(["classic", "fm", "bite", "cascade", "sub"]))
    base.update(arp_on=1, arp_mode=rng.choice(g["arp_modes"]), arp_rate=rng.choice(g["arp_rates"]), arp_oct=rng.choice([1, 2, 2, 3]),
                arp_gate=u(rng, 0.3, 0.7), arp_swing=g.get("swing", 0.0) * rng.random())
    base.env("aenv", 0.001, u(rng, 0.2, 0.4), 0.0, 0.2)
    if kind == "latch":
        base["arp_latch"] = 1
    return base


def atmos(rng, g, kind):
    p = Preset(mix_osc2=0.8, osc2_fine=u(rng, 4, 12), voice_spread=0.9, reverb_on=1, reverb_size=1.0, reverb_decay=u(rng, 9, 16),
               reverb_mix=0.5)
    p.env("aenv", u(rng, 2.0, 4.0), 2.0, 1.0, u(rng, 4.0, 6.0))
    if kind == "drone":
        p.update(osc2_freq=7, mix_sub=0.4, flt_cutoff=u(rng, 500, 900), flt_reso=0.3, lfo2_wave=6, lfo2_rate=0.06, lfo2_retrig=0)
        p.mm(5, LFO2, CUT, 0.35)
        if rng.random() < 0.5:
            p.update(fuzz_on=1, fuzz_sustain=u(rng, 0.6, 0.9), fuzz_tone=0.4, fuzz_volume=0.55, fuzz_mix=0.6)
    elif kind == "riser":
        p.update(mix_noise=0.4, flt_cutoff=150, flt_reso=0.4)
        p.env("menv", u(rng, 6, 10), 1.0, 1.0, 0.5)
        p.mm(5, MODENV, CUT, 1.0)
        p.mm(6, MODENV, F1, 0.3)
    elif kind == "sh":
        p.update(osc1_saw=0, osc1_pulse=1, mix_osc2=0, flt_cutoff=2500, flt_reso=0.5, lfo2_wave=5, lfo2_sync=1,
                 lfo2_rate=u(rng, 1.5, 4.0), lfo2_retrig=0, delay_on=1, delay_sync=1, delay_div=6, delay_pingpong=1, delay_mix=0.35)
        p.mm(5, LFO2, F1, 0.45)
        p.mm(6, LFO2, CUT, 0.3)
        p.env("aenv", 0.01, 1.0, 1.0, 0.5)
    elif kind == "wind":
        p.update(mix_osc1=0, mix_osc2=0, mix_noise=1, flt_cutoff=1500, flt_reso=0.5, lfo2_wave=6, lfo2_rate=0.15, phaser_on=1,
                 phaser_depth=1.0, phaser_fb=0.7, phaser_mix=0.5)
        p.mm(5, LFO2, CUT, 0.4)
    elif kind == "ring":
        p.update(osc2_freq=3, osc2_fine=15, mix_osc1=0.5, mix_osc2=0.5, mix_ring=1.0, flt_cutoff=1500, flt_reso=0.4, lfo2_wave=6, lfo2_rate=0.1)
        p.mm(5, LFO2, F2, 0.2)
    else:  # alien radio
        p.update(osc2_lofreq=1, osc2_kbd=0, mix_osc2=0, osc_xmod=0.5, mix_ring=0.3, lfo2_wave=5, lfo2_rate=6, flt_model=3, flt_mode=1,
                 flt_cutoff=1500, flt_reso=0.6, delay_on=1, delay_sync=1, delay_div=4, delay_fb=0.6, delay_mix=0.35)
        p.mm(5, LFO2, CUT, 0.35)
        p.env("aenv", 0.1, 1.0, 1.0, 1.0)
    return p


def drum(rng, g, kind):
    p = Preset(flt_keytrack=0, reverb_on=0)
    if kind == "kick":
        p.update(osc1_saw=0, mix_osc1=0, osc2_saw=0, osc2_tri=1, mix_osc2=1, mix_sub=0.5, flt_cutoff=400, mix_drive=u(rng, 0.3, 0.6))
        p.env("menv", 0.001, u(rng, 0.05, 0.09), 0.0, 0.1)
        p.mm(5, MODENV, F2, u(rng, 0.6, 0.8))
        p.env("aenv", 0.001, u(rng, 0.35, 0.7), 0.0, 0.3)
        return p, -16.0
    if kind == "snare":
        p.update(mix_osc1=0.4, osc1_saw=0, osc1_pulse=1, osc1_freq=12, mix_osc2=0, mix_noise=1.0, flt_model=3, flt_mode=1,
                 flt_cutoff=u(rng, 1500, 2500), flt_reso=0.2)
        p.env("aenv", 0.001, u(rng, 0.15, 0.25), 0.0, 0.15)
        return p, -20.0
    if kind == "hat":
        p.update(mix_osc1=0, mix_osc2=0, mix_noise=1.0, flt_model=3, flt_mode=2, flt_cutoff=u(rng, 6000, 9000), flt_reso=0.3)
        p.env("aenv", 0.002, u(rng, 0.05, 0.25), 0.0, 0.08)
        return p, -25.0
    if kind == "metal":
        p.update(osc1_freq=24, osc2_freq=17, osc2_fine=30, mix_osc1=0.5, mix_osc2=0.5, mix_ring=1.0, mix_drive=0.35, flt_model=3,
                 flt_mode=2, flt_slope=1, flt_cutoff=900)
        p.env("aenv", 0.001, 0.35, 0.0, 0.15)
        return p, -25.0
    # tom
    p.update(osc2_saw=0, osc2_tri=1, mix_osc2=0, osc1_saw=0, osc1_pulse=1, mix_osc1=0.8, flt_cutoff=2000)
    p.env("menv", 0.001, 0.12, 0.0, 0.1)
    p.mm(5, MODENV, F1, 0.6)
    p.mm(6, MODENV, FM, 0.3)
    p.env("aenv", 0.001, 0.35, 0.0, 0.3)
    return p, -16.0


ROLE_FN = {"Bass": bass, "Lead": lead, "Pad": pad, "Pluck": pluck, "Stab": stab, "Keys": keys, "Arp": arp, "Atmos & FX": atmos}

# ---------------------------------------------------------------------------------------------- genres
def G(name, **kw):
    kw["name"] = name
    kw.setdefault("dark", 0.5)
    kw.setdefault("age", 0.4)
    kw.setdefault("space", 0.5)
    kw.setdefault("delay_divs", [6])
    kw.setdefault("reverbs", [0, 1])
    kw.setdefault("fx", {})
    kw.setdefault("arp_modes", [0, 2, 4])
    kw.setdefault("arp_rates", [5])
    kw.setdefault("counts", DEFAULT_COUNTS)
    return kw


GENRES = [
    G("Techno", dark=0.7, bass=["rolling", "saw", "sub", "acid"], lead=["sync", "fuzz", "ring"], pad=["evolving", "wind", "choir"],
      pluck=["bite", "bp", "cascade"], stab=["techno", "dub"], keys=["organ", "bell"], atmos=["drone", "sh", "alien"],
      drums=["kick", "hat", "metal"], fx={"fuzz": 0.25, "echo": 0.2, "phaser": 0.15}, delay_divs=[5, 6], arp_rates=[5, 6],
      adj=["Warehouse", "Concrete", "Iron", "Nocturnal", "Industrial", "Hypnotic", "Raw", "Cold", "Machine", "Tunnel", "Steel", "Carbon", "Night", "Grid", "Obsidian"]),
    G("Melodic Techno", dark=0.55, bass=["rolling", "saw", "sub"], lead=["horn", "fuzz", "supersaw", "pwm"],
      pad=["shoegaze", "tape", "evolving", "warm"], pluck=["classic", "pingpong", "fm"], stab=["dub", "techno"], keys=["epiano", "bell"],
      atmos=["drone", "riser", "ring"], drums=["kick", "hat"], fx={"fuzz": 0.4, "echo": 0.35, "phaser": 0.2}, reverbs=[1, 0],
      adj=["Afterglow", "Melancholic", "Stellar", "Distant", "Crimson", "Velvet", "Twilight", "Silent", "Lunar", "Aurora", "Solitude", "Ember", "Faded", "Horizon", "Nomad"]),
    G("Progressive House", dark=0.4, bass=["rolling", "saw", "sub", "pluck"], lead=["supersaw", "pwm", "horn"], pad=["warm", "strings", "plate"],
      pluck=["classic", "pingpong", "fm"], stab=["house", "brass"], keys=["epiano", "bell"], atmos=["riser", "drone"], drums=["kick", "hat"],
      fx={"echo": 0.15, "phaser": 0.1}, delay_divs=[6, 9], reverbs=[0, 1],
      adj=["Sunrise", "Open", "Endless", "Coastal", "Golden", "Elevate", "Skyline", "Wander", "Glide", "Summit", "Clear", "Tidal", "Radiant", "Journey", "Soaring"]),
    G("Deep House", dark=0.6, age=0.6, bass=["sub", "pluck", "fm"], lead=["flute", "pwm"], pad=["warm", "strings", "tape"],
      pluck=["classic", "bp"], stab=["house", "organ", "dub"], keys=["epiano", "organ", "vibes"], atmos=["drone", "wind"], drums=["hat", "tom"],
      fx={"echo": 0.25}, reverbs=[1, 2], swing=0.3,
      adj=["Smoky", "Velour", "Basement", "Late Night", "Mellow", "Soulful", "Dusky", "Warm", "Hushed", "Satin", "Amber", "Lounge", "Soft", "Ember", "Moody"]),
    G("House", dark=0.4, bass=["sub", "saw", "pluck"], lead=["pwm", "supersaw"], pad=["warm", "strings"], pluck=["classic", "bp"],
      stab=["house", "organ", "brass"], keys=["organ", "epiano"], atmos=["riser", "sh"], drums=["kick", "snare", "hat"], reverbs=[1, 0], swing=0.25,
      adj=["Jack", "Groove", "Classic", "Disco", "Uplift", "Friday", "Block", "Piano", "Loft", "Chicago", "Garage", "Sunday", "Vinyl", "Party", "Filter"]),
    G("Tech House", dark=0.55, bass=["rolling", "sub", "fm", "pluck"], lead=["sync", "ring"], pad=["evolving", "choir"], pluck=["bp", "bite", "random"],
      stab=["techno", "house"], keys=["clav", "organ"], atmos=["sh", "alien"], drums=["kick", "hat", "tom"], fx={"phaser": 0.2}, swing=0.2,
      adj=["Bounce", "Swing", "Funk", "Rubber", "Shuffle", "Punch", "Warehouse", "Tool", "Jackin", "Pressure", "Bumper", "Stomp", "Snap", "Tight", "Groovy"]),
    G("Minimal", dark=0.55, bass=["sub", "pluck", "fm"], lead=["ring", "fm"], pad=["choir", "evolving"], pluck=["bp", "random", "fm"],
      stab=["dub", "techno"], keys=["bell", "clav"], atmos=["sh", "ring"], drums=["metal", "tom", "hat"], delay_divs=[3, 5, 6], arp_rates=[5, 6],
      adj=["Micro", "Click", "Dry", "Sparse", "Fine", "Tiny", "Precise", "Pointillist", "Glitch", "Quiet", "Thin", "Nano", "Dot", "Clean", "Tick"]),
    G("Trance", dark=0.3, bass=["saw", "rolling", "pluck"], lead=["supersaw", "sync", "pwm"], pad=["strings", "warm", "plate"],
      pluck=["pingpong", "classic", "fm"], stab=["brass", "house"], keys=["bell", "epiano"], atmos=["riser", "drone"], drums=["kick", "hat"],
      counts={"Bass": 4, "Lead": 5, "Pad": 3, "Pluck": 3, "Stab": 2, "Keys": 1, "Arp": 4, "Atmos & FX": 2, "Drums": 1},
      reverbs=[0, 1], delay_divs=[6, 5], arp_modes=[0, 2], arp_rates=[5, 6],
      adj=["Euphoric", "Anthem", "Celestial", "Uplifting", "Eternal", "Ascend", "Heaven", "Starlight", "Dreamer", "Infinity", "Arcadia", "Emotion", "Legend", "Paradise", "Summer"]),
    G("Psytrance", dark=0.5, bass=["rolling", "acid", "fm"], lead=["acid", "sync", "ring", "fm"], pad=["evolving", "wind"], pluck=["bite", "fm", "random"],
      stab=["techno"], keys=["bell"], atmos=["alien", "sh", "ring"], drums=["kick", "metal"], fx={"phaser": 0.3}, arp_rates=[5, 7], arp_modes=[0, 3, 4],
      adj=["Fractal", "Shaman", "Cosmic", "Portal", "Spiral", "Astral", "Mantra", "Lotus", "Galaxy", "Hyper", "Mystic", "Vortex", "Nebula", "Prism", "Tribal"]),
    G("Drum & Bass", dark=0.55, bass=["reese", "wobble", "sub", "fm"], lead=["sync", "supersaw", "fuzz"], pad=["strings", "plate", "evolving"],
      pluck=["pingpong", "fm"], stab=["techno", "rave"], keys=["epiano", "bell"], atmos=["riser", "drone"], drums=["snare", "hat"],
      counts={"Bass": 7, "Lead": 3, "Pad": 3, "Pluck": 2, "Stab": 2, "Keys": 2, "Arp": 2, "Atmos & FX": 3, "Drums": 1},
      adj=["Liquid", "Neuro", "Jungle", "Roller", "Amen", "Sonic", "Break", "Riddim", "Rapid", "Tech", "Halftime", "Shadow", "Urban", "Pulse", "Future"]),
    G("Dubstep", dark=0.6, bass=["wobble", "reese", "fm", "808"], lead=["fuzz", "sync", "ring"], pad=["evolving", "choir"], pluck=["bite", "fm"],
      stab=["rave", "techno"], keys=["bell"], atmos=["riser", "alien"], drums=["snare", "metal"], fx={"fuzz": 0.35},
      counts={"Bass": 8, "Lead": 3, "Pad": 2, "Pluck": 2, "Stab": 2, "Keys": 1, "Arp": 2, "Atmos & FX": 3, "Drums": 2},
      adj=["Heavy", "Growl", "Filthy", "Mutant", "Titan", "Wub", "Monster", "Crusher", "Brutal", "Riot", "Hammer", "Venom", "Beast", "Gritty", "Grime"]),
    G("UK Garage", dark=0.45, bass=["fm", "sub", "pluck", "808"], lead=["pwm", "flute"], pad=["warm", "strings"], pluck=["classic", "bp"],
      stab=["organ", "house"], keys=["organ", "epiano"], atmos=["sh"], drums=["snare", "hat"], swing=0.45, reverbs=[1],
      adj=["Two Step", "Shuffle", "Skippy", "Speed", "London", "Pirate", "Bumpy", "Swingin", "Rewind", "Sunday", "Dubplate", "Vocal", "Bubbling", "Late", "Crisp"]),
    G("Synthwave", dark=0.35, age=0.5, bass=["saw", "pluck", "sub"], lead=["pwm", "supersaw", "sync"], pad=["strings", "warm", "plate"],
      pluck=["pingpong", "classic"], stab=["brass", "house"], keys=["epiano", "bell"], atmos=["drone", "riser"], drums=["tom", "snare"],
      fx={"chorus": 0.7}, reverbs=[1, 0], delay_divs=[6, 8],
      adj=["Neon", "Retro", "Outrun", "Chrome", "Midnight", "Sunset", "Arcade", "Laser", "Vapor", "Miami", "Turbo", "Cassette", "Nightdrive", "Pastel", "Grid"]),
    G("Electro", dark=0.5, bass=["saw", "fm", "acid"], lead=["sync", "ring", "fm"], pad=["choir", "evolving"], pluck=["bite", "random"],
      stab=["techno", "rave"], keys=["clav", "bell"], atmos=["sh", "alien"], drums=["metal", "tom", "kick"], delay_divs=[5, 6],
      adj=["Robot", "Circuit", "Breakdance", "Vocoder", "Planet", "Bass Station", "Cyber", "Android", "Signal", "Freestyle", "Kraft", "Mainframe", "Modem", "Pixel", "Voltage"]),
    G("Acid", dark=0.5, bass=["acid", "acid", "acid", "saw"], lead=["acid", "sync"], pad=["evolving", "choir"], pluck=["bite", "bite", "cascade"],
      stab=["techno", "rave"], keys=["clav"], atmos=["sh", "alien"], drums=["kick", "hat"], fx={"fuzz": 0.35}, delay_divs=[6],
      counts={"Bass": 8, "Lead": 3, "Pad": 2, "Pluck": 3, "Stab": 2, "Keys": 1, "Arp": 3, "Atmos & FX": 2, "Drums": 1},
      adj=["Squelch", "Acid", "Rubber", "Silver", "Smiley", "Resonant", "Wet", "Liquid", "Chemical", "Bleep", "Bubble", "Chirp", "Slide", "Accent", "Juicy"]),
    G("Ambient", dark=0.45, age=0.5, space=1.0, bass=["sub", "saw"], lead=["flute", "pwm"], pad=["plate", "evolving", "tape", "fm_glass", "choir", "warm"],
      pluck=["fm", "pingpong", "classic"], stab=["dub"], keys=["bell", "vibes", "spring"], atmos=["drone", "wind", "ring", "riser"], drums=["metal"],
      counts={"Bass": 2, "Lead": 2, "Pad": 7, "Pluck": 3, "Stab": 1, "Keys": 3, "Arp": 2, "Atmos & FX": 5, "Drums": 0},
      fx={"echo": 0.3}, reverbs=[1, 0, 2],
      adj=["Glacial", "Weightless", "Ether", "Drift", "Hollow", "Morning", "Floating", "Tidal", "Cloud", "Stillness", "Meadow", "Fog", "Aether", "Slow", "Breathing"]),
    G("Downtempo & Lo-Fi", dark=0.6, age=0.9, bass=["sub", "pluck", "fm"], lead=["flute", "pwm"], pad=["tape", "warm", "strings"],
      pluck=["classic", "bp", "sub"], stab=["dub", "organ"], keys=["epiano", "spring", "vibes"], atmos=["drone", "wind"], drums=["snare", "hat"],
      fx={"echo": 0.5}, reverbs=[2, 1], swing=0.35,
      adj=["Dusty", "Cassette", "Rainy", "Sleepy", "Warm", "Vinyl", "Faded", "Hazy", "Bedroom", "Worn", "Mellow", "Sunday", "Tape", "Cozy", "Lazy"]),
    G("IDM", dark=0.5, bass=["fm", "acid", "sub"], lead=["fm", "ring", "sync"], pad=["fm_glass", "evolving"], pluck=["random", "fm", "bp"],
      stab=["techno"], keys=["bell", "clav"], atmos=["sh", "alien", "ring"], drums=["metal", "tom"], arp_modes=[3, 4], arp_rates=[6, 7],
      adj=["Glitch", "Fractured", "Modular", "Algorithm", "Broken", "Granular", "Stochastic", "Crystal", "Recursive", "Binary", "Quantum", "Cellular", "Scatter", "Mosaic", "Matrix"]),
    G("Future Bass", dark=0.3, bass=["808", "reese", "wobble"], lead=["supersaw", "fm", "pwm"], pad=["warm", "shoegaze", "plate"],
      pluck=["pingpong", "fm", "classic"], stab=["brass", "rave"], keys=["bell", "epiano"], atmos=["riser"], drums=["snare", "kick"],
      fx={"phaser": 0.2}, reverbs=[1, 0],
      adj=["Kawaii", "Candy", "Bloom", "Glow", "Feels", "Sparkle", "Heartbeat", "Pastel", "Colors", "Dreamy", "Shine", "Wave", "Supersonic", "Pop", "Bright"]),
    G("Dub Techno", dark=0.75, age=0.6, bass=["sub", "rolling"], lead=["pwm", "ring"], pad=["tape", "evolving", "choir"], pluck=["bp", "classic"],
      stab=["dub", "dub", "dub"], keys=["spring", "organ"], atmos=["drone", "wind"], drums=["hat", "metal"], fx={"echo": 0.7}, reverbs=[2, 1],
      counts={"Bass": 4, "Lead": 2, "Pad": 4, "Pluck": 3, "Stab": 5, "Keys": 2, "Arp": 2, "Atmos & FX": 2, "Drums": 1},
      adj=["Echo", "Haze", "Chamber", "Basic", "Submerged", "Fog", "Shadow", "Delay", "Resonance", "Depth", "Mist", "Space", "Tape", "Channel", "Grey"]),
]

NOUNS = {
    "Bass": ["Pressure", "Rumble", "Engine", "Current", "Anchor", "Undertow", "Foundation", "Low End", "Driver", "Body", "Weight", "Motion", "Core", "Roller", "Groundwork"],
    "Lead": ["Signal", "Voice", "Flare", "Ray", "Call", "Horn", "Cry", "Line", "Beacon", "Spark", "Blade", "Hymn", "Quest", "Shout", "Solo"],
    "Pad": ["Horizon", "Veil", "Tide", "Cathedral", "Haze", "Canvas", "Bloom", "Field", "Choir", "Dream", "Glow", "Ocean", "Cloud", "Atmosphere", "Blanket"],
    "Pluck": ["Drops", "Glint", "Shards", "Ripple", "Dew", "Pebbles", "Sparks", "Strings", "Rain", "Needles", "Beads", "Taps", "Petals", "Crystals", "Flicker"],
    "Stab": ["Stab", "Chords", "Hit", "Memory", "Punch", "Chop", "Accent", "Burst", "Voicing", "Minor", "Stack", "Slice", "Echo", "Pulse", "Shot"],
    "Keys": ["Keys", "Tines", "Organ", "Piano", "Bells", "Mallets", "Clav", "Chimes", "Upright", "Suitcase", "Glass", "Celesta", "Harp", "Lounge", "Parlour"],
    "Arp": ["Sequence", "Cycle", "Pattern", "Motion", "Runner", "Loop", "Ladder", "Orbit", "Spiral", "Stream", "Engine", "Steps", "Chase", "Gears", "Cascade"],
    "Atmos & FX": ["Drift", "Void", "Field", "Static", "Portal", "Transmission", "Riser", "Wind", "Abyss", "Nebula", "Signal", "Swell", "Machine", "Echoes", "Ghost"],
    "Drums": ["Kick", "Snare", "Hat", "Perc", "Tom", "Clank", "Knock", "Tick", "Hit", "Shaker"],
}
DRUM_NAMES = {"kick": "Kick", "snare": "Snare", "hat": "Hat", "metal": "Metal Perc", "tom": "Tom"}


def genre_touch(p, rng, g, role):
    """Genre-wide character: darkness, analog age, space, and the genre's favourite effects."""
    if "flt_cutoff" in p and role not in ("Drums",):
        p["flt_cutoff"] *= 1.35 - 0.7 * g["dark"] + u(rng, -0.08, 0.08)
    p.setdefault("analog_age", min(1.0, g["age"] + u(rng, -0.1, 0.1)))
    fx = g["fx"]
    if role in ("Pad", "Lead", "Pluck", "Keys", "Arp", "Stab", "Atmos & FX"):
        if "reverb_on" not in p and "echo_on" not in p and rng.random() < 0.75:
            p.update(reverb_on=1, reverb_type=rng.choice(g["reverbs"]), reverb_size=u(rng, 0.5, 0.95),
                     reverb_decay=u(rng, 2.0, 5.0) * (1.0 + g["space"]), reverb_mix=u(rng, 0.2, 0.35) * (0.8 + 0.5 * g["space"]))
        if "delay_on" not in p and "echo_on" not in p and role != "Pad" and rng.random() < 0.6:
            p.update(delay_on=1, delay_sync=1, delay_div=rng.choice(g["delay_divs"]), delay_pingpong=int(rng.random() < 0.5),
                     delay_fb=u(rng, 0.3, 0.55), delay_mix=u(rng, 0.15, 0.3))
        if "echo_on" not in p and rng.random() < fx.get("echo", 0.1):
            p.update(echo_on=1, echo_mode=rng.choice([3, 7, 8, 10]), echo_rate=u(rng, 0.25, 0.6), echo_intensity=u(rng, 0.4, 0.62),
                     echo_wow=u(rng, 0.3, 0.7), echo_volume=u(rng, 0.35, 0.55), echo_reverb=u(rng, 0.25, 0.45))
            p.pop("delay_on", None)
        if "fuzz_on" not in p and rng.random() < fx.get("fuzz", 0.05):
            p.update(fuzz_on=1, fuzz_sustain=u(rng, 0.35, 0.7), fuzz_tone=u(rng, 0.35, 0.55), fuzz_volume=0.6, fuzz_mix=u(rng, 0.25, 0.55))
        if "phaser_on" not in p and rng.random() < fx.get("phaser", 0.05):
            p.update(phaser_on=1, phaser_rate=u(rng, 0.05, 0.5), phaser_depth=u(rng, 0.6, 1.0), phaser_fb=u(rng, 0.3, 0.6), phaser_mix=0.4)
        if "chorus_on" not in p and rng.random() < fx.get("chorus", 0.25):
            p.update(chorus_on=1, chorus_mode=rng.choice([1, 2, 3]), chorus_mix=u(rng, 0.3, 0.5))
    # Performance: mod wheel opens the filter on most sounds.
    if "mm4_amt" not in p and role not in ("Drums",):
        p.mm(4, WHEEL, CUT, u(rng, 0.3, 0.5))
    return p


def clamp(key, value):
    if key in RANGES:
        lo, hi = RANGES[key]
        value = max(lo, min(hi, value))
    elif isinstance(value, float) and not key.startswith("mm"):
        value = max(0.0, min(1.0, value)) if key.endswith(("_mix", "_s", "_amt", "_depth", "_fb", "_sustain", "_tone", "_volume",
                                                            "_intensity", "_wow", "_input", "_reverb", "_rate", "_gate", "_swing")) \
            and key not in ("lfo_rate", "lfo2_rate", "phaser_rate", "chorus_rate") else value
    return value


def write(path, name, genre, role, target, p):
    lines = ['<?xml version="1.0" encoding="UTF-8"?>',
             '<AUGUR5_PRESET formatVersion="1" name=%s category=%s target="%.1f" genre=%s pack=%s author="TONAL LAB">' % (
                 quoteattr(name), quoteattr(role), target, quoteattr(genre), quoteattr(PACK)),
             "  <PARAMS>"]
    for key in sorted(p):
        v = clamp(key, p[key])
        lines.append('    <P id="%s" value="%s"/>' % (key, ("%.4f" % v).rstrip("0").rstrip(".") if isinstance(v, float) else str(v)))
    lines.append("  </PARAMS>")
    lines.append("</AUGUR5_PRESET>")
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main():
    if OUT.exists():
        shutil.rmtree(OUT)
    used = set()
    total = 0
    for gi, g in enumerate(GENRES, 1):
        folder = OUT / ("%02d %s" % (gi, g["name"]))
        folder.mkdir(parents=True)
        rng = random.Random(0xA5 * 1000 + gi)
        for role, count in g["counts"].items():
            for k in range(count):
                if role == "Drums":
                    kind = g["drums"][k % len(g["drums"])]
                    p, target = drum(rng, g, kind)
                    noun = DRUM_NAMES[kind]
                else:
                    kinds = g[{"Bass": "bass", "Lead": "lead", "Pad": "pad", "Pluck": "pluck", "Stab": "stab", "Keys": "keys",
                               "Atmos & FX": "atmos", "Arp": "pluck"}[role]]
                    kind = kinds[k % len(kinds)] if role != "Arp" else ("latch" if k == 2 else "arp")
                    p = ROLE_FN[role](rng, g, kind)
                    target = -18.0
                    noun = rng.choice(NOUNS[role])
                p = genre_touch(p, rng, g, role)
                for _ in range(200):
                    name = "%s %s" % (rng.choice(g["adj"]), noun if role == "Drums" else rng.choice(NOUNS[role]))
                    if name not in used:
                        break
                used.add(name)
                write(folder / ("%s %s.augur5" % (ROLE_PREFIX[role], name)), name, g["name"], role, target, p)
                total += 1
    (OUT / "README.txt").write_text(README, encoding="utf-8")
    print("%d presets in %d genres -> %s" % (total, len(GENRES), OUT))


README = """AUGUR-5 ANTHOLOGY VOL.1  -  TONAL LAB
500 presets for AUGUR-5 "3340" across 20 styles of electronic music.

INSTALL
  In AUGUR-5: BROWSER > Install expansion pack... and choose this .zip file.
  The sounds appear under BROWSER > USER / EXPANSIONS > AUGUR-5 Anthology Vol.1.
  (Or unzip the folder into your AUGUR-5 presets folder: BROWSER > Open presets folder.)

NAMES
  BA bass  LD lead  PD pad  PL pluck  CH chord / stab  KY keys  AR arpeggio  FX atmosphere  DR drums
  Every sound is loudness-matched. The MOD WHEEL opens the filter on most of them.
  Delays and LFOs follow your project tempo.

INSTALAR (ES)
  En AUGUR-5: BROWSER > Install expansion pack... y elige este .zip.
  Los sonidos aparecen en BROWSER > USER / EXPANSIONS > AUGUR-5 Anthology Vol.1.

(c) TONAL LAB. Licensed for use in your music productions; redistribution of the preset files is not permitted.
"""

if __name__ == "__main__":
    main()
