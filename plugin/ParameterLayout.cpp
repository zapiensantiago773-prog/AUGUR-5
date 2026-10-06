#include "ParameterLayout.h"
#include "Parameters.h"

namespace augur5
{

namespace
{
using APF = juce::AudioParameterFloat;
using APB = juce::AudioParameterBool;
using APC = juce::AudioParameterChoice;
using API = juce::AudioParameterInt;
using Range = juce::NormalisableRange<float>;

constexpr int version = 1; // ParameterID version hint for AU/VST3 hosts; bump only for new parameters

juce::ParameterID pid (const juce::String& id) { return { id, version }; }

// Logarithmic range: equal knob travel per octave / decade (cutoff, times, rates).
Range logRange (float lo, float hi)
{
    const float ratio = std::log (hi / lo);
    return Range (lo, hi,
                  [lo, ratio] (float, float, float v) { return lo * std::exp (v * ratio); },
                  [lo, ratio] (float, float, float x) { return std::log (juce::jmax (x, lo) / lo) / ratio; });
}

juce::String formatHz (float v, int)
{
    return v >= 1000.0f ? juce::String (v / 1000.0f, 2) + " kHz" : juce::String (v, v < 10.0f ? 2 : 1) + " Hz";
}

juce::String formatSeconds (float v, int)
{
    return v < 1.0f ? juce::String (v * 1000.0f, v < 0.01f ? 1 : 0) + " ms" : juce::String (v, 2) + " s";
}

juce::String formatPercent (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + " %"; }
juce::String formatBipolar (float v, int) { return (v > 0.0f ? "+" : "") + juce::String (juce::roundToInt (v * 100.0f)) + " %"; }

std::unique_ptr<APF> percent (const juce::String& id, const juce::String& name, float def)
{
    return std::make_unique<APF> (pid (id), name, Range (0.0f, 1.0f), def,
                                  juce::AudioParameterFloatAttributes().withStringFromValueFunction (formatPercent));
}

std::unique_ptr<APF> bipolar (const juce::String& id, const juce::String& name, float def)
{
    return std::make_unique<APF> (pid (id), name, Range (-1.0f, 1.0f), def,
                                  juce::AudioParameterFloatAttributes().withStringFromValueFunction (formatBipolar));
}

std::unique_ptr<APF> seconds (const juce::String& id, const juce::String& name, float lo, float hi, float def)
{
    return std::make_unique<APF> (pid (id), name, logRange (lo, hi), def,
                                  juce::AudioParameterFloatAttributes().withStringFromValueFunction (formatSeconds));
}

std::unique_ptr<APF> hertz (const juce::String& id, const juce::String& name, float lo, float hi, float def)
{
    return std::make_unique<APF> (pid (id), name, logRange (lo, hi), def,
                                  juce::AudioParameterFloatAttributes().withStringFromValueFunction (formatHz));
}

std::unique_ptr<APB> toggle (const juce::String& id, const juce::String& name, bool def)
{
    return std::make_unique<APB> (pid (id), name, def);
}
} // namespace

const juce::StringArray& matrixSourceNames()
{
    static const juce::StringArray names { "FILTER ENV", "AMP ENV", "OSC 2", "LFO", "MOD WHEEL", "VELOCITY", "AFTERTOUCH", "NOISE",
                                           "MOD ENV", "LFO 2", "KEYTRACK", "NOTE RANDOM" };
    return names;
}

const juce::StringArray& matrixDestNames()
{
    static const juce::StringArray names { "OSC 1 FREQ", "OSC 2 FREQ", "OSC 1 PW", "OSC 2 PW", "FILTER CUTOFF", "RESONANCE", "AMP LEVEL", "LFO RATE",
                                           "FM AMOUNT", "RING LEVEL", "SUB LEVEL", "DRIVE", "OSC 1 LEVEL", "OSC 2 LEVEL", "NOISE LEVEL", "LFO 2 RATE" };
    return names;
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    namespace P = params;
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    const auto semis = juce::AudioParameterIntAttributes().withStringFromValueFunction ([] (int v, int) {
        return (v > 0 ? "+" : "") + juce::String (v) + " st";
    });
    const auto cents = juce::AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) {
        return (v > 0.0f ? "+" : "") + juce::String (v, 1) + " ct";
    });
    const auto pwText = juce::AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) {
        return juce::String (juce::roundToInt (v)) + " %";
    });

    // Oscillators
    layout.add (std::make_unique<API> (pid (P::osc1_freq), "Osc 1 Frequency", -24, 24, 0, semis));
    layout.add (std::make_unique<APF> (pid (P::osc1_fine), "Osc 1 Fine", Range (-50.0f, 50.0f), 0.0f, cents));
    layout.add (std::make_unique<APF> (pid (P::osc1_pw), "Osc 1 Width", Range (5.0f, 95.0f), 50.0f, pwText));
    layout.add (toggle (P::osc1_saw, "Osc 1 Saw", true));
    layout.add (toggle (P::osc1_pulse, "Osc 1 Pulse", false));
    layout.add (toggle (P::osc1_sync, "Osc 1 Sync", false));

    layout.add (std::make_unique<API> (pid (P::osc2_freq), "Osc 2 Frequency", -24, 24, 0, semis));
    layout.add (std::make_unique<APF> (pid (P::osc2_fine), "Osc 2 Fine", Range (-50.0f, 50.0f), 0.0f, cents));
    layout.add (std::make_unique<APF> (pid (P::osc2_pw), "Osc 2 Width", Range (5.0f, 95.0f), 50.0f, pwText));
    layout.add (toggle (P::osc2_saw, "Osc 2 Saw", true));
    layout.add (toggle (P::osc2_tri, "Osc 2 Triangle", false));
    layout.add (toggle (P::osc2_pulse, "Osc 2 Pulse", false));
    layout.add (toggle (P::osc2_lofreq, "Osc 2 Lo Freq", false));
    layout.add (toggle (P::osc2_kbd, "Osc 2 Keyboard", true));

    // Mixer
    layout.add (percent (P::mix_osc1, "Mixer Osc 1", 0.8f));
    layout.add (percent (P::mix_osc2, "Mixer Osc 2", 0.6f));
    layout.add (percent (P::mix_noise, "Mixer Noise", 0.0f));
    layout.add (percent (P::mix_drive, "Mixer Drive", 0.2f));
    layout.add (percent (P::mix_ring, "Ring Mod Level", 0.0f));
    layout.add (percent (P::mix_sub, "Sub Osc Level", 0.0f));
    layout.add (std::make_unique<APC> (pid (P::sub_oct), "Sub Osc Octave", juce::StringArray { "-1 OCT", "-2 OCT" }, 0));
    layout.add (percent (P::osc_xmod, "Cross Mod (FM)", 0.0f));

    // LFO 2 / Mod Env
    layout.add (hertz (P::lfo2_rate, "LFO 2 Rate", 0.05f, 30.0f, 2.0f));
    layout.add (std::make_unique<APC> (pid (P::lfo2_wave), "LFO 2 Wave",
                                       juce::StringArray { "SINE", "TRI", "SAW UP", "SAW DN", "SQR", "S&H", "SMOOTH" }, 0));
    layout.add (toggle (P::lfo2_sync, "LFO 2 Sync", false));
    layout.add (toggle (P::lfo2_retrig, "LFO 2 Retrigger", true));
    layout.add (seconds (P::menv_a, "Mod Env Attack", 0.001f, 10.0f, 0.01f));
    layout.add (seconds (P::menv_d, "Mod Env Decay", 0.001f, 10.0f, 0.5f));
    layout.add (percent (P::menv_s, "Mod Env Sustain", 0.0f));
    layout.add (seconds (P::menv_r, "Mod Env Release", 0.001f, 10.0f, 0.5f));

    // Filter
    layout.add (hertz (P::flt_cutoff, "Filter Cutoff", 20.0f, 20000.0f, 2500.0f));
    layout.add (percent (P::flt_reso, "Filter Resonance", 0.15f));
    layout.add (bipolar (P::flt_env_amt, "Filter Env Amount", 0.35f));
    layout.add (std::make_unique<APC> (pid (P::flt_model), "Filter Model", juce::StringArray { "REV 3 (CEM3320)", "REV 1 (SSM2040)", "CASCADE", "MULTIMODE", "BITE" }, 0));
    layout.add (std::make_unique<APC> (pid (P::flt_slope), "Filter Slope", juce::StringArray { "24 dB", "12 dB" }, 0));
    layout.add (std::make_unique<APC> (pid (P::flt_mode), "Filter Mode", juce::StringArray { "LP", "BP", "HP" }, 0));
    layout.add (std::make_unique<APF> (pid (P::hpf_cutoff), "HPF Cutoff", logRange (10.0f, 2000.0f), 10.0f,
                                       juce::AudioParameterFloatAttributes().withStringFromValueFunction (
                                           [] (float v, int n) { return v <= 10.5f ? juce::String ("OFF") : formatHz (v, n); })));
    layout.add (std::make_unique<APC> (pid (P::flt_keytrack), "Filter Key Track", juce::StringArray { "OFF", "HALF", "FULL" }, 2));
    layout.add (percent (P::flt_velocity, "Velocity to Filter", 0.0f));

    // Amplifier
    layout.add (std::make_unique<APF> (pid (P::master_volume), "Master Volume", Range (-60.0f, 6.0f, 0.1f, 2.5f), 0.0f,
                                       juce::AudioParameterFloatAttributes().withStringFromValueFunction (
                                           [] (float v, int) { return v <= -59.9f ? juce::String ("-inf dB") : juce::String (v, 1) + " dB"; })));
    layout.add (std::make_unique<APF> (pid (P::amp_level), "Level", Range (-60.0f, 6.0f, 0.1f), -6.0f,
                                       juce::AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) {
                                           return v <= -59.9f ? juce::String ("-inf dB") : juce::String (v, 1) + " dB";
                                       })));
    layout.add (percent (P::amp_velocity, "Velocity to Amp", 0.5f));
    layout.add (percent (P::at_amount, "Aftertouch", 0.0f));

    // Envelopes
    layout.add (seconds (P::fenv_a, "Filter Env Attack", 0.001f, 10.0f, 0.005f));
    layout.add (seconds (P::fenv_d, "Filter Env Decay", 0.001f, 10.0f, 0.6f));
    layout.add (percent (P::fenv_s, "Filter Env Sustain", 0.3f));
    layout.add (seconds (P::fenv_r, "Filter Env Release", 0.001f, 10.0f, 0.5f));
    layout.add (seconds (P::aenv_a, "Amp Env Attack", 0.001f, 10.0f, 0.003f));
    layout.add (seconds (P::aenv_d, "Amp Env Decay", 0.001f, 10.0f, 0.8f));
    layout.add (percent (P::aenv_s, "Amp Env Sustain", 0.8f));
    layout.add (seconds (P::aenv_r, "Amp Env Release", 0.001f, 10.0f, 0.4f));

    // LFO
    layout.add (hertz (P::lfo_rate, "LFO Rate", 0.05f, 30.0f, 4.0f));
    layout.add (percent (P::lfo_amount, "LFO Amount", 0.5f));
    layout.add (std::make_unique<APF> (pid (P::lfo_delay), "LFO Delay", Range (0.0f, 5.0f, 0.0f, 0.4f), 0.0f,
                                       juce::AudioParameterFloatAttributes().withStringFromValueFunction (formatSeconds)));
    layout.add (std::make_unique<APC> (pid (P::lfo_wave), "LFO Wave", juce::StringArray { "TRI", "SAW", "SQR", "S&H" }, 0));
    layout.add (toggle (P::lfo_sync, "LFO Sync", false));

    // Poly Mod
    layout.add (toggle (P::pm_on, "Poly Mod On", false));
    layout.add (percent (P::pm_fenv_amt, "Poly Mod Filter Env", 0.0f));
    layout.add (percent (P::pm_osc2_amt, "Poly Mod Osc 2", 0.0f));
    layout.add (toggle (P::pm_dst_freqa, "Poly Mod to Freq A", true));
    layout.add (toggle (P::pm_dst_pwa, "Poly Mod to PW A", false));
    layout.add (toggle (P::pm_dst_filter, "Poly Mod to Filter", false));

    // Mod matrix (defaults mirror the panel design, with zero amount)
    const int defaultSrc[] = { 0, 2, 3, 4, 8, 9, 5, 11 };
    const int defaultDst[] = { 0, 2, 4, 6, 4, 4, 4, 1 };
    for (int slot = 1; slot <= P::kNumMatrixSlots; ++slot)
    {
        const auto n = juce::String (slot);
        layout.add (std::make_unique<APC> (pid (P::mmSrc (slot)), "Matrix " + n + " Source", matrixSourceNames(), defaultSrc[slot - 1]));
        layout.add (std::make_unique<APC> (pid (P::mmDst (slot)), "Matrix " + n + " Destination", matrixDestNames(), defaultDst[slot - 1]));
        layout.add (bipolar (P::mmAmt (slot), "Matrix " + n + " Amount", 0.0f));
    }

    // Per voice / vintage
    layout.add (percent (P::voice_detune, "Voice Detune", 0.3f));
    layout.add (percent (P::voice_spread, "Voice Spread", 0.3f));
    layout.add (bipolar (P::voice_pan, "Pan", 0.0f));
    layout.add (std::make_unique<API> (pid (P::voice_count), "Voices", 1, 16, 5));
    layout.add (percent (P::analog_age, "Analog Age", 0.35f));

    // Effects
    layout.add (toggle (P::delay_on, "Delay On", false));
    layout.add (seconds (P::delay_time, "Delay Time", 0.01f, 2.0f, 0.375f));
    layout.add (percent (P::delay_fb, "Delay Feedback", 0.35f));
    layout.add (percent (P::delay_mix, "Delay Mix", 0.3f));

    // Global
    layout.add (std::make_unique<APF> (pid (P::master_tune), "Master Tune", Range (-100.0f, 100.0f), 0.0f, cents));
    layout.add (std::make_unique<APF> (pid (P::glide), "Glide", Range (0.0f, 5.0f, 0.0f, 0.35f), 0.0f,
                                       juce::AudioParameterFloatAttributes().withStringFromValueFunction (formatSeconds)));
    layout.add (toggle (P::unison, "Unison", false));
    layout.add (toggle (P::legato, "Legato", false));

    // Additional
    layout.add (std::make_unique<APC> (pid (P::osc_model), "VCO Model", juce::StringArray { "REV 3 (CEM3340)", "REV 1 (SSM2030)" }, 0));
    layout.add (std::make_unique<API> (pid (P::pb_range), "Pitch Bend Range", 0, 24, 2, semis));
    layout.add (toggle (P::vintage_cv, "Vintage 7-bit Knobs", false));

    // Effects (expansion)
    layout.add (toggle (P::fuzz_on, "Fuzz On", false));
    layout.add (percent (P::fuzz_sustain, "Fuzz Sustain", 0.6f));
    layout.add (percent (P::fuzz_tone, "Fuzz Tone", 0.5f));
    layout.add (percent (P::fuzz_volume, "Fuzz Volume", 0.5f));
    layout.add (percent (P::fuzz_mix, "Fuzz Mix", 1.0f));
    layout.add (toggle (P::delay_sync, "Delay Sync", false));
    {
        juce::StringArray divs;
        for (auto* n : augur::delaySyncNames)
            divs.add (n);
        layout.add (std::make_unique<APC> (pid (P::delay_div), "Delay Division", divs, 6));
    }
    layout.add (toggle (P::delay_pingpong, "Delay Ping-Pong", false));
    // ---- Effects rack (shared with PYTHIA 32) ----
    {
        // Every read-out rounds first and has its inverse: hosts let users type values ("1.2 kHz", "45 %").
        using Fmt = std::function<juce::String (float, int)>;
        using Parse = std::function<float (const juce::String&)>;
        const auto rounded = [] (double v, int decimals) {
            const double scale = std::pow (10.0, decimals);
            const double r = std::round (v * scale) / scale;
            return r == 0.0 ? 0.0 : r;
        };
        const auto fixed = [rounded] (double v, int decimals) { return juce::String (rounded (v, decimals), decimals); };
        const auto numberIn = [] (const juce::String& t) { return t.trim().retainCharacters ("+-0123456789.eE").getFloatValue(); };
        const Parse plain = numberIn;
        const Parse fromPct = [numberIn] (const juce::String& t) { return numberIn (t) / 100.0f; };
        const Parse fromHz = [numberIn] (const juce::String& t) { return t.toLowerCase().contains ("k") ? numberIn (t) * 1000.0f : numberIn (t); };
        const Parse fromMs = [numberIn] (const juce::String& t) {
            const auto x = t.trim().toLowerCase();
            return x.endsWith ("ms") || ! x.endsWith ("s") ? numberIn (x) : numberIn (x) * 1000.0f;
        };
        const Fmt pct = [] (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + " %"; };
        const Fmt pctBi = [] (float v, int) { const int r = juce::roundToInt (v * 100.0f); return juce::String (r > 0 ? "+" : "") + juce::String (r) + " %"; };
        const Fmt dB = [rounded, fixed] (float v, int) { return juce::String (rounded (v, 1) > 0.0 ? "+" : "") + fixed (v, 1) + " dB"; };
        const auto scaled = [rounded, fixed] (double v, const char* unit, const char* bigUnit) {
            if (rounded (v, 2) < 10.0)
                return fixed (v, 2) + " " + unit;
            if (rounded (v, 0) < 1000.0)
                return fixed (v, 0) + " " + unit;
            return fixed (v / 1000.0, 2) + " " + bigUnit;
        };
        const Fmt hz = [scaled] (float v, int) { return scaled (v, "Hz", "kHz"); };
        const Fmt ms = [scaled] (float v, int) { return scaled (v, "ms", "s"); };
        const Fmt sec = [rounded, fixed] (float v, int) { return rounded (v, 2) < 10.0 ? fixed (v, 2) + " s" : fixed (v, 1) + " s"; };
        const Fmt times = [fixed] (float v, int) { return fixed (v, 2) + juce::String::fromUTF8 (" \xc3\x97"); };
        const Fmt deg = [] (float v, int) { return juce::String (juce::roundToInt (v)) + juce::String::fromUTF8 ("\xc2\xb0"); };
        const auto skewed = [] (float lo, float hi, float centre) { Range r (lo, hi); r.setSkewForCentre (centre); return r; };
        const auto addFmt = [&] (const char* id, const char* name, Range r, float def, Fmt fmt, Parse parse) {
            layout.add (std::make_unique<APF> (pid (id), name, r, def,
                                               juce::AudioParameterFloatAttributes().withStringFromValueFunction (std::move (fmt)).withValueFromStringFunction (std::move (parse))));
        };
        const auto addB = [&] (const char* id, const char* name, bool def) { layout.add (toggle (id, name, def)); };
        const auto addC = [&] (const char* id, const char* name, const juce::StringArray& c, int def) {
            layout.add (std::make_unique<APC> (pid (id), name, c, def));
        };

        addB (P::fx_drive_on, "Drive On", false);
        addC (P::fx_drive_model, "Drive Model", P::driveModels(), 0);
        addFmt (P::fx_drive_amount, "Drive Amount", { 0.0f, 48.0f }, 12.0f, dB, plain);
        addFmt (P::fx_drive_bias, "Drive Bias", { -1.0f, 1.0f }, 0.0f, pctBi, fromPct);
        addFmt (P::fx_drive_tone, "Drive Tone", { 0.0f, 1.0f }, 0.6f, pct, fromPct);
        addFmt (P::fx_drive_output, "Drive Output", { -24.0f, 12.0f }, 0.0f, dB, plain);
        addFmt (P::fx_drive_mix, "Drive Mix", { 0.0f, 1.0f }, 1.0f, pct, fromPct);

        addB (P::fx_chorus_on, "Chorus On", false);
        addC (P::fx_chorus_mode, "Chorus Mode", P::chorusModes(), 0);
        addFmt (P::fx_chorus_rate, "Chorus Rate", skewed (0.25f, 4.0f, 1.0f), 1.0f, times, plain);
        addFmt (P::fx_chorus_depth, "Chorus Depth", { 0.0f, 2.0f }, 1.0f, pct, fromPct);
        addFmt (P::fx_chorus_tone, "Chorus Tone", { 0.0f, 1.0f }, 0.5f, pct, fromPct);
        addFmt (P::fx_chorus_hiss, "Chorus Hiss", { 0.0f, 1.0f }, 0.1f, pct, fromPct);
        addFmt (P::fx_chorus_width, "Chorus Width", { 0.0f, 1.5f }, 1.0f, pct, fromPct);
        addFmt (P::fx_chorus_mix, "Chorus Mix", { 0.0f, 1.0f }, 0.5f, pct, fromPct);

        addB (P::fx_phaser_on, "Phaser On", false);
        addC (P::fx_phaser_stages, "Phaser Stages", P::phaserStages(), 1);
        addFmt (P::fx_phaser_rate, "Phaser Rate", skewed (0.02f, 10.0f, 0.6f), 0.3f, hz, fromHz);
        addB (P::fx_phaser_sync, "Phaser Sync", false);
        addC (P::fx_phaser_division, "Phaser Note", P::noteValues(), 9);
        addFmt (P::fx_phaser_depth, "Phaser Depth", { 0.0f, 1.0f }, 0.8f, pct, fromPct);
        addFmt (P::fx_phaser_center, "Phaser Manual", skewed (100.0f, 5000.0f, 700.0f), 700.0f, hz, fromHz);
        addFmt (P::fx_phaser_feedback, "Phaser Color", { -0.95f, 0.95f }, 0.3f, pctBi, fromPct);
        addFmt (P::fx_phaser_spread, "Phaser Spread", { 0.0f, 180.0f }, 90.0f, deg, plain);
        addC (P::fx_phaser_lfo, "Phaser LFO", P::phaserLfos(), 1);
        addFmt (P::fx_phaser_mix, "Phaser Mix", { 0.0f, 1.0f }, 0.5f, pct, fromPct);

        addB (P::fx_flanger_on, "Flanger On", false);
        addFmt (P::fx_flanger_rate, "Flanger Rate", skewed (0.01f, 10.0f, 0.4f), 0.2f, hz, fromHz);
        addB (P::fx_flanger_sync, "Flanger Sync", false);
        addC (P::fx_flanger_division, "Flanger Note", P::noteValues(), 14);
        addFmt (P::fx_flanger_depth, "Flanger Depth", { 0.0f, 1.0f }, 0.7f, pct, fromPct);
        addFmt (P::fx_flanger_manual, "Flanger Manual", skewed (0.1f, 10.0f, 2.0f), 2.0f, ms, fromMs);
        addFmt (P::fx_flanger_feedback, "Flanger Regen", { -0.95f, 0.95f }, 0.5f, pctBi, fromPct);
        addB (P::fx_flanger_tz, "Flanger Through-Zero", false);
        addFmt (P::fx_flanger_spread, "Flanger Spread", { 0.0f, 180.0f }, 90.0f, deg, plain);
        addFmt (P::fx_flanger_mix, "Flanger Mix", { 0.0f, 1.0f }, 0.6f, pct, fromPct);

        addB (P::fx_echo_on, "Tape Echo On", false);
        addC (P::fx_echo_mode, "Tape Echo Mode", P::echoModes(), 3);
        addFmt (P::fx_echo_time, "Tape Echo Repeat Rate", skewed (25.0f, 1000.0f, 177.0f), 177.0f, ms, fromMs);
        addB (P::fx_echo_sync, "Tape Echo Sync", false);
        addC (P::fx_echo_division, "Tape Echo Note", P::noteValues(), 6);
        addFmt (P::fx_echo_intensity, "Tape Echo Intensity", { 0.0f, 1.0f }, 0.45f, pct, fromPct);
        addFmt (P::fx_echo_wow, "Tape Echo Wow", { 0.0f, 1.0f }, 0.25f, pct, fromPct);
        addFmt (P::fx_echo_flutter, "Tape Echo Flutter", { 0.0f, 1.0f }, 0.2f, pct, fromPct);
        addFmt (P::fx_echo_sat, "Tape Echo Saturation", { 0.0f, 1.0f }, 0.35f, pct, fromPct);
        addFmt (P::fx_echo_bass, "Tape Echo Bass", { -12.0f, 12.0f }, 0.0f, dB, plain);
        addFmt (P::fx_echo_treble, "Tape Echo Treble", { -12.0f, 12.0f }, 0.0f, dB, plain);
        addFmt (P::fx_echo_age, "Tape Echo Tape Age", { 0.0f, 1.0f }, 0.25f, pct, fromPct);
        addFmt (P::fx_echo_width, "Tape Echo Width", { 0.0f, 1.0f }, 0.6f, pct, fromPct);
        addFmt (P::fx_echo_spring, "Tape Echo Spring", { 0.0f, 1.0f }, 0.35f, pct, fromPct);
        addFmt (P::fx_echo_mix, "Tape Echo Mix", { 0.0f, 1.0f }, 0.3f, pct, fromPct);

        addB (P::fx_reverb_on, "Reverb On", false);
        addC (P::fx_reverb_type, "Reverb Type", P::reverbTypes(), 0);
        addFmt (P::fx_reverb_size, "Reverb Size", skewed (0.3f, 2.0f, 1.0f), 1.0f, pct, fromPct);
        addFmt (P::fx_reverb_decay, "Reverb Decay", skewed (0.2f, 30.0f, 3.0f), 2.5f, sec, plain);
        addFmt (P::fx_reverb_predelay, "Reverb Pre-Delay", skewed (0.0f, 500.0f, 60.0f), 12.0f, ms, fromMs);
        addFmt (P::fx_reverb_damp, "Reverb Damping", { 0.0f, 1.0f }, 0.45f, pct, fromPct);
        addFmt (P::fx_reverb_lowcut, "Reverb Low Cut", skewed (20.0f, 1000.0f, 150.0f), 90.0f, hz, fromHz);
        addFmt (P::fx_reverb_mod, "Reverb Modulation", { 0.0f, 1.0f }, 0.4f, pct, fromPct);
        addFmt (P::fx_reverb_width, "Reverb Width", { 0.0f, 1.5f }, 1.0f, pct, fromPct);
        addFmt (P::fx_reverb_shimmer, "Reverb Shimmer", { 0.0f, 1.0f }, 0.5f, pct, fromPct);
        addC (P::fx_reverb_pitch, "Reverb Shimmer Pitch", P::shimmerPitches(), 3);
        addB (P::fx_reverb_freeze, "Reverb Freeze", false);
        addFmt (P::fx_reverb_mix, "Reverb Mix", { 0.0f, 1.0f }, 0.25f, pct, fromPct);

        addB (P::fx_comp_on, "Comp On", false);
        addFmt (P::fx_comp_threshold, "Comp Threshold", { -40.0f, 0.0f }, -12.0f, dB, plain);
        addC (P::fx_comp_ratio, "Comp Ratio", P::compRatios(), 3);
        addC (P::fx_comp_attack, "Comp Attack", P::compAttacks(), 3);
        addC (P::fx_comp_release, "Comp Release", P::compReleases(), 4);
        addFmt (P::fx_comp_makeup, "Comp Make-Up", { 0.0f, 20.0f }, 0.0f, dB, plain);
        addFmt (P::fx_comp_schpf, "Comp SC High-Pass", skewed (20.0f, 300.0f, 80.0f), 20.0f, hz, fromHz);
        addFmt (P::fx_comp_mix, "Comp Mix", { 0.0f, 1.0f }, 1.0f, pct, fromPct);
    }

    // Voice mode / trims
    layout.add (std::make_unique<APC> (pid (P::voice_mode), "Voice Mode", juce::StringArray { "POLY", "DUO" }, 0));
    for (int v = 1; v <= P::kNumTrims; ++v)
    {
        layout.add (std::make_unique<APF> (pid (P::trimTune (v)), "Voice " + juce::String (v) + " Tune Trim", Range (-50.0f, 50.0f), 0.0f, cents));
        layout.add (std::make_unique<APF> (pid (P::trimCut (v)), "Voice " + juce::String (v) + " Cutoff Trim", Range (-1.0f, 1.0f), 0.0f,
                                           juce::AudioParameterFloatAttributes().withStringFromValueFunction (
                                               [] (float x, int) { return (x > 0.0f ? "+" : "") + juce::String (x, 2) + " oct"; })));
    }

    // Oscillator octave switches
    layout.add (std::make_unique<APC> (pid (P::osc1_oct), "Osc 1 Octave", juce::StringArray { "-2", "-1", "0", "+1", "+2" }, 2));
    layout.add (std::make_unique<APC> (pid (P::osc2_oct), "Osc 2 Octave", juce::StringArray { "-2", "-1", "0", "+1", "+2" }, 2));

    // Quality
    layout.add (std::make_unique<APC> (pid (P::quality), "Quality", juce::StringArray { "ECO", "GREAT", "DIVINE" }, 1));
    layout.add (std::make_unique<APC> (pid (P::offline_quality), "Offline Quality", juce::StringArray { "SAME", "DIVINE" }, 1));

    // Arpeggiator
    layout.add (toggle (P::arp_on, "Arp On", false));
    layout.add (std::make_unique<APC> (pid (P::arp_mode), "Arp Mode", juce::StringArray { "UP", "DOWN", "UP-DOWN", "RANDOM", "ORDER" }, 0));
    layout.add (std::make_unique<API> (pid (P::arp_oct), "Arp Octaves", 1, 4, 1));
    {
        juce::StringArray rates;
        for (auto* n : augur::arpRateNames)
            rates.add (n);
        layout.add (std::make_unique<APC> (pid (P::arp_rate), "Arp Rate", rates, 5));
    }
    layout.add (std::make_unique<APF> (pid (P::arp_gate), "Arp Gate", Range (0.02f, 1.0f), 0.5f,
                                       juce::AudioParameterFloatAttributes().withStringFromValueFunction (formatPercent)));
    layout.add (std::make_unique<APF> (pid (P::arp_swing), "Arp Swing", Range (0.0f, 0.5f), 0.0f,
                                       juce::AudioParameterFloatAttributes().withStringFromValueFunction (formatPercent)));
    layout.add (toggle (P::arp_latch, "Arp Latch", false));

    return layout;
}

