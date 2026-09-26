// AUGUR-5 "3340" — Parameter IDs (generated from the GUI design)
// Each ID matches the `param` of a knob/toggle in the design (show them with the "showIds" tweak).
// These IDs are stored in every DAW project and preset: never rename, reuse or remove one once released.
// Parameters are registered in the APVTS layout phase by phase, as their DSP is implemented.
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

namespace augur5::params
{
    // ---------- OSCILLATORS ----------
    inline constexpr auto osc1_freq   = "osc1_freq";    // semitones  -24..+24   default 0
    inline constexpr auto osc1_fine   = "osc1_fine";    // cents      -50..+50   default 0
    inline constexpr auto osc1_pw     = "osc1_pw";      // %           5..95     default 50
    inline constexpr auto osc1_saw    = "osc1_saw";     // bool                  default true
    inline constexpr auto osc1_pulse  = "osc1_pulse";   // bool                  default false
    inline constexpr auto osc1_sync   = "osc1_sync";    // bool (A synced to B)  default false

    inline constexpr auto osc2_freq   = "osc2_freq";
    inline constexpr auto osc2_fine   = "osc2_fine";
    inline constexpr auto osc2_pw     = "osc2_pw";
    inline constexpr auto osc2_saw    = "osc2_saw";
    inline constexpr auto osc2_tri    = "osc2_tri";
    inline constexpr auto osc2_pulse  = "osc2_pulse";
    inline constexpr auto osc2_lofreq = "osc2_lofreq";  // bool: OSC 2 as LFO
    inline constexpr auto osc2_kbd    = "osc2_kbd";     // bool: keyboard tracking  default true

    // ---------- MIXER ----------
    inline constexpr auto mix_osc1  = "mix_osc1";       // 0..1
    inline constexpr auto mix_osc2  = "mix_osc2";
    inline constexpr auto mix_noise = "mix_noise";
    inline constexpr auto mix_drive = "mix_drive";      // saturation into the filter

    // ---------- FILTER ----------
    inline constexpr auto flt_cutoff   = "flt_cutoff";   // Hz 20..20000 (skewed)
    inline constexpr auto flt_reso     = "flt_reso";     // 0..1 (self-osc near 1)
    inline constexpr auto flt_env_amt  = "flt_env_amt";  // -1..+1
    inline constexpr auto flt_model    = "flt_model";    // choice: 0 = REV3 (CEM3320), 1 = REV1 (SSM2040), 2 = CASCADE, 3 = MULTIMODE, 4 = BITE
    inline constexpr auto flt_slope    = "flt_slope";    // choice: 0 = 24 dB, 1 = 12 dB
    inline constexpr auto flt_mode     = "flt_mode";     // choice: 0 = LP, 1 = BP, 2 = HP
    inline constexpr auto hpf_cutoff   = "hpf_cutoff";   // Hz 10..2000 (10 = off)
    inline constexpr auto flt_keytrack = "flt_keytrack"; // choice: 0 off, 1 half, 2 full
    inline constexpr auto flt_velocity = "flt_velocity"; // 0..1

    // ---------- AMPLIFIER ----------
    inline constexpr auto amp_level    = "amp_level";    // dB -inf..+6
    inline constexpr auto amp_velocity = "amp_velocity"; // 0..1
    inline constexpr auto at_amount    = "at_amount";    // aftertouch 0..1

    // ---------- ENVELOPES (CEM3310-style RC curves) ----------
    inline constexpr auto fenv_a = "fenv_a"; // s 0.001..10 (skewed)
    inline constexpr auto fenv_d = "fenv_d"; // s 0.001..10
    inline constexpr auto fenv_s = "fenv_s"; // 0..1
    inline constexpr auto fenv_r = "fenv_r"; // s 0.001..10
    inline constexpr auto aenv_a = "aenv_a";
    inline constexpr auto aenv_d = "aenv_d";
    inline constexpr auto aenv_s = "aenv_s";
    inline constexpr auto aenv_r = "aenv_r";

