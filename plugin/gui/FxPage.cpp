#include "FxPage.h"

#include "../Parameters.h"
#include "Engine/SynthParams.h"
#include "Rack/FxCommon.h"

#include <complex>

namespace augur5::ui
{

namespace
{
namespace P = augur5::params;

// Effect ids: 0..7 = the rack (FxId order), 8 = FUZZ.
const char* const fxTitles[FxPage::numEffects] { "DRIVE", "CHORUS", "PHASER", "FLANGER", "DELAY", "TAPE ECHO", "REVERB", "BUS COMP", "FUZZ" };
const char* const fxOnIds[FxPage::numEffects] { P::fx_drive_on, P::fx_chorus_on, P::fx_phaser_on, P::fx_flanger_on, P::delay_on,
                                                P::fx_echo_on, P::fx_reverb_on, P::fx_comp_on, P::fuzz_on };

juce::Colour fxAccent (int id)
{
    switch (id)
    {
        case 0: return colours::amber;
        case 1: return colours::slate;
        case 2: return colours::plum;
        case 3: return colours::plum;
        case 4: return colours::sage;
        case 5: return colours::amber;
        case 6: return colours::slate;
        case 7: return colours::ink;
        default: return colours::accent;
    }
}

struct KnobDef { const char* id; const char* label; };
struct ChoiceDef { const char* id; const char* label; juce::StringArray (*labels)(); };
struct ToggleDef { const char* id; const char* label; };
struct ComboDef { const char* id; const char* label; };
struct Spec
{
    const char* subtitle;
    std::vector<ChoiceDef> choices;
    std::vector<KnobDef> knobs;
    std::vector<ToggleDef> toggles;
    std::vector<ComboDef> combos;
};

juce::StringArray echoNumbers() { return { "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12" }; }

const Spec& specFor (int id)
{
    static const std::vector<Spec> specs {
        { "12AX7 triode stage (Koren equations) \xc2\xb7 Tube Screamer clipper \xc2\xb7 tape hysteresis (Jiles-Atherton) \xc2\xb7 wavefolder \xc2\xb7 bit-crusher. "
          "4x oversampled and level-compensated: DRIVE changes the colour, not the loudness.",
          { { P::fx_drive_model, "MODEL", P::driveModels } },
          { { P::fx_drive_amount, "DRIVE" }, { P::fx_drive_bias, "BIAS" }, { P::fx_drive_tone, "TONE" }, { P::fx_drive_output, "OUTPUT" }, { P::fx_drive_mix, "MIX" } },
          {}, {} },
        { "Bucket-brigade choruses: Juno-60 I / II / I+II (measured rates and delay spans, inverted right channel), "
          "Dimension D (trapezoid LFO, inverted cross-feed) and the string-machine ensemble (three BBDs, two 3-phase LFOs).",
          { { P::fx_chorus_mode, "MODE", P::chorusModes } },
          { { P::fx_chorus_rate, "RATE" }, { P::fx_chorus_depth, "DEPTH" }, { P::fx_chorus_tone, "TONE" }, { P::fx_chorus_hiss, "BBD HISS" },
            { P::fx_chorus_width, "WIDTH" }, { P::fx_chorus_mix, "MIX" } },
          {}, {} },
        { "All-pass cascade swept exponentially like the JFETs / OTAs of the Phase 90, Small Stone (COLOR regeneration) and Bi-Phase. "
          "LFO or the input's envelope; stereo spread between the two cascades.",
          { { P::fx_phaser_stages, "STAGES", P::phaserStages }, { P::fx_phaser_lfo, "SWEEP", P::phaserLfos } },
          { { P::fx_phaser_rate, "RATE" }, { P::fx_phaser_depth, "DEPTH" }, { P::fx_phaser_center, "MANUAL" }, { P::fx_phaser_feedback, "COLOR" },
            { P::fx_phaser_spread, "SPREAD" }, { P::fx_phaser_mix, "MIX" } },
          { { P::fx_phaser_sync, "SYNC" } }, { { P::fx_phaser_division, "NOTE" } } },
        { "BBD flanger with positive / negative regeneration, or two-machine tape flanging: the dry path waits at MANUAL and the "
          "swept path crosses it, so the comb collapses through zero.",
          {},
          { { P::fx_flanger_rate, "RATE" }, { P::fx_flanger_depth, "DEPTH" }, { P::fx_flanger_manual, "MANUAL" }, { P::fx_flanger_feedback, "REGEN" },
            { P::fx_flanger_spread, "SPREAD" }, { P::fx_flanger_mix, "MIX" } },
          { { P::fx_flanger_sync, "SYNC" }, { P::fx_flanger_tz, "THROUGH-ZERO" } }, { { P::fx_flanger_division, "NOTE" } } },
        { "AUGUR's stereo tape delay: a clean digital line with a darkening, slightly saturated feedback path, free time or "
          "host-synced divisions, optional ping-pong between the channels.",
          {},
          { { P::delay_time, "TIME" }, { P::delay_fb, "FEEDBACK" }, { P::delay_mix, "MIX" } },
          { { P::delay_sync, "SYNC" }, { P::delay_pingpong, "PING-PONG" } }, { { P::delay_div, "NOTE" } } },
        { "The classic space echo's 12-position MODE selector: three playback heads at the RE-201 spacing (1 : 1.90 : 2.75), "
          "one motor for all (it glides when REPEAT RATE moves), wow & flutter, tape saturation in the loop and the spring tank.",
          { { P::fx_echo_mode, "MODE", echoNumbers } },
          { { P::fx_echo_time, "REPEAT RATE" }, { P::fx_echo_intensity, "INTENSITY" }, { P::fx_echo_sat, "SATURATION" }, { P::fx_echo_wow, "WOW" },
            { P::fx_echo_flutter, "FLUTTER" }, { P::fx_echo_bass, "BASS" }, { P::fx_echo_treble, "TREBLE" }, { P::fx_echo_age, "TAPE AGE" },
            { P::fx_echo_width, "WIDTH" }, { P::fx_echo_spring, "SPRING" }, { P::fx_echo_mix, "MIX" } },
          { { P::fx_echo_sync, "SYNC" } }, { { P::fx_echo_division, "NOTE" } } },
        { "Dattorro plate (AES 1997 tank) \xc2\xb7 room and hall: 8-line feedback delay network with Householder mixing, two-band RT60 "
          "and modulated lines \xc2\xb7 shimmer: a pitch shifter inside the loop \xc2\xb7 spring: a three-spring tank with its chirped dispersion.",
          { { P::fx_reverb_type, "TYPE", P::reverbTypes } },
          { { P::fx_reverb_size, "SIZE" }, { P::fx_reverb_decay, "DECAY" }, { P::fx_reverb_predelay, "PRE-DELAY" }, { P::fx_reverb_damp, "DAMPING" },
            { P::fx_reverb_lowcut, "LOW CUT" }, { P::fx_reverb_mod, "MOD" }, { P::fx_reverb_width, "WIDTH" }, { P::fx_reverb_shimmer, "SHIMMER" },
            { P::fx_reverb_mix, "MIX" } },
          { { P::fx_reverb_freeze, "FREEZE" } }, { { P::fx_reverb_pitch, "SHIMMER PITCH" } } },
        { "Bus compressor of the G-console lineage: VCA with feedback detection (the glue), stepped attack / release, program-dependent "
          "AUTO release, side-chain high-pass and parallel mix.",
          { { P::fx_comp_ratio, "RATIO", P::compRatios }, { P::fx_comp_attack, "ATTACK", P::compAttacks }, { P::fx_comp_release, "RELEASE", P::compReleases } },
          { { P::fx_comp_threshold, "THRESHOLD" }, { P::fx_comp_makeup, "MAKE-UP" }, { P::fx_comp_schpf, "SC HPF" }, { P::fx_comp_mix, "MIX" } },
          {}, {} },
        { "The melancholic wall-of-sound pedal on the voice bus, before the rack: input stage, two cascaded diode clipping stages, the "
          "passive tone stack that scoops the mids around 1 kHz and a recovery stage. 8x oversampled with ADAA clippers.",
          {},
          { { P::fuzz_sustain, "SUSTAIN" }, { P::fuzz_tone, "TONE" }, { P::fuzz_volume, "VOLUME" }, { P::fuzz_mix, "MIX" } },
          {}, {} },
    };
    return specs[static_cast<size_t> (id)];
}

float rawValue (APVTS& st, const char* id)
{
    auto* v = st.getRawParameterValue (id);
    return v != nullptr ? v->load() : 0.0f;
}

// RE-201 MODE selector -> playback heads (bit per head) and spring.
constexpr int echoHeadMask[12] { 1, 2, 4, 6, 1, 2, 4, 3, 6, 5, 7, 0 };

void strokeCurve (juce::Graphics& g, const juce::Path& p, juce::Colour c, float width = 1.6f)
{
    g.setColour (c.withAlpha (0.16f));
    g.strokePath (p, juce::PathStrokeType (width + 3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (c);
    g.strokePath (p, juce::PathStrokeType (width, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}
} // namespace

//==============================================================================
// Live view of one effect.
class FxViz final : public juce::Component
{
public:
    FxViz (APVTS& s, FxModel& m, int fx) : st (s), model (m), id (fx) { setInterceptsMouseClicks (false, false); }

    void tick()
    {
        t += 1.0 / 30.0;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        const auto r = getLocalBounds().toFloat();
        g.setColour (colours::subPanel);
        g.fillRoundedRectangle (r, 7.0f);
        g.setColour (colours::subPanelBorder);
        g.drawRoundedRectangle (r.reduced (0.5f), 7.0f, 1.0f);
        const auto plot = r.reduced (18.0f, 22.0f).withTrimmedTop (8.0f);
        g.setColour (colours::hairline);
        for (int i = 1; i < 4; ++i)
        {
            g.fillRect (plot.getX(), plot.getY() + plot.getHeight() * static_cast<float> (i) / 4.0f, plot.getWidth(), 1.0f);
            g.fillRect (plot.getX() + plot.getWidth() * static_cast<float> (i) / 4.0f, plot.getY(), 1.0f, plot.getHeight());
        }
        switch (id)
        {
            case 0: drive (g, plot); break;
            case 1: chorus (g, plot); break;
            case 2: phaser (g, plot); break;
            case 3: flanger (g, plot); break;
            case 4: delay (g, plot); break;
            case 5: echo (g, plot); break;
            case 6: reverb (g, plot); break;
            case 7: comp (g, plot); break;
            default: fuzz (g, plot); break;
        }
    }

private:
    juce::Colour accent() const { return fxAccent (id); }

    void caption (juce::Graphics& g, juce::Rectangle<float> plot, const juce::String& text)
    {
        drawTracked (g, text, plot.withHeight (14.0f).translated (0.0f, -20.0f), Fonts::jost (9.0f, true, 0.16f), colours::captionLight,
                     juce::Justification::centredLeft);
    }

    void curve (juce::Graphics& g, juce::Rectangle<float> plot, const std::function<float (float)>& f, juce::Colour c, float lo = -1.0f,
                float hi = 1.0f, bool fillUnder = false)
    {
        juce::Path p;
        for (int i = 0; i <= 240; ++i)
        {
            const float x = static_cast<float> (i) / 240.0f;
            const float y = juce::jlimit (lo, hi, f (x));
            const juce::Point<float> pt (plot.getX() + x * plot.getWidth(), plot.getBottom() - (y - lo) / (hi - lo) * plot.getHeight());
            if (i == 0)
                p.startNewSubPath (pt);
            else
                p.lineTo (pt);
        }
        if (fillUnder)
        {
            juce::Path area (p);
            area.lineTo (plot.getRight(), plot.getBottom());
            area.lineTo (plot.getX(), plot.getBottom());
            area.closeSubPath();
            g.setGradientFill (juce::ColourGradient (c.withAlpha (0.12f), 0.0f, plot.getY(), c.withAlpha (0.0f), 0.0f, plot.getBottom(), false));
            g.fillPath (area);
        }
        strokeCurve (g, p, c);
    }

    // Log frequency axis labels under a 20 Hz - 20 kHz plot.
    void freqAxis (juce::Graphics& g, juce::Rectangle<float> plot)
    {
        for (const auto& [f, text] : { std::pair { 100.0f, "100" }, std::pair { 1000.0f, "1k" }, std::pair { 10000.0f, "10k" } })
        {
            const float x = plot.getX() + plot.getWidth() * std::log10 (f / 20.0f) / 3.0f;
            drawTracked (g, text, { x - 20.0f, plot.getBottom() + 3.0f, 40.0f, 12.0f }, Fonts::mono (7.5f), colours::caption, juce::Justification::centred);
        }
    }

    void drive (juce::Graphics& g, juce::Rectangle<float> plot)
    {
        caption (g, plot, juce::String::fromUTF8 ("TRANSFER CURVE  (IN \xe2\x86\x92 OUT)"));
        const int driveModel = juce::roundToInt (rawValue (st, P::fx_drive_model));
        const float amountDb = rawValue (st, P::fx_drive_amount);
        const float gain = juce::Decibels::decibelsToGain (amountDb);
        const float bias = rawValue (st, P::fx_drive_bias);
        g.setColour (colours::hairline.darker (0.08f));
        g.drawLine (plot.getX(), plot.getBottom(), plot.getRight(), plot.getY(), 1.0f);
        curve (g, plot, [&] (float x01) {
            const float x = (x01 * 2.0f - 1.0f) * gain;
            switch (driveModel)
            {
                case 0: return std::tanh (x + 0.105f * x * x * (0.5f - 0.5f * bias));
                case 1: return (x > 0.0f ? 1.0f : 0.8f + 0.2f * (1.0f - bias)) * std::asinh (2.5f * x) / std::asinh (2.5f * gain);
                case 2: return std::tanh (0.8f * x) * (1.0f - 0.25f * (0.5f - 0.5f * bias) * std::exp (-8.0f * x * x));
                case 3: return std::sin (0.5f * juce::MathConstants<float>::pi * (x + 0.5f * bias));
                default:
                {
                    const float levels = std::exp2 (16.0f - 14.0f * juce::jlimit (0.0f, 1.0f, amountDb / 48.0f) - 1.0f);
                    return std::round ((x01 * 2.0f - 1.0f) * levels) / levels;
                }
            }
        }, accent());
    }

    void fuzz (juce::Graphics& g, juce::Rectangle<float> plot)
    {
        // Top: the two cascaded diode stages. Bottom: the tone stack's response (mid scoop).
        const auto top = plot.withHeight (plot.getHeight() * 0.56f);
        const auto bottom = plot.withTrimmedTop (plot.getHeight() * 0.68f);
        caption (g, top, juce::String::fromUTF8 ("TWO CLIPPING STAGES  (IN \xe2\x86\x92 OUT)"));
        const float sustain = rawValue (st, P::fuzz_sustain), tone = rawValue (st, P::fuzz_tone);
        const float gain1 = 2.0f * std::exp2 (sustain * 6.6f);
        const auto clip = [] (float x) { return x / std::sqrt (1.0f + x * x); };
        const float norm = clip (12.0f * clip (gain1));
        // Input span +-0.1 (a quiet note): the knee is visible and SUSTAIN visibly squares it off.
        curve (g, top, [&] (float x01) { return clip (12.0f * clip (gain1 * (x01 * 2.0f - 1.0f) * 0.1f)) / norm; }, accent());
        drawTracked (g, "TONE STACK", bottom.withHeight (12.0f).translated (0.0f, -15.0f), Fonts::jost (9.0f, true, 0.16f), colours::captionLight,
                     juce::Justification::centredLeft);
        curve (g, bottom, [&] (float x) {
            const float f = 20.0f * std::pow (1000.0f, x);
            const std::complex<float> s (0.0f, f);
            const auto lo = 1.0f / (1.0f + s / 410.0f);
            const auto hi = (s / 1850.0f) / (1.0f + s / 1850.0f);
            return juce::Decibels::gainToDecibels (std::abs (lo * (1.0f - tone) + hi * tone * 1.6f), -60.0f);
        }, colours::ink.withAlpha (0.7f), -30.0f, 6.0f, true);
        freqAxis (g, bottom);
    }

    void chorus (juce::Graphics& g, juce::Rectangle<float> plot)
    {
        caption (g, plot, "BBD DELAY LINES  (LIVE MODULATION)");
        const int mode = juce::roundToInt (rawValue (st, P::fx_chorus_mode));
        const float rate = rawValue (st, P::fx_chorus_rate), depth = rawValue (st, P::fx_chorus_depth);
        const float hz[] { 0.513f, 0.863f, 9.75f, 0.25f, 0.6f };
        const int lines = mode == 4 ? 3 : 2;
        for (int l = 0; l < lines; ++l)
        {
            const float y = plot.getY() + plot.getHeight() * static_cast<float> (l + 1) / static_cast<float> (lines + 1);
            g.setColour (colours::subPanelBorder);
            g.fillRect (plot.getX(), y, plot.getWidth(), 1.0f);
            const float ph = static_cast<float> (t * hz[juce::jlimit (0, 4, mode)] * rate) + (lines == 3 ? static_cast<float> (l) / 3.0f : (l == 1 ? 0.5f : 0.0f));
            const float m = mode == 3 ? juce::jlimit (-1.0f, 1.0f, 1.6f * (2.0f * std::abs (2.0f * (ph - std::floor (ph)) - 1.0f) - 1.0f))
                                      : std::sin (juce::MathConstants<float>::twoPi * ph);
            const float x = plot.getCentreX() + m * depth * 0.19f * plot.getWidth();
            g.setColour (accent().withAlpha (0.14f));
            g.fillEllipse (x - 12.0f, y - 12.0f, 24.0f, 24.0f);
            g.setColour (accent());
            g.fillEllipse (x - 4.5f, y - 4.5f, 9.0f, 9.0f);
            drawTracked (g, lines == 3 ? "BBD " + juce::String (l + 1) : (l == 0 ? juce::String ("LEFT") : juce::String ("RIGHT")),
                         { plot.getX(), y - 18.0f, 80.0f, 12.0f }, Fonts::jost (8.5f, true, 0.12f), colours::caption, juce::Justification::centredLeft);
        }
    }

    // |H(f)| of the phaser (dry + all-pass cascade) at the current sweep.
    void phaser (juce::Graphics& g, juce::Rectangle<float> plot)
    {
        caption (g, plot, "RESPONSE  20 Hz - 20 kHz  (LIVE SWEEP " + juce::String (juce::roundToInt (model.phaserHz())) + " Hz)");
        const int stages = P::phaserStages()[juce::roundToInt (rawValue (st, P::fx_phaser_stages))].getIntValue();
        const float fc = juce::jmax (20.0f, model.phaserHz()), mix = rawValue (st, P::fx_phaser_mix), fb = rawValue (st, P::fx_phaser_feedback);
        curve (g, plot, [&] (float x) {
            const float f = 20.0f * std::pow (1000.0f, x);
            const float phi = -2.0f * static_cast<float> (stages) * std::atan (f / fc);
            const std::complex<float> ap = std::polar (1.0f, phi);
            const std::complex<float> shifted = ap / (1.0f - fb * ap);
            return juce::Decibels::gainToDecibels (std::abs ((1.0f - mix) + mix * shifted), -60.0f);
        }, accent(), -36.0f, 12.0f, true);
        freqAxis (g, plot);
    }

    void flanger (juce::Graphics& g, juce::Rectangle<float> plot)
    {
        caption (g, plot, "COMB FILTER  (LIVE DELAY " + juce::String (model.flangerMs(), 2) + " ms)");
        const float d = model.flangerMs() * 0.001f, mix = rawValue (st, P::fx_flanger_mix), fb = rawValue (st, P::fx_flanger_feedback);
        const bool tz = rawValue (st, P::fx_flanger_tz) > 0.5f;
        curve (g, plot, [&] (float x) {
            const float f = 20.0f * std::pow (1000.0f, x);
            const std::complex<float> z = std::polar (1.0f, -juce::MathConstants<float>::twoPi * f * d);
            const std::complex<float> wet = z / (1.0f - fb * z);
            return juce::Decibels::gainToDecibels (std::abs ((1.0f - 0.5f * mix) + 0.5f * mix * (tz ? -wet : wet)), -60.0f);
        }, accent(), -36.0f, 12.0f, true);
        freqAxis (g, plot);
    }

    void delay (juce::Graphics& g, juce::Rectangle<float> plot)
    {
        const bool sync = rawValue (st, P::delay_sync) > 0.5f;
        const bool ping = rawValue (st, P::delay_pingpong) > 0.5f;
        const double beats = augur::delaySyncBeats[static_cast<size_t> (juce::jlimit (0, 11, juce::roundToInt (rawValue (st, P::delay_div))))];
        const float seconds = sync ? static_cast<float> (beats * 60.0 / juce::jmax (20.0, model.tempo())) : rawValue (st, P::delay_time);
        const float fb = rawValue (st, P::delay_fb), mix = rawValue (st, P::delay_mix);
        caption (g, plot, "REPEATS  (" + juce::String (juce::roundToInt (seconds * 1000.0f)) + " ms" + (ping ? ", PING-PONG)" : ")"));
        const float window = juce::jmax (0.5f, seconds * 8.5f);
        const float mid = plot.getCentreY();
        g.setColour (colours::subPanelBorder);
        g.fillRect (plot.getX(), mid, plot.getWidth(), 1.0f);
        drawTracked (g, "L", { plot.getX() - 14.0f, plot.getY(), 12.0f, 12.0f }, Fonts::mono (8.0f), colours::caption, juce::Justification::centred);
        drawTracked (g, "R", { plot.getX() - 14.0f, plot.getBottom() - 12.0f, 12.0f, 12.0f }, Fonts::mono (8.0f), colours::caption, juce::Justification::centred);
        // Dry hit, then the repeats.
        const auto bar = [&] (float time, float amp, bool right, juce::Colour c) {
            const float x = plot.getX() + plot.getWidth() * time / window;
            const float hgt = plot.getHeight() * 0.46f * amp;
            g.setColour (c);
            g.fillRoundedRectangle (x - 2.0f, right ? mid + 1.0f : mid - hgt, 4.0f, hgt, 2.0f);
            if (! ping)
                g.fillRoundedRectangle (x - 2.0f, mid + 1.0f, 4.0f, hgt, 2.0f);
        };
        bar (0.0f, 1.0f - 0.5f * mix, false, colours::ink.withAlpha (0.55f));
        float amp = mix;
        for (int n = 1; n < 24; ++n)
        {
            const float time = seconds * static_cast<float> (n);
            if (time > window || amp < 0.01f)
                break;
            bar (time, amp, ping && n % 2 == 0, accent());
            amp *= fb;
        }
    }

    void echo (juce::Graphics& g, juce::Rectangle<float> plot)
    {
        const int mode = juce::jlimit (0, 11, juce::roundToInt (rawValue (st, P::fx_echo_mode)));
        const float head = juce::jmax (1.0f, model.echoHeadMs());
        const bool spring = mode >= 4 && rawValue (st, P::fx_echo_spring) > 0.001f;
        caption (g, plot, juce::String::fromUTF8 ("TAPE LOOP  \xc2\xb7  MODE ") + P::echoModes()[mode].fromFirstOccurrenceOf (" ", false, false).trim());

        const auto loop = juce::Rectangle<float> (plot.getX() + 20.0f, plot.getY() + 34.0f, plot.getWidth() - 40.0f, plot.getHeight() * 0.42f);
        g.setColour (colours::amber.withAlpha (0.35f));
        g.drawRoundedRectangle (loop, loop.getHeight() * 0.5f, 2.5f);
        const float spin = static_cast<float> (std::fmod (t * 177.0 / juce::jmax (25.0f, head), 1.0));
        for (int k = 0; k < 14; ++k)
        {
            const float u = std::fmod (spin + static_cast<float> (k) / 14.0f, 1.0f);
            g.setColour (colours::caption.withAlpha (0.6f));
            g.fillEllipse (loop.getX() + loop.getHeight() * 0.5f + u * (loop.getWidth() - loop.getHeight()) - 1.5f, loop.getBottom() - 1.5f, 3.0f, 3.0f);
        }
        const float ratios[] { 1.0f, 1.90f, 2.75f };
        const float x0 = loop.getX() + loop.getWidth() * 0.14f, span = loop.getWidth() * 0.26f;
        const auto headBlock = [&] (float x, const juce::String& text, bool on, const juce::String& under) {
            g.setColour (on ? accent() : colours::ledOff);
            g.fillRoundedRectangle (x - 5.0f, loop.getY() - 8.0f, 10.0f, 16.0f, 2.0f);
            drawTracked (g, text, { x - 24.0f, loop.getY() - 26.0f, 48.0f, 12.0f }, Fonts::jost (8.5f, true, 0.1f), on ? colours::text : colours::caption,
                         juce::Justification::centred);
            if (under.isNotEmpty())
                drawTracked (g, under, { x - 34.0f, loop.getY() + 12.0f, 68.0f, 12.0f }, Fonts::mono (8.0f), on ? colours::label : colours::caption,
                             juce::Justification::centred);
        };
        headBlock (x0, "REC", true, {});
        for (int h = 0; h < 3; ++h)
            headBlock (x0 + span * ratios[h], "H" + juce::String (h + 1), (echoHeadMask[mode] >> h & 1) != 0,
                       juce::String (juce::roundToInt (head * ratios[h])) + " ms");

        // Spring tank under the loop.
        const auto tank = juce::Rectangle<float> (plot.getX() + 40.0f, loop.getBottom() + 34.0f, plot.getWidth() - 80.0f, 34.0f);
        g.setColour (spring ? colours::slate.withAlpha (0.08f) : colours::subPanel);
        g.fillRoundedRectangle (tank, 5.0f);
        g.setColour (spring ? colours::slate.withAlpha (0.5f) : colours::subPanelBorder);
        g.drawRoundedRectangle (tank, 5.0f, 1.0f);
        juce::Path coil;
        const float wobble = spring ? 1.5f * std::sin (static_cast<float> (t) * 9.0f) : 0.0f;
        for (int i = 0; i <= 160; ++i)
        {
            const float u = static_cast<float> (i) / 160.0f;
            const float x = tank.getX() + 18.0f + u * (tank.getWidth() - 36.0f);
            const float y = tank.getCentreY() + (7.0f + wobble * std::sin (u * 9.0f)) * std::sin (u * juce::MathConstants<float>::twoPi * 22.0f);
            if (i == 0)
                coil.startNewSubPath (x, y);
            else
                coil.lineTo (x, y);
        }
        g.setColour (spring ? colours::slate : colours::caption.withAlpha (0.5f));
        g.strokePath (coil, juce::PathStrokeType (1.0f));
        drawTracked (g, spring ? "SPRING  " + juce::String (juce::roundToInt (rawValue (st, P::fx_echo_spring) * 100.0f)) + " %" : juce::String ("SPRING OFF"),
                     { tank.getX(), tank.getBottom() + 4.0f, tank.getWidth(), 12.0f }, Fonts::jost (8.5f, true, 0.14f), colours::captionLight,
                     juce::Justification::centred);
    }

    void reverb (juce::Graphics& g, juce::Rectangle<float> plot)
    {
        const float decay = rawValue (st, P::fx_reverb_decay), damp = rawValue (st, P::fx_reverb_damp);
        const bool freeze = rawValue (st, P::fx_reverb_freeze) > 0.5f;
        const int type = juce::roundToInt (rawValue (st, P::fx_reverb_type));
        const float window = juce::jmax (2.0f, decay * 1.5f);
        caption (g, plot, freeze ? juce::String ("FROZEN") : P::reverbTypes()[type] + juce::String::fromUTF8 ("  \xc2\xb7  DECAY  LOWS / HIGHS  0 - ") + juce::String (window, 1) + " s");
        const float alpha = 1.0f - 0.9f * damp;
        curve (g, plot, [&] (float x) { return freeze ? 0.0f : juce::jmax (-60.0f, -60.0f * x * window / decay); }, accent(), -60.0f, 0.0f, true);
        curve (g, plot, [&] (float x) { return freeze ? 0.0f : juce::jmax (-60.0f, -60.0f * x * window / (decay * alpha)); }, colours::caption, -60.0f, 0.0f);
    }

    void comp (juce::Graphics& g, juce::Rectangle<float> plot)
    {
        const float gr = model.compGainReduction();
        smoothedGr += 0.3f * (gr - smoothedGr);
        caption (g, plot, "GAIN REDUCTION  " + juce::String (smoothedGr, 1) + " dB");
        const auto c = juce::Point<float> (plot.getCentreX(), plot.getBottom() - 6.0f);
        const float radius = juce::jmin (plot.getWidth() * 0.45f, plot.getHeight() * 0.85f);
        juce::Path arc;
        arc.addCentredArc (c.x, c.y, radius, radius, 0.0f, -1.1f, 1.1f, true);
        g.setColour (colours::subPanelBorder);
        g.strokePath (arc, juce::PathStrokeType (2.0f));
        for (int db = 0; db <= 20; db += 4)
        {
            const float a = -1.1f + 2.2f * (1.0f - static_cast<float> (db) / 20.0f);
            const auto p1 = c.getPointOnCircumference (radius, a), p2 = c.getPointOnCircumference (radius - 10.0f, a);
            g.setColour (colours::captionLight);
            g.drawLine ({ p1, p2 }, 1.2f);
            drawTracked (g, juce::String (db), juce::Rectangle<float> (30.0f, 12.0f).withCentre (c.getPointOnCircumference (radius + 13.0f, a)),
                         Fonts::mono (8.0f), colours::captionLight, juce::Justification::centred);
        }
        const float a = -1.1f + 2.2f * (1.0f - juce::jlimit (0.0f, 20.0f, smoothedGr) / 20.0f);
        juce::Path needle;
        needle.startNewSubPath (c);
        needle.lineTo (c.getPointOnCircumference (radius - 4.0f, a));
        strokeCurve (g, needle, colours::accent, 1.6f);
        g.setColour (colours::ink);
        g.fillEllipse (c.x - 4.0f, c.y - 4.0f, 8.0f, 8.0f);
    }

    APVTS& st;
    FxModel& model;
    int id;
    double t = 0.0;
    float smoothedGr = 0.0f;
};

//==============================================================================
// Large editor of one effect.
class FxEditor final : public juce::Component
{
public:
    FxEditor (APVTS& st, FxModel& m, int fx) : state (st), id (fx), spec (specFor (fx)), viz (st, m, fx)
    {
        const auto accent = fxAccent (fx);
        onToggle = std::make_unique<ParamToggle> (st, fxOnIds[fx], "ON", accent == colours::ink ? colours::accent : accent);
        addAndMakeVisible (*onToggle);
        for (const auto& c : spec.choices)
        {
            std::vector<std::unique_ptr<juce::Button>> row;
            for (const auto& l : c.labels())
            {
                auto b = std::make_unique<SegmentButton> (l, accent == colours::ink ? colours::accent : accent);
                b->idText = c.id;
                row.push_back (std::move (b));
            }
            auto* group = groups.add (new ChoiceGroup (st, c.id, std::move (row)));
            addAndMakeVisible (group);
            choiceValues.push_back (st.getRawParameterValue (c.id));
        }
        const int knobSize = spec.knobs.size() <= 6 ? 58 : 48;
        for (const auto& k : spec.knobs)
        {
            auto* knob = knobs.add (new Knob (st, k.id, k.label, knobSize, accent == colours::ink ? colours::accent : accent));
            knob->setSize (Knob::boundsFor (knobSize).getWidth(), Knob::boundsFor (knobSize).getHeight());
            addAndMakeVisible (knob);
        }
        for (const auto& t : spec.toggles)
            addAndMakeVisible (toggles.add (new ParamToggle (st, t.id, t.label, accent == colours::ink ? colours::accent : accent)));
        for (const auto& c : spec.combos)
            addAndMakeVisible (combos.add (new ParamChoiceBox (st, c.id)));
        addAndMakeVisible (viz);
    }

    void poll()
    {
        viz.tick();
        // Captions that depend on a choice (the echo MODE read-out).
        float sum = 0.0f;
        for (auto* v : choiceValues)
            sum += v != nullptr ? v->load() * 7.0f : 0.0f;
        if (sum != lastChoices)
        {
            lastChoices = sum;
            repaint (0, 0, viz.getX(), getHeight());
        }
    }

    void paint (juce::Graphics& g) override
    {
        const auto accent = fxAccent (id);
        drawTracked (g, fxTitles[id], { 16.0f, 4.0f, 320.0f, 36.0f }, Fonts::light (25.0f, 0.3f), colours::title.interpolatedWith (accent, 0.25f),
                     juce::Justification::centredLeft);
        g.setColour (accent);
        g.fillRoundedRectangle (16.0f, 42.0f, 22.0f, 2.0f, 1.0f);
        g.setFont (Fonts::jost (10.5f, false, 0.02f));
        g.setColour (colours::captionLight);
        g.drawFittedText (juce::String::fromUTF8 (spec.subtitle), 16, 52, viz.getX() - 50, 34, juce::Justification::topLeft, 2, 0.9f);
        for (size_t i = 0; i < spec.choices.size(); ++i)
            drawTracked (g, spec.choices[i].label, { 16.0f, static_cast<float> (choiceY (static_cast<int> (i))), 96.0f, 28.0f },
                         Fonts::jost (9.5f, true, 0.2f), colours::label, juce::Justification::centredLeft);
        for (int i = 0; i < combos.size(); ++i)
            drawTracked (g, spec.combos[static_cast<size_t> (i)].label,
                         { static_cast<float> (combos[i]->getX()), static_cast<float> (combos[i]->getY() - 16), 160.0f, 12.0f },
                         Fonts::jost (8.5f, true, 0.2f), colours::caption, juce::Justification::centredLeft);
        if (id == 5 && ! choiceValues.empty() && choiceValues[0] != nullptr)
        {
            // The selector position spelled out: which heads, and whether the spring tank is in.
            const int mode = juce::jlimit (0, 11, juce::roundToInt (choiceValues[0]->load()));
            drawTracked (g, juce::String::fromUTF8 ("POSITION ") + P::echoModes()[mode].trim().replace ("  ", juce::String::fromUTF8 ("  \xc2\xb7  ")),
                         { 116.0f, static_cast<float> (choiceY (0) + 32), 600.0f, 14.0f }, Fonts::jost (9.0f, true, 0.14f), accent.darker (0.2f),
                         juce::Justification::centredLeft);
        }
    }

    void resized() override
    {
        viz.setBounds (getWidth() - 470, 8, 462, getHeight() - 16);
        onToggle->setBounds (viz.getX() - 96, 12, 72, 26);
        const int left = 16, right = viz.getX() - 30;
        for (int i = 0; i < groups.size(); ++i)
        {
            auto* group = groups[i];
            const int n = group->getNumButtons();
            const int gap = n > 8 ? 4 : 6;
            const int w = juce::jmin (n > 8 ? 42 : 104, (right - left - 100 - gap * (n - 1)) / n);
            group->setBounds (left + 100, choiceY (i), n * w + (n - 1) * gap, 28);
            for (int b = 0; b < n; ++b)
                group->getButton (b).setBounds (b * (w + gap), 0, w, 28);
        }
        // Knobs: up to six per row.
        const int perRow = knobs.size() > 6 ? (knobs.size() + 1) / 2 : knobs.size();
        const int knobTop = choiceY (static_cast<int> (groups.size())) + (id == 5 ? 26 : 6);
        const int cellW = juce::jmin (124, (right - left) / juce::jmax (1, perRow));
        for (int i = 0; i < knobs.size(); ++i)
        {
            auto* k = knobs[i];
            const int col = i % perRow, rowIdx = i / perRow;
            const auto b = k->getBounds();
            k->setBounds (left + col * cellW + (cellW - b.getWidth()) / 2, knobTop + rowIdx * (b.getHeight() + 12), b.getWidth(), b.getHeight());
        }
        const int knobH = knobs.isEmpty() ? 80 : knobs[0]->getHeight();
        const int rowsOfKnobs = (knobs.size() + perRow - 1) / juce::jmax (1, perRow);
        const int y = knobTop + rowsOfKnobs * (knobH + 12) + 20;
        int x = left;
        for (auto* t : toggles)
        {
            t->setBounds (x, y, 132, 28);
            x += 144;
        }
        for (auto* c : combos)
        {
            c->setBounds (x, y, 132, 28);
            x += 144;
        }
    }

private:
    static int choiceY (int i) { return 96 + i * 36; }

    APVTS& state;
    int id;
    const Spec& spec;
    FxViz viz;
    std::unique_ptr<ParamToggle> onToggle;
    juce::OwnedArray<ChoiceGroup> groups;
    std::vector<std::atomic<float>*> choiceValues;
    float lastChoices = -1.0f;
    juce::OwnedArray<Knob> knobs;
    juce::OwnedArray<ParamToggle> toggles;
    juce::OwnedArray<ParamChoiceBox> combos;
};

//==============================================================================
// One slot of the chain strip: position, name, ON switch; click to edit, drag to move (the rack units).
class FxSlot final : public juce::Component
{
public:
    FxSlot (FxPage& p, APVTS& st, int fx)
        : page (p), id (fx), onToggle (st, fxOnIds[fx], "ON", fxAccent (fx) == colours::ink ? colours::accent : fxAccent (fx)),
          on (st.getRawParameterValue (fxOnIds[fx]))
    {
        addAndMakeVisible (onToggle);
    }

    void setSelected (bool s)
    {
        selected = s;
        repaint();
    }
    void setPosition (int p)
    {
        position = p;
        repaint();
    }
    void poll()
    {
        const bool active = on != nullptr && on->load() > 0.5f;
        if (active != shownActive)
        {
            shownActive = active;
            repaint();
        }
    }

    void paint (juce::Graphics& g) override
    {
        const auto r = getLocalBounds().toFloat().reduced (0.5f);
        const auto accent = fxAccent (id);
        g.setColour (juce::Colours::white);
        g.fillRoundedRectangle (r, 7.0f);
        if (selected)
        {
            g.setColour (accent.withAlpha (0.08f));
            g.fillRoundedRectangle (r, 7.0f);
        }
        g.setColour (selected ? accent.withAlpha (0.85f) : (shownActive ? accent.withAlpha (0.45f) : colours::fieldBorder));
        g.drawRoundedRectangle (r, 7.0f, selected ? 1.5f : 1.0f);
        if (selected)
        {
            g.setColour (accent);
            g.fillRoundedRectangle (r.getX() + 12.0f, r.getBottom() - 3.0f, r.getWidth() - 24.0f, 2.0f, 1.0f);
        }
        drawTracked (g, id == FxPage::fuzzId ? juce::String ("PRE") : juce::String (position + 1), { 10.0f, 7.0f, 30.0f, 12.0f },
                     Fonts::mono (8.0f), colours::caption, juce::Justification::centredLeft);
        drawTracked (g, fxTitles[id], { 10.0f, 18.0f, r.getWidth() - 20.0f, 16.0f }, Fonts::jost (11.0f, true, 0.18f),
                     shownActive ? colours::title : colours::captionLight, juce::Justification::centredLeft);
        if (id != FxPage::fuzzId)
        {
            // Grip: this unit can be dragged.
            g.setColour (colours::tickMinor);
            for (int i = 0; i < 3; ++i)
                g.fillRect (r.getRight() - 18.0f, 9.0f + 3.0f * static_cast<float> (i), 8.0f, 1.0f);
        }
    }

    void resized() override { onToggle.setBounds (8, getHeight() - 25, 58, 20); }

    void mouseDown (const juce::MouseEvent&) override { page.select (id); }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (id != FxPage::fuzzId && e.getDistanceFromDragStart() > 6)
            page.dragSlot (id, e.getEventRelativeTo (&page).x);
    }
    void mouseUp (const juce::MouseEvent&) override { page.dropSlot(); }

private:
    FxPage& page;
    int id;
    ParamToggle onToggle;
    std::atomic<float>* on;
    bool selected = false, shownActive = false;
    int position = 0;
};

//==============================================================================
FxPage::FxPage (APVTS& st, FxModel& m) : state (st), model (m)
{
    for (int i = 0; i < numEffects; ++i)
    {
        addAndMakeVisible (slots.add (new FxSlot (*this, st, i)));
        addChildComponent (editors.add (new FxEditor (st, m, i)));
    }
    order = model.getOrder();
    select (fuzzId);
}

FxPage::~FxPage() = default;

void FxPage::select (int fx)
{
    selected = juce::jlimit (0, numEffects - 1, fx);
    for (int i = 0; i < numEffects; ++i)
    {
        slots[i]->setSelected (i == selected);
        editors[i]->setVisible (i == selected);
    }
}

juce::Rectangle<int> FxPage::slotBounds (int position) const
{
    // Position 0 = FUZZ (fixed), 1..8 = the rack.
    constexpr int x0 = 68, gap = 14, w = 142;
    return { x0 + position * (w + gap), 54, w, 58 };
}

void FxPage::dragSlot (int fx, int x)
{
    dragging = fx;
    const int target = juce::jlimit (0, 7, (x - 68) / (142 + 14) - 1);
    std::vector<int> v (order.begin(), order.end());
    const auto it = std::find (v.begin(), v.end(), fx);
    if (it == v.end() || static_cast<int> (std::distance (v.begin(), it)) == target)
        return;
    v.erase (it);
    v.insert (v.begin() + target, fx);
    std::copy (v.begin(), v.end(), order.begin());
    layoutSlots();
}

void FxPage::dropSlot()
{
    if (dragging >= 0)
        model.setOrder (order);
    dragging = -1;
}

void FxPage::layoutSlots()
{
    slots[fuzzId]->setBounds (slotBounds (0));
    for (int p = 0; p < 8; ++p)
    {
        auto* s = slots[order[static_cast<size_t> (p)]];
        s->setBounds (slotBounds (p + 1));
        s->setPosition (p);
    }
    repaint();
}

void FxPage::poll()
{
    if (dragging < 0)
    {
        const auto current = model.getOrder();
        if (current != order)
        {
            order = current;
            layoutSlots();
        }
    }
    for (auto* s : slots)
        s->poll();
    editors[selected]->poll();
}

void FxPage::resized()
{
    layoutSlots();
    for (auto* e : editors)
        e->setBounds (68, 146, 1400, 482);
}

void FxPage::paint (juce::Graphics& g)
{
    drawSection (g, { 52.0f, 10.0f, 1432.0f, 112.0f }, "EFFECTS CHAIN", colours::accent);
    drawTracked (g, juce::String::fromUTF8 ("voice bus  \xe2\x86\x92  FUZZ  \xe2\x86\x92  rack (drag a unit to reorder)  \xc2\xb7  click a unit to edit it"),
                 { 260.0f, 24.0f, 900.0f, 16.0f }, Fonts::jost (10.0f, false, 0.06f), colours::caption, juce::Justification::centredLeft);
    // Chevrons between the units.
    g.setColour (colours::caption);
    for (int p = 0; p < 8; ++p)
    {
        const auto r = slotBounds (p);
        const float x = static_cast<float> (r.getRight()) + 7.0f;
        juce::Path arrow;
        arrow.startNewSubPath (x - 2.5f, 79.0f);
        arrow.lineTo (x + 2.0f, 83.0f);
        arrow.lineTo (x - 2.5f, 87.0f);
        g.strokePath (arrow, juce::PathStrokeType (1.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    drawSection (g, { 52.0f, 134.0f, 1432.0f, 506.0f }, {}, colours::accent);
}

} // namespace augur5::ui