//==============================================================================

struct ParameterBinding::Raw
{
    using A = std::atomic<float>*;
    A osc1Freq, osc1Fine, osc1Pw, osc1Saw, osc1Pulse, osc1Sync;
    A osc2Freq, osc2Fine, osc2Pw, osc2Saw, osc2Tri, osc2Pulse, osc2LoFreq, osc2Kbd, oscModel;
    A mix1, mix2, mixNoise, mixDrive;
    A cutoff, reso, envAmt, fltModel, keytrack, fltVel;
    A level, ampVel, atAmount;
    A fA, fD, fS, fR, aA, aD, aS, aR;
    A lfoRate, lfoAmount, lfoDelay, lfoWave, lfoSync;
    A pmOn, pmFenv, pmOsc2, pmFreqA, pmPwA, pmFilter;
    std::array<A, 8> mmSrc, mmDst, mmAmt;
    A mixRing, mixSub, subOct, xmod, lfo2Rate, lfo2Wave, lfo2Sync, lfo2Retrig, mA, mD, mS, mR;
    A detune, spread, pan, voices, age;
    A dlOn, dlTime, dlFb, dlMix;
    A tune, glide, unison, legato, pbRange, vintage;
    A arpOn, arpMode, arpOct, arpRate, arpGate, arpSwing, arpLatch;
    A fltSlope, fltMode, hpf, voiceMode, osc1Oct, osc2Oct, master;
    std::array<A, 8> trimTune, trimCut;
    A fzOn, fzSus, fzTone, fzVol, fzMix, dlSync, dlDiv, dlPing;
    struct Rack
    {
        A driveOn, driveModel, driveAmount, driveBias, driveTone, driveOutput, driveMix;
        A chorusOn, chorusMode, chorusRate, chorusDepth, chorusTone, chorusHiss, chorusWidth, chorusMix;
        A phaserOn, phaserStages, phaserRate, phaserSync, phaserDiv, phaserDepth, phaserCenter, phaserFb, phaserSpread, phaserLfo, phaserMix;
        A flangerOn, flangerRate, flangerSync, flangerDiv, flangerDepth, flangerManual, flangerFb, flangerTz, flangerSpread, flangerMix;
        A echoOn, echoMode, echoTime, echoSync, echoDiv, echoIntensity, echoWow, echoFlutter, echoSat, echoBass, echoTreble, echoAge, echoWidth,
            echoSpring, echoMix;
        A reverbOn, reverbType, reverbSize, reverbDecay, reverbPredelay, reverbDamp, reverbLowcut, reverbMod, reverbWidth, reverbShimmer,
            reverbPitch, reverbFreeze, reverbMix;
        A compOn, compThreshold, compRatio, compAttack, compRelease, compMakeup, compSchpf, compMix;
    } rack;