    // ---------- LFO ----------
    inline constexpr auto lfo_rate   = "lfo_rate";   // Hz 0.05..30
    inline constexpr auto lfo_amount = "lfo_amount"; // 0..1
    inline constexpr auto lfo_delay  = "lfo_delay";  // s 0..5
    inline constexpr auto lfo_wave   = "lfo_wave";   // choice: tri, saw, sqr, s&h
    inline constexpr auto lfo_sync   = "lfo_sync";   // bool: tempo sync

    // ---------- POLY MOD ----------
    inline constexpr auto pm_on         = "pm_on";
    inline constexpr auto pm_fenv_amt   = "pm_fenv_amt";   // 0..1
    inline constexpr auto pm_osc2_amt   = "pm_osc2_amt";   // 0..1
    inline constexpr auto pm_dst_freqa  = "pm_dst_freqa";  // bool
    inline constexpr auto pm_dst_pwa    = "pm_dst_pwa";    // bool
    inline constexpr auto pm_dst_filter = "pm_dst_filter"; // bool

    // ---------- MOD MATRIX (4 slots) ----------
    // mmN_src: choice {FILTER ENV, AMP ENV, OSC 2, LFO, MOD WHEEL, VELOCITY, AFTERTOUCH, NOISE}
    // mmN_dst: choice {OSC 1 FREQ, OSC 2 FREQ, OSC 1 PW, OSC 2 PW, FILTER CUTOFF, RESONANCE, AMP LEVEL, LFO RATE}
    // mmN_amt: -1..+1
    inline juce::String mmSrc (int slot) { return "mm" + juce::String (slot) + "_src"; }
    inline juce::String mmDst (int slot) { return "mm" + juce::String (slot) + "_dst"; }
    inline juce::String mmAmt (int slot) { return "mm" + juce::String (slot) + "_amt"; }
    inline constexpr int kNumMatrixSlots = 8; // slots 5..8 were added with the modulation expansion

    // ---------- PER VOICE / VINTAGE ----------
    inline constexpr auto voice_detune = "voice_detune"; // 0..1 (per-voice analog spread)
    inline constexpr auto voice_spread = "voice_spread"; // stereo spread 0..1
    inline constexpr auto voice_pan    = "voice_pan";    // -1..+1
    inline constexpr auto voice_count  = "voice_count";  // int 1..16, default 5
    inline constexpr auto analog_age   = "analog_age";   // 0 = freshly calibrated .. 1 = worn vintage

    // ---------- EFFECTS ----------
    inline constexpr auto chorus_on = "chorus_on", chorus_rate = "chorus_rate", chorus_depth = "chorus_depth", chorus_mix = "chorus_mix";
    inline constexpr auto delay_on  = "delay_on",  delay_time  = "delay_time",  delay_fb     = "delay_fb",     delay_mix  = "delay_mix";
    inline constexpr auto reverb_on = "reverb_on", reverb_size = "reverb_size", reverb_decay = "reverb_decay", reverb_mix = "reverb_mix";

    // ---------- GLOBAL ----------
    inline constexpr auto master_tune = "master_tune"; // cents -100..+100
    inline constexpr auto glide       = "glide";       // s 0..5
    inline constexpr auto unison      = "unison";      // bool
    inline constexpr auto legato      = "legato";      // bool

    // ---------- ADDITIONAL (not on the panel; reachable from SETTINGS) ----------
    inline constexpr auto osc_model = "osc_model"; // choice: 0 = REV3 (CEM3340), 1 = REV1 (SSM2030)
    inline constexpr auto pb_range  = "pb_range";  // semitones 0..24, default 2
    inline constexpr auto vintage_cv = "vintage_cv"; // bool: Rev 3 7-bit knob digitising (128 steps + hysteresis)

    // ---------- OSCILLATOR EXTRAS ----------
    inline constexpr auto mix_ring  = "mix_ring";  // 0..1 ring modulator (OSC A x OSC B) level
    inline constexpr auto mix_sub   = "mix_sub";   // 0..1 sub oscillator level
    inline constexpr auto sub_oct   = "sub_oct";   // choice: -1 OCT, -2 OCT
    inline constexpr auto osc_xmod  = "osc_xmod";  // 0..1 linear FM OSC B -> OSC A

    // ---------- LFO 2 / MOD ENV ----------
    inline constexpr auto lfo2_rate   = "lfo2_rate";   // Hz 0.05..30 (division when synced)
    inline constexpr auto lfo2_wave   = "lfo2_wave";   // choice: SINE, TRI, SAW UP, SAW DN, SQR, S&H, SMOOTH
    inline constexpr auto lfo2_sync   = "lfo2_sync";   // bool
    inline constexpr auto lfo2_retrig = "lfo2_retrig"; // bool: restart on every note (poly) / free
    inline constexpr auto menv_a = "menv_a", menv_d = "menv_d", menv_s = "menv_s", menv_r = "menv_r";

    // ---------- EFFECTS (expansion) ----------
    inline constexpr auto fuzz_on = "fuzz_on", fuzz_sustain = "fuzz_sustain", fuzz_tone = "fuzz_tone",
                          fuzz_volume = "fuzz_volume", fuzz_mix = "fuzz_mix";
    inline constexpr auto phaser_on = "phaser_on", phaser_rate = "phaser_rate", phaser_depth = "phaser_depth",
                          phaser_fb = "phaser_fb", phaser_mix = "phaser_mix";
    inline constexpr auto chorus_mode    = "chorus_mode";    // choice: FREE, I, II, I+II
    inline constexpr auto delay_sync     = "delay_sync";     // bool
    inline constexpr auto delay_div      = "delay_div";      // choice: 1/32 .. 1 BAR (when synced)
    inline constexpr auto delay_pingpong = "delay_pingpong"; // bool
    inline constexpr auto reverb_type    = "reverb_type";    // choice: HALL, PLATE, SPRING

    // ---------- TAPE ECHO (multi-head tape loop + spring) ----------
    inline constexpr auto echo_on = "echo_on", echo_mode = "echo_mode", echo_rate = "echo_rate", echo_intensity = "echo_intensity",
                          echo_bass = "echo_bass", echo_treble = "echo_treble", echo_wow = "echo_wow", echo_input = "echo_input",
                          echo_volume = "echo_volume", echo_reverb = "echo_reverb";

    // ---------- VOICE MODE / TRIMS ----------
    inline constexpr auto voice_mode = "voice_mode"; // choice: POLY, DUO
    inline juce::String trimTune (int voice) { return "trim_tune_" + juce::String (voice); } // cents -50..50, voices 1..8
    inline juce::String trimCut (int voice) { return "trim_cut_" + juce::String (voice); }   // octaves -1..1
    inline constexpr int kNumTrims = 8;

    // ---------- OSCILLATOR OCTAVE ----------
    inline constexpr auto osc1_oct = "osc1_oct"; // choice: -2, -1, 0, +1, +2 octaves (default 0)
    inline constexpr auto osc2_oct = "osc2_oct";

    // ---------- QUALITY ----------
    inline constexpr auto quality         = "quality";         // choice: ECO (1x), GREAT (2x), DIVINE (4x)
    inline constexpr auto offline_quality = "offline_quality"; // choice: SAME, DIVINE (used while the host renders offline)

    // ---------- ARPEGGIATOR ----------
    inline constexpr auto arp_on    = "arp_on";    // bool
    inline constexpr auto arp_mode  = "arp_mode";  // choice: UP, DOWN, UP-DOWN, RANDOM, ORDER
    inline constexpr auto arp_oct   = "arp_oct";   // int 1..4
    inline constexpr auto arp_rate  = "arp_rate";  // choice: 1/4 .. 1/32 (host tempo)
    inline constexpr auto arp_gate  = "arp_gate";  // 0.02..1 of a step
    inline constexpr auto arp_swing = "arp_swing"; // 0..0.5 of a step
    inline constexpr auto arp_latch = "arp_latch"; // bool
}