    // Rev 3 knob digitiser: 7 bits, two-step software hysteresis (service manual 2-12).
    struct Knob7
    {
        juce::RangedAudioParameter* param = nullptr;
        A raw = nullptr;
        int step = -1;

        float read (bool vintage) noexcept
        {
            const float value = raw->load (std::memory_order_relaxed);
            if (! vintage)
            {
                step = -1;
                return value;
            }
            const int now = juce::roundToInt (param->convertTo0to1 (value) * 127.0f);
            if (step < 0 || std::abs (now - step) >= 2)
                step = now;
            return param->convertFrom0to1 (static_cast<float> (step) / 127.0f);
        }
    };
    std::vector<Knob7> knobs; // allocated once in the constructor
};

ParameterBinding::ParameterBinding (juce::AudioProcessorValueTreeState& s) : raw (std::make_unique<Raw>())
{
    namespace P = params;
    const auto get = [&s] (const juce::String& id) {
        auto* p = s.getRawParameterValue (id);
        jassert (p != nullptr);
        return p;
    };
    auto& r = *raw;
    r.osc1Freq = get (P::osc1_freq); r.osc1Fine = get (P::osc1_fine); r.osc1Pw = get (P::osc1_pw);
    r.osc1Saw = get (P::osc1_saw); r.osc1Pulse = get (P::osc1_pulse); r.osc1Sync = get (P::osc1_sync);
    r.osc2Freq = get (P::osc2_freq); r.osc2Fine = get (P::osc2_fine); r.osc2Pw = get (P::osc2_pw);
    r.osc2Saw = get (P::osc2_saw); r.osc2Tri = get (P::osc2_tri); r.osc2Pulse = get (P::osc2_pulse);
    r.osc2LoFreq = get (P::osc2_lofreq); r.osc2Kbd = get (P::osc2_kbd); r.oscModel = get (P::osc_model);
    r.mix1 = get (P::mix_osc1); r.mix2 = get (P::mix_osc2); r.mixNoise = get (P::mix_noise); r.mixDrive = get (P::mix_drive);
    r.cutoff = get (P::flt_cutoff); r.reso = get (P::flt_reso); r.envAmt = get (P::flt_env_amt);
    r.fltModel = get (P::flt_model); r.keytrack = get (P::flt_keytrack); r.fltVel = get (P::flt_velocity);
    r.level = get (P::amp_level); r.ampVel = get (P::amp_velocity); r.atAmount = get (P::at_amount);
    r.fA = get (P::fenv_a); r.fD = get (P::fenv_d); r.fS = get (P::fenv_s); r.fR = get (P::fenv_r);
    r.aA = get (P::aenv_a); r.aD = get (P::aenv_d); r.aS = get (P::aenv_s); r.aR = get (P::aenv_r);
    r.lfoRate = get (P::lfo_rate); r.lfoAmount = get (P::lfo_amount); r.lfoDelay = get (P::lfo_delay);
    r.lfoWave = get (P::lfo_wave); r.lfoSync = get (P::lfo_sync);
    r.pmOn = get (P::pm_on); r.pmFenv = get (P::pm_fenv_amt); r.pmOsc2 = get (P::pm_osc2_amt);
    r.pmFreqA = get (P::pm_dst_freqa); r.pmPwA = get (P::pm_dst_pwa); r.pmFilter = get (P::pm_dst_filter);
    for (int i = 0; i < P::kNumMatrixSlots; ++i)
    {
        r.mmSrc[static_cast<size_t> (i)] = get (P::mmSrc (i + 1));
        r.mmDst[static_cast<size_t> (i)] = get (P::mmDst (i + 1));
        r.mmAmt[static_cast<size_t> (i)] = get (P::mmAmt (i + 1));
    }
    r.detune = get (P::voice_detune); r.spread = get (P::voice_spread); r.pan = get (P::voice_pan);
    r.voices = get (P::voice_count); r.age = get (P::analog_age);
    r.dlOn = get (P::delay_on); r.dlTime = get (P::delay_time); r.dlFb = get (P::delay_fb); r.dlMix = get (P::delay_mix);
    r.tune = get (P::master_tune); r.glide = get (P::glide); r.unison = get (P::unison); r.legato = get (P::legato);
    r.pbRange = get (P::pb_range);
    r.vintage = get (P::vintage_cv);
    r.mixRing = get (P::mix_ring); r.mixSub = get (P::mix_sub); r.subOct = get (P::sub_oct); r.xmod = get (P::osc_xmod);
    r.lfo2Rate = get (P::lfo2_rate); r.lfo2Wave = get (P::lfo2_wave); r.lfo2Sync = get (P::lfo2_sync); r.lfo2Retrig = get (P::lfo2_retrig);
    r.mA = get (P::menv_a); r.mD = get (P::menv_d); r.mS = get (P::menv_s); r.mR = get (P::menv_r);
    r.fzOn = get (P::fuzz_on); r.fzSus = get (P::fuzz_sustain); r.fzTone = get (P::fuzz_tone); r.fzVol = get (P::fuzz_volume);
    r.fzMix = get (P::fuzz_mix); r.dlSync = get (P::delay_sync); r.dlDiv = get (P::delay_div); r.dlPing = get (P::delay_pingpong);
    {
        auto& x = r.rack;
        x.driveOn = get (P::fx_drive_on); x.driveModel = get (P::fx_drive_model); x.driveAmount = get (P::fx_drive_amount);
        x.driveBias = get (P::fx_drive_bias); x.driveTone = get (P::fx_drive_tone); x.driveOutput = get (P::fx_drive_output); x.driveMix = get (P::fx_drive_mix);
        x.chorusOn = get (P::fx_chorus_on); x.chorusMode = get (P::fx_chorus_mode); x.chorusRate = get (P::fx_chorus_rate);
        x.chorusDepth = get (P::fx_chorus_depth); x.chorusTone = get (P::fx_chorus_tone); x.chorusHiss = get (P::fx_chorus_hiss);
        x.chorusWidth = get (P::fx_chorus_width); x.chorusMix = get (P::fx_chorus_mix);
        x.phaserOn = get (P::fx_phaser_on); x.phaserStages = get (P::fx_phaser_stages); x.phaserRate = get (P::fx_phaser_rate);
        x.phaserSync = get (P::fx_phaser_sync); x.phaserDiv = get (P::fx_phaser_division); x.phaserDepth = get (P::fx_phaser_depth);
        x.phaserCenter = get (P::fx_phaser_center); x.phaserFb = get (P::fx_phaser_feedback); x.phaserSpread = get (P::fx_phaser_spread);
        x.phaserLfo = get (P::fx_phaser_lfo); x.phaserMix = get (P::fx_phaser_mix);
        x.flangerOn = get (P::fx_flanger_on); x.flangerRate = get (P::fx_flanger_rate); x.flangerSync = get (P::fx_flanger_sync);
        x.flangerDiv = get (P::fx_flanger_division); x.flangerDepth = get (P::fx_flanger_depth); x.flangerManual = get (P::fx_flanger_manual);
        x.flangerFb = get (P::fx_flanger_feedback); x.flangerTz = get (P::fx_flanger_tz); x.flangerSpread = get (P::fx_flanger_spread);
        x.flangerMix = get (P::fx_flanger_mix);
        x.echoOn = get (P::fx_echo_on); x.echoMode = get (P::fx_echo_mode); x.echoTime = get (P::fx_echo_time); x.echoSync = get (P::fx_echo_sync);
        x.echoDiv = get (P::fx_echo_division); x.echoIntensity = get (P::fx_echo_intensity); x.echoWow = get (P::fx_echo_wow);
        x.echoFlutter = get (P::fx_echo_flutter); x.echoSat = get (P::fx_echo_sat); x.echoBass = get (P::fx_echo_bass);
        x.echoTreble = get (P::fx_echo_treble); x.echoAge = get (P::fx_echo_age); x.echoWidth = get (P::fx_echo_width);
        x.echoSpring = get (P::fx_echo_spring); x.echoMix = get (P::fx_echo_mix);
        x.reverbOn = get (P::fx_reverb_on); x.reverbType = get (P::fx_reverb_type); x.reverbSize = get (P::fx_reverb_size);
        x.reverbDecay = get (P::fx_reverb_decay); x.reverbPredelay = get (P::fx_reverb_predelay); x.reverbDamp = get (P::fx_reverb_damp);
        x.reverbLowcut = get (P::fx_reverb_lowcut); x.reverbMod = get (P::fx_reverb_mod); x.reverbWidth = get (P::fx_reverb_width);
        x.reverbShimmer = get (P::fx_reverb_shimmer); x.reverbPitch = get (P::fx_reverb_pitch); x.reverbFreeze = get (P::fx_reverb_freeze);
        x.reverbMix = get (P::fx_reverb_mix);
        x.compOn = get (P::fx_comp_on); x.compThreshold = get (P::fx_comp_threshold); x.compRatio = get (P::fx_comp_ratio);
        x.compAttack = get (P::fx_comp_attack); x.compRelease = get (P::fx_comp_release); x.compMakeup = get (P::fx_comp_makeup);
        x.compSchpf = get (P::fx_comp_schpf); x.compMix = get (P::fx_comp_mix);
    }
    r.voiceMode = get (P::voice_mode);
    r.master = get (P::master_volume);
    r.osc1Oct = get (P::osc1_oct);
    r.osc2Oct = get (P::osc2_oct);
    for (int v = 0; v < P::kNumTrims; ++v)
    {
        r.trimTune[static_cast<size_t> (v)] = get (P::trimTune (v + 1));
        r.trimCut[static_cast<size_t> (v)] = get (P::trimCut (v + 1));
    }
    r.fltSlope = get (P::flt_slope); r.fltMode = get (P::flt_mode); r.hpf = get (P::hpf_cutoff);
    r.arpOn = get (P::arp_on); r.arpMode = get (P::arp_mode); r.arpOct = get (P::arp_oct); r.arpRate = get (P::arp_rate);
    r.arpGate = get (P::arp_gate); r.arpSwing = get (P::arp_swing); r.arpLatch = get (P::arp_latch);

    // The Prophet's own panel knobs (the pots listed in the service manual's program format).
    for (const char* id : { P::flt_cutoff, P::flt_reso, P::flt_env_amt, P::mix_osc1, P::mix_osc2, P::mix_noise,
                            P::osc1_pw, P::osc2_pw, P::osc2_fine, P::fenv_a, P::fenv_d, P::fenv_s, P::fenv_r,
                            P::aenv_a, P::aenv_d, P::aenv_s, P::aenv_r, P::lfo_rate, P::glide, P::pm_fenv_amt, P::pm_osc2_amt })
        r.knobs.push_back ({ s.getParameter (id), get (id), -1 });
}

ParameterBinding::~ParameterBinding() = default;

void ParameterBinding::fill (augur::SynthParams& p) noexcept
{
    auto& r = *raw;
    const auto f = [] (std::atomic<float>* a) { return a->load (std::memory_order_relaxed); };
    const auto b = [&f] (std::atomic<float>* a) { return f (a) > 0.5f; };
    const auto i = [&f] (std::atomic<float>* a) { return juce::roundToInt (f (a)); };

    p.osc1Semi = i (r.osc1Freq); p.osc1Fine = f (r.osc1Fine); p.osc1Pw = f (r.osc1Pw) * 0.01f;
    p.osc1Saw = b (r.osc1Saw); p.osc1Pulse = b (r.osc1Pulse); p.osc1Sync = b (r.osc1Sync);
    p.osc2Semi = i (r.osc2Freq); p.osc2Fine = f (r.osc2Fine); p.osc2Pw = f (r.osc2Pw) * 0.01f;
    p.osc2Saw = b (r.osc2Saw); p.osc2Tri = b (r.osc2Tri); p.osc2Pulse = b (r.osc2Pulse);
    p.osc2LoFreq = b (r.osc2LoFreq); p.osc2Kbd = b (r.osc2Kbd); p.oscModel = i (r.oscModel);
    p.mixOsc1 = f (r.mix1); p.mixOsc2 = f (r.mix2); p.mixNoise = f (r.mixNoise); p.mixDrive = f (r.mixDrive);
    p.cutoffHz = f (r.cutoff); p.resonance = f (r.reso); p.envAmount = f (r.envAmt);
    p.filterModel = i (r.fltModel); p.keytrack = i (r.keytrack); p.filterVelocity = f (r.fltVel);
    p.levelDb = f (r.level); p.ampVelocity = f (r.ampVel); p.aftertouchAmount = f (r.atAmount);
    p.fenvA = f (r.fA); p.fenvD = f (r.fD); p.fenvS = f (r.fS); p.fenvR = f (r.fR);
    p.aenvA = f (r.aA); p.aenvD = f (r.aD); p.aenvS = f (r.aS); p.aenvR = f (r.aR);
    p.lfoRate = f (r.lfoRate); p.lfoAmount = f (r.lfoAmount); p.lfoDelay = f (r.lfoDelay);
    p.lfoWave = i (r.lfoWave); p.lfoSync = b (r.lfoSync);
    p.pmOn = b (r.pmOn); p.pmFilterEnv = f (r.pmFenv); p.pmOsc2 = f (r.pmOsc2);
    p.pmFreqA = b (r.pmFreqA); p.pmPwA = b (r.pmPwA); p.pmFilter = b (r.pmFilter);
    for (size_t k = 0; k < r.mmSrc.size(); ++k)
    {
        p.matrix[k].source = static_cast<augur::ModSource> (juce::jlimit (0, static_cast<int> (augur::ModSource::count) - 1, i (r.mmSrc[k])));
        p.matrix[k].dest = static_cast<augur::ModDest> (juce::jlimit (0, static_cast<int> (augur::ModDest::count) - 1, i (r.mmDst[k])));
        p.matrix[k].amount = f (r.mmAmt[k]);
    }
    p.voiceDetune = f (r.detune); p.voiceSpread = f (r.spread); p.voicePan = f (r.pan);
    p.voiceCount = i (r.voices); p.analogAge = f (r.age);
    p.masterTuneCents = f (r.tune); p.glide = f (r.glide); p.unison = b (r.unison); p.legato = b (r.legato);
    p.pitchBendRange = i (r.pbRange);
    p.mixRing = f (r.mixRing); p.mixSub = f (r.mixSub); p.subOctave = i (r.subOct); p.crossMod = f (r.xmod);
    p.lfo2Rate = f (r.lfo2Rate); p.lfo2Wave = i (r.lfo2Wave); p.lfo2Sync = b (r.lfo2Sync); p.lfo2Retrig = b (r.lfo2Retrig);
    p.menvA = f (r.mA); p.menvD = f (r.mD); p.menvS = f (r.mS); p.menvR = f (r.mR);
    p.fuzzOn = b (r.fzOn); p.fuzzSustain = f (r.fzSus); p.fuzzTone = f (r.fzTone); p.fuzzVolume = f (r.fzVol); p.fuzzMix = f (r.fzMix);
    {
        // Effects rack (the processing order is plugin state, set by the processor).
        namespace R = augur::rack;
        auto& fx = p.fx;
        const auto& x = r.rack;
        const auto on = [&fx] (R::FxId id, bool v) { fx.on[static_cast<size_t> (id)] = v; };
        on (R::FxId::drive, b (x.driveOn));
        on (R::FxId::chorus, b (x.chorusOn));
        on (R::FxId::phaser, b (x.phaserOn));
        on (R::FxId::flanger, b (x.flangerOn));
        on (R::FxId::delay, b (r.dlOn));
        on (R::FxId::echo, b (x.echoOn));
        on (R::FxId::reverb, b (x.reverbOn));
        on (R::FxId::comp, b (x.compOn));

        fx.drive.model = static_cast<R::DriveModel> (juce::jlimit (0, 4, i (x.driveModel)));
        fx.drive.driveDb = f (x.driveAmount);
        fx.drive.bias = f (x.driveBias);
        fx.drive.tone = f (x.driveTone);
        fx.drive.outputDb = f (x.driveOutput);
        fx.drive.mix = f (x.driveMix);

        fx.chorus.mode = static_cast<R::ChorusMode> (juce::jlimit (0, 4, i (x.chorusMode)));
        fx.chorus.rate = f (x.chorusRate);
        fx.chorus.depth = f (x.chorusDepth);
        fx.chorus.tone = f (x.chorusTone);
        fx.chorus.hiss = f (x.chorusHiss);
        fx.chorus.width = f (x.chorusWidth);
        fx.chorus.mix = f (x.chorusMix);

        static constexpr int stageCounts[] { 4, 6, 8, 12 };
        fx.phaser.stages = stageCounts[juce::jlimit (0, 3, i (x.phaserStages))];
        fx.phaser.rateHz = f (x.phaserRate);
        fx.phaserSync = b (x.phaserSync);
        fx.phaserDivision = i (x.phaserDiv);
        fx.phaser.depth = f (x.phaserDepth);
        fx.phaser.centreHz = f (x.phaserCenter);
        fx.phaser.feedback = f (x.phaserFb);
        fx.phaser.spreadDeg = f (x.phaserSpread);
        fx.phaser.lfo = static_cast<R::PhaserLfo> (juce::jlimit (0, 2, i (x.phaserLfo)));
        fx.phaser.mix = f (x.phaserMix);

        fx.flanger.rateHz = f (x.flangerRate);
        fx.flangerSync = b (x.flangerSync);
        fx.flangerDivision = i (x.flangerDiv);
        fx.flanger.depth = f (x.flangerDepth);
        fx.flanger.manualMs = f (x.flangerManual);
        fx.flanger.feedback = f (x.flangerFb);
        fx.flanger.throughZero = b (x.flangerTz);
        fx.flanger.spreadDeg = f (x.flangerSpread);
        fx.flanger.mix = f (x.flangerMix);

        fx.delay.timeSeconds = f (r.dlTime);
        fx.delay.feedback = f (r.dlFb);
        fx.delay.mix = f (r.dlMix);
        fx.delay.sync = b (r.dlSync);
        fx.delay.division = i (r.dlDiv);
        fx.delay.pingPong = b (r.dlPing);

        // The classic unit's MODE selector: 1-4 echo only, 5-11 echo + spring, 12 spring only.
        // Head sets of the rack's echo: 0 H1, 1 H2, 2 H3, 3 H1+H2, 4 H2+H3, 5 H1+H3, 6 all.
        static constexpr int selectorHeads[12] { 0, 1, 2, 4, 0, 1, 2, 3, 4, 5, 6, 6 };
        const int selector = juce::jlimit (0, 11, i (x.echoMode));
        fx.echo.mode = selectorHeads[selector];
        fx.echoHeadsOff = selector == 11;
        fx.echoSpring = selector >= 4 ? f (x.echoSpring) : 0.0;
        fx.echo.headMs = f (x.echoTime);
        fx.echoSync = b (x.echoSync);
        fx.echoDivision = i (x.echoDiv);
        fx.echo.intensity = f (x.echoIntensity);
        fx.echo.wow = f (x.echoWow);
        fx.echo.flutter = f (x.echoFlutter);
        fx.echo.saturation = f (x.echoSat);
        fx.echo.bassDb = f (x.echoBass);
        fx.echo.trebleDb = f (x.echoTreble);
        fx.echo.age = f (x.echoAge);
        fx.echo.width = f (x.echoWidth);
        fx.echo.mix = f (x.echoMix);

        const int reverbType = juce::jlimit (0, 4, i (x.reverbType));
        fx.reverbSpring = reverbType == 4;
        fx.reverb.type = static_cast<R::ReverbType> (juce::jmin (3, reverbType));
        fx.reverb.size = f (x.reverbSize);
        fx.reverb.decayS = f (x.reverbDecay);
        fx.reverb.predelayMs = f (x.reverbPredelay);
        fx.reverb.damping = f (x.reverbDamp);
        fx.reverb.lowCutHz = f (x.reverbLowcut);
        fx.reverb.modulation = f (x.reverbMod);
        fx.reverb.width = f (x.reverbWidth);
        fx.reverb.shimmer = f (x.reverbShimmer);
        static constexpr double pitches[] { -12.0, 5.0, 7.0, 12.0, 19.0, 24.0 };
        fx.reverb.pitchSemis = pitches[juce::jlimit (0, 5, i (x.reverbPitch))];
        fx.reverb.freeze = b (x.reverbFreeze);
        fx.reverb.mix = f (x.reverbMix);

        fx.comp.thresholdDb = f (x.compThreshold);
        fx.comp.ratioIndex = i (x.compRatio);
        fx.comp.attackIndex = i (x.compAttack);
        fx.comp.releaseIndex = i (x.compRelease);
        fx.comp.makeupDb = f (x.compMakeup);
        fx.comp.scHpfHz = f (x.compSchpf);
        fx.comp.mix = f (x.compMix);
    }
    p.voiceMode = i (r.voiceMode);
    p.masterVolumeDb = f (r.master);
    p.osc1Octave = i (r.osc1Oct) - 2;
    p.osc2Octave = i (r.osc2Oct) - 2;
    for (size_t v = 0; v < 8; ++v)
    {
        p.trimTune[v] = f (r.trimTune[v]);
        p.trimCutoff[v] = f (r.trimCut[v]);
    }
    p.filterSlope = i (r.fltSlope); p.filterMode = i (r.fltMode); p.hpfHz = f (r.hpf);
    p.arpOn = b (r.arpOn); p.arpMode = i (r.arpMode); p.arpOctaves = i (r.arpOct); p.arpRate = i (r.arpRate);
    p.arpGate = f (r.arpGate); p.arpSwing = f (r.arpSwing); p.arpLatch = b (r.arpLatch);

    // Panel knobs, optionally through the 7-bit digitiser (order matches the constructor list).
    const bool vintage = b (r.vintage);
    float k[21];
    for (size_t n = 0; n < r.knobs.size() && n < 21; ++n)
        k[n] = r.knobs[n].read (vintage);
    p.cutoffHz = k[0]; p.resonance = k[1]; p.envAmount = k[2];
    p.mixOsc1 = k[3]; p.mixOsc2 = k[4]; p.mixNoise = k[5];
    p.osc1Pw = k[6] * 0.01f; p.osc2Pw = k[7] * 0.01f; p.osc2Fine = k[8];
    p.fenvA = k[9]; p.fenvD = k[10]; p.fenvS = k[11]; p.fenvR = k[12];
    p.aenvA = k[13]; p.aenvD = k[14]; p.aenvS = k[15]; p.aenvR = k[16];
    p.lfoRate = k[17]; p.glide = k[18]; p.pmFilterEnv = k[19]; p.pmOsc2 = k[20];
}

} // namespace augur5
