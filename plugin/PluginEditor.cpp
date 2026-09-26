#include "PluginEditor.h"

#include "Parameters.h"
#include "PluginProcessor.h"
#include "gui/Theme.h"
#include "gui/Widgets.h"

namespace augur5::ui
{

namespace
{
namespace P = augur5::params;

void drawPanel (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& title)
{
    g.setColour (juce::Colours::black.withAlpha (0.4f));
    g.fillRoundedRectangle (r.translated (0.0f, 2.0f).expanded (1.0f), 9.0f);
    g.setGradientFill (juce::ColourGradient (colours::panelTop, 0.0f, r.getY(), colours::panelBottom, 0.0f, r.getBottom(), false));
    g.fillRoundedRectangle (r, 8.0f);
    g.setColour (juce::Colours::white.withAlpha (0.04f));
    g.drawLine (r.getX() + 6.0f, r.getY() + 1.5f, r.getRight() - 6.0f, r.getY() + 1.5f, 1.0f);
    g.setColour (colours::panelBorder);
    g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);
    drawTracked (g, title, { r.getX() + 16.0f, r.getY() + 16.0f, r.getWidth() - 32.0f, 16.0f }, Fonts::jost (13.0f, true, 0.22f),
                 colours::title, juce::Justification::centredLeft);
}

void drawSubPanel (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& title, float titlePx = 12.0f)
{
    g.setColour (colours::subPanel);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (colours::subPanelBorder);
    g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
    if (title.isNotEmpty())
        drawTracked (g, title, { r.getX() + 12.0f, r.getY() + (titlePx < 12.0f ? 10.0f : 12.0f), r.getWidth() - 24.0f, titlePx < 12.0f ? 14.0f : 26.0f },
                     Fonts::jost (titlePx, true, 0.16f), colours::subTitle, juce::Justification::centredLeft);
}

void drawCaption (juce::Graphics& g, const juce::String& text, float x, float y, float w = 200.0f)
{
    drawTracked (g, text, { x, y, w, 11.0f }, Fonts::jost (9.0f, false, 0.2f), colours::caption, juce::Justification::centredLeft);
}

void drawLogo (juce::Graphics& g, float x, float y, float scale, juce::Colour c)
{
    static const juce::Path logo = svgPath ("M1 10 H8 L11 3 L15 17 L19 6 L22 13 L24 10 H33");
    g.setColour (c);
    g.strokePath (logo, juce::PathStrokeType (1.4f, juce::PathStrokeType::mitered, juce::PathStrokeType::rounded),
                  juce::AffineTransform::scale (scale).translated (x, y));
}

void drawWalnut (juce::Graphics& g, juce::Rectangle<float> r, bool mirrored)
{
    // repeating-linear-gradient(90deg, #3e2212, #5a331b, #3a200f, #6a3d21) plus a lengthwise grain.
    static const juce::Colour stops[] { juce::Colour (0xff3e2212), juce::Colour (0xff5a331b), juce::Colour (0xff3a200f), juce::Colour (0xff6a3d21) };
    for (int x = 0; x < static_cast<int> (r.getWidth()); ++x)
    {
        const int phase = (mirrored ? x + 5 : x) % 11;
        const float t = static_cast<float> (phase) / 11.0f * 4.0f;
        const int i0 = static_cast<int> (t) % 4;
        const auto col = stops[i0].interpolatedWith (stops[(i0 + 1) % 4], t - std::floor (t));
        g.setColour (col);
        g.fillRect (r.getX() + static_cast<float> (x), r.getY(), 1.0f, r.getHeight());
    }
    juce::Random grain (mirrored ? 17 : 5);
    for (int i = 0; i < 90; ++i)
    {
        const float x = r.getX() + grain.nextFloat() * r.getWidth();
        const float y = r.getY() + grain.nextFloat() * r.getHeight();
        g.setColour (juce::Colours::black.withAlpha (0.08f + 0.1f * grain.nextFloat()));
        g.fillRect (x, y, 0.8f, 30.0f + 120.0f * grain.nextFloat());
    }
    juce::ColourGradient shade (juce::Colours::black.withAlpha (0.55f), r.getX(), 0.0f, juce::Colours::black.withAlpha (0.5f), r.getRight(), 0.0f, false);
    shade.addColour (0.5, juce::Colour (0x1affc896));
    g.setGradientFill (shade);
    g.fillRect (r);
}
} // namespace

//==============================================================================
class Canvas final : public juce::Component
{
public:
    Canvas (APVTS& s, std::function<float (int)> voiceLevel) : state (s)
    {
        // Oscillators
        wave (P::osc1_saw, WaveIcon::Saw, 240, 174);
        wave (P::osc1_pulse, WaveIcon::Pulse, 280, 174);
        knob (P::osc1_freq, "FREQUENCY", 64, 80, 227);
        knob (P::osc1_fine, "FINE", 36, 181, 255);
        knob (P::osc1_pw, "WIDTH", 36, 254, 255);
        toggle (P::osc1_sync, "SYNC", 80, 352, 72);
        dropdown (P::osc1_oct, 240, 352, 72);

        wave (P::osc2_saw, WaveIcon::Saw, 470, 174);
        wave (P::osc2_tri, WaveIcon::Triangle, 510, 174);
        wave (P::osc2_pulse, WaveIcon::Pulse, 550, 174);
        knob (P::osc2_freq, "FREQUENCY", 64, 350, 227);
        knob (P::osc2_fine, "FINE", 36, 451, 255);
        knob (P::osc2_pw, "WIDTH", 36, 524, 255);
        toggle (P::osc2_lofreq, "LO FREQ", 350, 352, 84);
        toggle (P::osc2_kbd, "KBD", 442, 352, 64);
        dropdown (P::osc2_oct, 514, 352, 72);

        // Mixer
        knob (P::mix_osc1, "OSC 1", 36, 648, 199);
        knob (P::mix_osc2, "OSC 2", 36, 731, 199);
        knob (P::mix_noise, "NOISE", 36, 648, 285);
        knob (P::mix_drive, "DRIVE", 36, 731, 285);

        // Filter
        knob (P::flt_cutoff, "CUTOFF", 64, 842, 162);
        knob (P::flt_reso, "RESONANCE", 44, 942, 182);
        knob (P::flt_env_amt, "ENV AMT", 44, 1022, 182);
        dropdown (P::flt_model, 842, 309, 120);
        choice (P::flt_slope, { "24 dB", "12 dB" }, { 56, 56 }, 970, 309);
        choice (P::flt_keytrack, { "OFF", "HALF", "FULL" }, { 44, 48, 48 }, 842, 364);
        dropdown (P::flt_mode, 1010, 364, 80);

        // Amplifier
        knob (P::amp_velocity, "VEL \xe2\x80\xba AMP", 44, 1134, 162);
        knob (P::flt_velocity, "VEL \xe2\x80\xba FILTER", 44, 1234, 162);
        knob (P::at_amount, "AFTERTOUCH", 44, 1334, 162);
        voices = add (std::make_unique<VoiceActivity> (state, std::move (voiceLevel)), { 1134, 326, 268, 66 });
        add (std::make_unique<LevelFader> (state), { 1418, 162, 50, 230 });

        // Mod matrix
        for (int slot = 1; slot <= P::kNumMatrixSlots; ++slot)
            matrixRows[static_cast<size_t> (slot - 1)] = add (std::make_unique<MatrixRow> (state, slot),
                                                               { 68, juce::roundToInt (487.0f + 62.33f * static_cast<float> ((slot - 1) % 4)), 438, 30 });
        for (int page = 0; page < 2; ++page)
        {
            auto* b = add (std::make_unique<LedToggle> (page == 0 ? "1-4" : "5-8"), { 390 + 66 * page, 428, 60, 26 });
            b->setClickingTogglesState (false);
            b->onClick = [this, page] { showMatrixPage (page); };
            matrixPages[static_cast<size_t> (page)] = b;
        }
        showMatrixPage (0);

        // LFO
        {
            std::vector<std::unique_ptr<juce::Button>> b;
            for (auto icon : { WaveIcon::Triangle, WaveIcon::Saw, WaveIcon::Square, WaveIcon::SampleHold })
                b.push_back (std::make_unique<WaveButton> (icon));
            auto* group = add (std::make_unique<ChoiceGroup> (state, P::lfo_wave, std::move (b)), { 550, 462, 154, 26 });
            for (int i = 0; i < 4; ++i)
                group->getButton (i).setBounds (i * 40, 0, 34, 26);
        }
        knob (P::lfo_rate, "RATE", 44, 595, 498);
        knob (P::lfo_delay, "DELAY", 32, 550, 586);
        knob (P::lfo_amount, "AMOUNT", 32, 652, 586);
        toggle (P::lfo_sync, "SYNC", 550, 676, 156);

        // Envelopes
        envelopes[0] = add (std::make_unique<EnvelopeDisplay> (state, P::fenv_a, P::fenv_d, P::fenv_s, P::fenv_r), { 764, 518, 242, 80 });
        envelopes[1] = add (std::make_unique<EnvelopeDisplay> (state, P::aenv_a, P::aenv_d, P::aenv_s, P::aenv_r), { 1042, 518, 242, 80 });
        const char* envIds[2][4] = { { P::fenv_a, P::fenv_d, P::fenv_s, P::fenv_r }, { P::aenv_a, P::aenv_d, P::aenv_s, P::aenv_r } };
        const char* envLabels[4] = { "ATTACK", "DECAY", "SUSTAIN", "RELEASE" };
        for (int e = 0; e < 2; ++e)
            for (int k = 0; k < 4; ++k)
                knob (envIds[e][k], envLabels[k], 32, (e == 0 ? 764 : 1042) + 62 * k, 626);

        // Poly mod
        toggle (P::pm_on, "ON", 1340, 462, 126);
        knob (P::pm_fenv_amt, "FILT ENV", 32, 1340, 500);
        knob (P::pm_osc2_amt, "OSC 2", 32, 1412, 500);
        toggle (P::pm_dst_freqa, "FREQ A", 1340, 599, 126);
        toggle (P::pm_dst_pwa, "PW A", 1340, 633, 126);
        toggle (P::pm_dst_filter, "FILTER", 1340, 667, 126);

        // Per voice / vintage
        knob (P::voice_detune, "DETUNE", 44, 68, 796);
        knob (P::voice_spread, "SPREAD", 44, 145, 796);
        knob (P::voice_pan, "PAN", 44, 221, 796);
        add (std::make_unique<VoiceCounter> (state), { 298, 808, 68, 55 });
        knob (P::analog_age, "ANALOG AGE", 56, 419, 782);

        // Effects
        const char* fxOn[3] = { P::chorus_on, P::delay_on, P::reverb_on };
        const char* fxIds[3][3] = { { P::chorus_rate, P::chorus_depth, P::chorus_mix },
                                    { P::delay_time, P::delay_fb, P::delay_mix },
                                    { P::reverb_size, P::reverb_decay, P::reverb_mix } };
        const char* fxLabels[3][3] = { { "RATE", "DEPTH", "MIX" }, { "TIME", "FEEDBACK", "MIX" }, { "SIZE", "DECAY", "MIX" } };
        for (int f = 0; f < 3; ++f)
        {
            const float bx = fxBoxX (f);
            add (std::make_unique<FxLed> (state, fxOn[f]), { juce::roundToInt (bx + 211.33f - 12.0f - 22.0f), 786, 22, 14 });
            for (int k = 0; k < 3; ++k)
                knob (fxIds[f][k], fxLabels[f][k], 32, juce::roundToInt (bx + 12.0f + 65.67f * static_cast<float> (k)), 808);
        }

        // Global
        knob (P::master_tune, "TUNE", 40, 1254, 798);
        knob (P::glide, "GLIDE", 40, 1321, 798);
        toggle (P::unison, "UNISON", 1388, 803, 80);
        toggle (P::legato, "LEGATO", 1388, 839, 80);

        // ---- Expansion modules: laid out in their own panel coordinates, placed in the right-hand block ----
        moveExpansion = true;
        toggle (P::arp_on, "ARP", 68, 966, 70);
        toggle (P::arp_latch, "LATCH", 146, 966, 80);
        dropdown (P::arp_mode, 68, 1024, 110);
        dropdown (P::arp_rate, 186, 1024, 80);
        dropdown (P::arp_oct, 274, 1024, 60);
        knob (P::arp_gate, "GATE", 32, 368, 956);
        knob (P::arp_swing, "SWING", 32, 368, 1036);

        dropdown (P::lfo2_wave, 480, 980, 110);
        toggle (P::lfo2_sync, "SYNC", 480, 1022, 110);
        toggle (P::lfo2_retrig, "RETRIG", 480, 1058, 110);
        knob (P::lfo2_rate, "RATE", 44, 616, 972);

        envelopes[2] = add (std::make_unique<EnvelopeDisplay> (state, P::menv_a, P::menv_d, P::menv_s, P::menv_r), { 742, 960, 268, 54 });
        {
            const char* ids[4] = { P::menv_a, P::menv_d, P::menv_s, P::menv_r };
            const char* labels[4] = { "ATTACK", "DECAY", "SUSTAIN", "RELEASE" };
            for (int k = 0; k < 4; ++k)
                knob (ids[k], labels[k], 32, 746 + 66 * k, 1030);
        }

        knob (P::mix_sub, "SUB", 36, 1054, 962);
        knob (P::mix_ring, "RING", 36, 1128, 962);
        knob (P::osc_xmod, "FM B\xe2\x80\xba" "A", 36, 1202, 962);
        choice (P::sub_oct, { "-1 OCT", "-2 OCT" }, { 70, 70 }, 1054, 1062);

        knob (P::hpf_cutoff, "HPF", 44, 1298, 962);
        dropdown (P::voice_mode, 1376, 980, 92);
        dropdown (P::quality, 1376, 1062, 92);

        // ---- Row 5: fuzz, phaser, effect options, voice trims ----
        {
            const char* ids[4] = { P::fuzz_sustain, P::fuzz_tone, P::fuzz_volume, P::fuzz_mix };
            const char* labels[4] = { "SUSTAIN", "TONE", "VOLUME", "MIX" };
            for (int k = 0; k < 4; ++k)
                knob (ids[k], labels[k], 36, 72 + 76 * k, 1196);
        }
        {
            const char* ids[4] = { P::phaser_rate, P::phaser_depth, P::phaser_fb, P::phaser_mix };
            const char* labels[4] = { "RATE", "DEPTH", "FEEDBACK", "MIX" };
            for (int k = 0; k < 4; ++k)
                knob (ids[k], labels[k], 36, 414 + 76 * k, 1196);
        }

        choice (P::chorus_mode, { "FREE", "I", "II", "I+II" }, { 60, 40, 44, 60 }, 752, 1180);
        toggle (P::delay_sync, "SYNC", 752, 1230, 72);
        dropdown (P::delay_div, 832, 1230, 76);
        toggle (P::delay_pingpong, "PING-PONG", 916, 1230, 116);
        choice (P::reverb_type, { "HALL", "PLATE", "SPRING" }, { 58, 62, 70 }, 752, 1280);

        for (int v = 1; v <= P::kNumTrims; ++v)
        {
            const int x = 1098 + 48 * (v - 1);
            knob (P::trimTune (v), ("T" + juce::String (v)).toRawUTF8(), 20, x, 1162);
            knob (P::trimCut (v), ("C" + juce::String (v)).toRawUTF8(), 20, x, 1240);
        }

        moveExpansion = false;

        // ---- Row 4: tape echo (full width, like a rack unit) ----
        add (std::make_unique<ParamToggle> (state, P::echo_on, "ON"), { 200, 930, 70, 28 });
        dropdown (P::echo_mode, 72, 978, 176);
        {
            const char* ids[8] = { P::echo_rate, P::echo_intensity, P::echo_bass, P::echo_treble,
                                   P::echo_wow, P::echo_input, P::echo_volume, P::echo_reverb };
            const char* labels[8] = { "REPEAT", "INTENSITY", "BASS", "TREBLE", "WOW", "INPUT", "ECHO", "REVERB" };
            for (int k = 0; k < 8; ++k)
                knob (ids[k], labels[k], 44, 300 + 112 * k + (k >= 6 ? 30 : 0), 962);
        }
        echoMode = state.getRawParameterValue (P::echo_mode);
        for (const auto& [id, panel] : { std::pair { P::fuzz_on, 6 }, std::pair { P::phaser_on, 7 } })
        {
            const auto to = expansionPanels()[static_cast<size_t> (panel)].to;
            add (std::make_unique<ParamToggle> (state, id, "ON"), { to.getX() + 130, to.getY() + 10, 70, 28 });
        }

        // Master volume: instrument output, independent of the presets (their LEVEL matches their loudness).
        knob (P::master_volume, "MASTER", 36, 2112, 12);

        // Header (the preset box sits in the middle of the wide panel)
        prevPreset = add (std::make_unique<ArrowButton> (false), { presetBoxX + 6, 27, 30, 30 });
        nextPreset = add (std::make_unique<ArrowButton> (true), { presetBoxX + 264, 27, 30, 30 });

        const auto rectPath = [] { juce::Path p; p.addRoundedRectangle (3.0f, 3.0f, 12.0f, 12.0f, 1.5f); return p; };
        const auto circlePath = [] { juce::Path p; p.addEllipse (6.5f, 6.5f, 5.0f, 5.0f); return p; };
        undo = add (std::make_unique<HeaderButton> ("UNDO", std::vector<juce::Path> { svgPath ("M5 7 H12 A4 4 0 0 1 12 15 H7"), svgPath ("M7 4 L4 7 L7 10") }), { 1150 + headerShift, 29, 44, 42 });
        redo = add (std::make_unique<HeaderButton> ("REDO", std::vector<juce::Path> { svgPath ("M13 7 H6 A4 4 0 0 0 6 15 H11"), svgPath ("M11 4 L14 7 L11 10") }), { 1216 + headerShift, 29, 44, 42 });
        browser = add (std::make_unique<HeaderButton> ("BROWSER", std::vector<juce::Path> { rectPath(), svgPath ("M6 7 H12 M6 10 H12 M6 13 H10") }), { 1282 + headerShift, 29, 60, 42 });
        settings = add (std::make_unique<HeaderButton> ("SETTINGS", std::vector<juce::Path> { circlePath(), svgPath ("M9 2 V4 M9 14 V16 M2 9 H4 M14 9 H16 M4 4 L5.5 5.5 M12.5 12.5 L14 14 M4 14 L5.5 12.5 M12.5 5.5 L14 4") }), { 1364 + headerShift, 29, 64, 42 });

        setSize (width, height);
    }

    std::function<juce::String()> presetName;
    std::function<void()> onPresetNameClicked;

    ArrowButton* prevPreset = nullptr;
    ArrowButton* nextPreset = nullptr;
    HeaderButton *undo = nullptr, *redo = nullptr, *browser = nullptr, *settings = nullptr;

    void poll()
    {
        for (auto* e : envelopes)
            e->poll();
        voices->poll();
        if (echoMode != nullptr && juce::roundToInt (echoMode->load()) != shownEchoMode)
        {
            shownEchoMode = juce::roundToInt (echoMode->load());
            repaint (echoPanel().toNearestInt());
        }
        const auto name = presetName ? presetName() : juce::String();
        if (name != shownPreset)
        {
            shownPreset = name;
            repaint (presetBox().toNearestInt());
        }
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (presetBox().reduced (40.0f, 0.0f).contains (e.position) && onPresetNameClicked)
            onPresetNameClicked();
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (colours::background);

        // Walnut cheeks and the main panel
        const float w = static_cast<float> (width), h = static_cast<float> (height);
        drawWalnut (g, { 0.0f, 0.0f, 24.0f, h }, false);
        drawWalnut (g, { w - 24.0f, 0.0f, 24.0f, h }, true);
        {
            juce::ColourGradient bg (juce::Colour (0xff1c1c1f), w * 0.5f, 0.0f, juce::Colour (0xff0f0f11), w * 0.5f, 1100.0f, true);
            bg.addColour (0.6, juce::Colour (0xff121214));
            g.setGradientFill (bg);
            g.fillRect (24.0f, 0.0f, w - 48.0f, h);
            g.setColour (juce::Colour (0xff2a2a2e));
            g.fillRect (24.0f, 0.0f, w - 48.0f, 1.0f);
        }

        paintHeader (g);

        // Row 1
        drawPanel (g, { 52, 118, 560, 290 }, "OSCILLATORS");
        drawSubPanel (g, { 68, 162, 258, 230 }, "OSC 1");
        drawSubPanel (g, { 338, 162, 258, 230 }, "OSC 2");
        drawCaption (g, "OCTAVE", 240, 338);
        drawCaption (g, "OCTAVE", 514, 338);
        drawPanel (g, { 624, 118, 190, 290 }, "MIXER");
        drawPanel (g, { 826, 118, 280, 290 }, "FILTER");
        drawCaption (g, "MODEL", 842, 292);
        drawCaption (g, "SLOPE", 970, 292);
        drawCaption (g, "KEY TRACK", 842, 347);
        drawCaption (g, "MODE", 1010, 347);
        drawPanel (g, { 1118, 118, 366, 290 }, "AMPLIFIER");

        // Row 2
        drawPanel (g, { 52, 420, 470, 300 }, "MODULATION MATRIX");
        drawCaption (g, "SOURCE", 92, 464);
        drawCaption (g, "DESTINATION", 258, 464);
        drawCaption (g, "AMOUNT", 420, 464);
        drawPanel (g, { 534, 420, 190, 300 }, "LFO");
        drawPanel (g, { 736, 420, 576, 300 }, "ENVELOPES");
        drawSubPanel (g, { 752, 464, 266, 240 }, "FILTER ENV");
        drawSubPanel (g, { 1030, 464, 266, 240 }, "AMP ENV");
        drawPanel (g, { 1324, 420, 160, 300 }, "POLY MOD");
        drawCaption (g, "DESTINATION", 1340, 578);

        // Row 3
        drawPanel (g, { 52, 732, 330, 178 }, "PER VOICE");
        drawPanel (g, { 394, 732, 130, 178 }, "VINTAGE");
        drawTracked (g, "NEW", { 411, 874, 48, 10 }, Fonts::jost (8.5f, false, 0.14f), colours::caption, juce::Justification::centredLeft);
        drawTracked (g, "WORN", { 459, 874, 48, 10 }, Fonts::jost (8.5f, false, 0.14f), colours::caption, juce::Justification::centredRight);
        drawPanel (g, { 536, 732, 690, 178 }, "EFFECTS");
        const char* fxNames[3] = { "CHORUS", "DELAY", "REVERB" };
        for (int f = 0; f < 3; ++f)
            drawSubPanel (g, { fxBoxX (f), 776.0f, 211.33f, 118.0f }, fxNames[f], 11.0f);
        drawPanel (g, { 1238, 732, 246, 178 }, "GLOBAL");

        paintTapeEcho (g);

        // Expansion block (right-hand side)
        for (const auto& p : expansionPanels())
            drawPanel (g, p.to.toFloat(), p.title);
        const auto caption = [&g, this] (const char* text, int x, int y) {
            const auto at = moved ({ x, y });
            drawCaption (g, text, static_cast<float> (at.x), static_cast<float> (at.y));
        };
        caption ("MODE", 68, 1008);
        caption ("RATE", 186, 1008);
        caption ("OCTAVES", 274, 1008);
        caption ("WAVE", 480, 964);
        caption ("SUB OCTAVE", 1054, 1046);
        caption ("VOICE MODE", 1376, 964);
        caption ("QUALITY", 1376, 1046);
        caption ("CHORUS MODE", 752, 1164);
        caption ("DELAY", 752, 1214);
        caption ("REVERB", 752, 1264);
        {
            const auto trims = expansionPanels()[5].to.toFloat();
            drawTracked (g, "TUNE  /  CUTOFF", { trims.getRight() - 236.0f, trims.getY() + 14.0f, 220.0f, 16.0f }, Fonts::jost (9.0f, false, 0.2f),
                         colours::caption, juce::Justification::centredRight);
        }

        // Footer
        g.setColour (juce::Colour (0xff1f1f23));
        g.fillRect (52.0f, 1162.0f, w - 104.0f, 1.0f);
        drawLogo (g, 52.0f, 1176.0f, 30.0f / 34.0f, colours::headerButton);
        drawTracked (g, juce::String (juce::CharPointer_UTF8 ("AUGUR-5 \xe2\x80\x9c" "3340\xe2\x80\x9d")), { 96, 1176, 400, 18 },
                     Fonts::michroma (11.0f, 0.24f), colours::headerButton, juce::Justification::centredLeft);
        drawTracked (g, "A  TONAL LAB  INSTRUMENT", { w * 0.5f - 200.0f, 1176, 400, 18 }, Fonts::michroma (10.0f, 0.3f),
                     colours::accent.withAlpha (0.85f), juce::Justification::centred);
        drawTracked (g, "ANALOG SOUL  /  DIGITAL PRECISION", { w - 452.0f, 1176, 400, 18 }, Fonts::jost (10.0f, false, 0.26f),
                     colours::caption, juce::Justification::centredRight);
    }

private:
    static float fxBoxX (int f) { return 552.0f + 223.33f * static_cast<float> (f); }
    // Wide layout: the original three rows on the left, the expansion modules in a block on the right.
    static constexpr int width = 2608, height = 1216;
    static constexpr int headerShift = width - 1536;
    static constexpr int presetBoxX = width / 2 - 150;
    static juce::Rectangle<float> presetBox() { return { static_cast<float> (presetBoxX), 22.0f, 300.0f, 40.0f }; }

    // Each expansion panel: where its controls were designed (from, 190 px high) and where it sits (to).
    // Controls keep their layout inside the panel and are centred in the new size.
    struct MovedPanel
    {
        juce::Rectangle<int> from, to;
        const char* title;
    };
    static const std::array<MovedPanel, 9>& expansionPanels()
    {
        static const std::array<MovedPanel, 9> panels { {
            { { 52, 922, 400, 190 }, { 1496, 118, 462, 290 }, "ARPEGGIATOR" },
            { { 464, 922, 250, 190 }, { 1970, 118, 311, 290 }, "LFO 2" },
            { { 1282, 922, 202, 190 }, { 2293, 118, 263, 290 }, "HPF  /  VOICE" },
            { { 726, 922, 300, 190 }, { 1496, 420, 300, 300 }, "MOD ENV" },
            { { 736, 1124, 332, 190 }, { 1808, 420, 332, 300 }, "FX OPTIONS" },
            { { 1080, 1124, 404, 190 }, { 2152, 420, 404, 300 }, "VOICE TRIMS" },
            { { 52, 1124, 330, 190 }, { 1496, 732, 378, 178 }, "FUZZ" },
            { { 394, 1124, 330, 190 }, { 1886, 732, 378, 178 }, "PHASER" },
            { { 1038, 922, 232, 190 }, { 2276, 732, 280, 178 }, "OSC +" },
        } };
        return panels;
    }
    static juce::Point<int> moved (juce::Point<int> p)
    {
        for (const auto& m : expansionPanels())
            if (m.from.contains (p))
                return p + juce::Point<int> (m.to.getX() + (m.to.getWidth() - m.from.getWidth()) / 2 - m.from.getX(),
                                             m.to.getY() + juce::jmax (0, (m.to.getHeight() - m.from.getHeight()) / 2) - m.from.getY());
        return p;
    }
    bool moveExpansion = false;
    std::atomic<float>* echoMode = nullptr;
    int shownEchoMode = -1;

    static juce::Rectangle<float> echoPanel() { return { 52.0f, 922.0f, static_cast<float> (width - 104), 190.0f }; }

    // Tape echo row: the selector, the playback heads it uses and a drawing of the tape loop.
    void paintTapeEcho (juce::Graphics& g)
    {
        static constexpr int headMask[12] = { 1, 2, 4, 6, 1, 2, 4, 3, 6, 5, 7, 0 };
        const int mode = juce::jlimit (0, 11, shownEchoMode < 0 ? 3 : shownEchoMode);
        const bool reverb = mode >= 4;

        const auto panel = echoPanel();
        drawPanel (g, panel, "TAPE ECHO");
        drawCaption (g, "MODE SELECTOR", 72, 962);
        drawCaption (g, "PLAYBACK HEADS", 72, 1022);
        for (int h = 0; h < 3; ++h)
        {
            const juce::Rectangle<float> led { 72.0f + 46.0f * static_cast<float> (h), 1040.0f, 38.0f, 26.0f };
            const bool on = (headMask[mode] >> h & 1) != 0;
            g.setColour (on ? colours::accent.withAlpha (0.9f) : colours::ledOff);
            g.fillRoundedRectangle (led, 4.0f);
            drawTracked (g, juce::String (h + 1), led, Fonts::jost (11.0f, true), on ? juce::Colours::black : colours::caption,
                         juce::Justification::centred);
        }
        {
            const juce::Rectangle<float> led { 210.0f, 1040.0f, 64.0f, 26.0f };
            g.setColour (reverb ? colours::accent.withAlpha (0.9f) : colours::ledOff);
            g.fillRoundedRectangle (led, 4.0f);
            drawTracked (g, "SPRING", led, Fonts::jost (9.0f, true, 0.1f), reverb ? juce::Colours::black : colours::caption,
                         juce::Justification::centred);
        }

        // Tape loop: two reels, the tape path and the heads (record + three playback heads).
        const float x0 = 1300.0f, y0 = 952.0f, x1 = panel.getRight() - 90.0f;
        const float reelR = 58.0f, cy = y0 + 70.0f;
        const juce::Point<float> leftReel { x0 + reelR, cy }, rightReel { x1 - reelR, cy };
        for (const auto& c : { leftReel, rightReel })
        {
            g.setColour (juce::Colour (0xff111113));
            g.fillEllipse (c.x - reelR, c.y - reelR, reelR * 2.0f, reelR * 2.0f);
            g.setColour (colours::panelBorder.brighter (0.3f));
            g.drawEllipse (c.x - reelR, c.y - reelR, reelR * 2.0f, reelR * 2.0f, 1.5f);
            g.drawEllipse (c.x - 12.0f, c.y - 12.0f, 24.0f, 24.0f, 1.5f);
            for (int spoke = 0; spoke < 3; ++spoke)
            {
                const float a = juce::MathConstants<float>::twoPi * static_cast<float> (spoke) / 3.0f;
                g.drawLine (c.x + 14.0f * std::cos (a), c.y + 14.0f * std::sin (a), c.x + (reelR - 8.0f) * std::cos (a),
                            c.y + (reelR - 8.0f) * std::sin (a), 1.2f);
            }
        }
        // Tape path across the head block, below the reels.
        const float tapeY = cy + reelR + 6.0f;
        g.setColour (juce::Colour (0xff6a4a2f));
        g.drawLine (leftReel.x, cy - reelR, rightReel.x, cy - reelR, 2.0f);
        g.drawLine (leftReel.x - reelR + 2.0f, cy, leftReel.x - reelR + 2.0f, tapeY - 6.0f, 2.0f);
        g.drawLine (rightReel.x + reelR - 2.0f, cy, rightReel.x + reelR - 2.0f, tapeY - 6.0f, 2.0f);
        g.drawLine (leftReel.x - reelR + 2.0f, tapeY - 6.0f, rightReel.x + reelR - 2.0f, tapeY - 6.0f, 2.0f);

        const float span = (rightReel.x - leftReel.x) - 2.0f * reelR;
        const float headY = tapeY - 22.0f;
        const auto head = [&] (float x, const juce::String& text, bool on, bool record) {
            const juce::Rectangle<float> r { x - 17.0f, headY, 34.0f, 18.0f };
            g.setColour (on ? colours::accent : (record ? colours::label.darker (0.3f) : colours::ledOff));
            g.fillRoundedRectangle (r, 3.0f);
            drawTracked (g, text, r.translated (0.0f, -20.0f), Fonts::jost (9.0f, true, 0.1f), on ? colours::accent : colours::caption,
                         juce::Justification::centred);
        };
        const float start = leftReel.x + reelR + span * 0.12f;
        const float unit = span * 0.2f;
        head (start, "REC", false, true);
        const float ratio[3] = { 1.0f, 1.95f, 2.9f };
        for (int h = 0; h < 3; ++h)
            head (start + unit * ratio[h], "HEAD " + juce::String (h + 1), (headMask[mode] >> h & 1) != 0, false);
    }

    void paintHeader (juce::Graphics& g)
    {
        // Logo type: AUGUR-5 + 3340 on a shared baseline, then the tagline.
        const auto big = Fonts::michroma (26.0f, 0.14f);
        const auto small = Fonts::michroma (12.0f, 0.16f);
        const float baseline = 60.0f;
        g.setColour (colours::text);
        g.setFont (big);
        g.drawSingleLineText ("AUGUR-5", 52, juce::roundToInt (baseline));
        const float w1 = juce::GlyphArrangement::getStringWidth (big, "AUGUR-5");
        g.setColour (colours::accent);
        g.setFont (small);
        g.drawSingleLineText ("3340", juce::roundToInt (52.0f + w1 + 12.0f), juce::roundToInt (baseline));
        const float w2 = juce::GlyphArrangement::getStringWidth (small, "3340");
        const float taglineX = 52.0f + w1 + 12.0f + w2 + 26.0f;
        const auto tagline = Fonts::jost (11.0f, false, 0.28f);
        drawTracked (g, "ANALOG MODELING", { taglineX, 36.0f, 240.0f, 14.0f }, tagline, colours::captionLight, juce::Justification::centredLeft);
        drawTracked (g, "POLYSYNTH", { taglineX, 52.0f, 240.0f, 14.0f }, tagline, colours::captionLight, juce::Justification::centredLeft);

        // Preset box
        const auto box = presetBox();
        g.setColour (juce::Colour (0xff0d0d0f));
        g.fillRoundedRectangle (box, 6.0f);
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.drawRoundedRectangle (box.reduced (1.5f), 5.0f, 2.0f);
        g.setColour (juce::Colour (0xff2a2a2f));
        g.drawRoundedRectangle (box.reduced (0.5f), 6.0f, 1.0f);
        drawTracked (g, shownPreset, box.reduced (40.0f, 0.0f), Fonts::jost (15.0f, false, 0.04f), colours::text, juce::Justification::centred);
        drawTracked (g, "PRESET", { box.getX(), 68.0f, box.getWidth(), 11.0f }, Fonts::jost (9.0f, false, 0.24f), colours::caption,
                     juce::Justification::centred);

        drawLogo (g, 1450.0f + headerShift, 40.0f, 1.0f, juce::Colour (0xffc9c1b5));

        g.setColour (juce::Colour (0xff26262b));
        g.fillRect (52.0f, 100.0f, static_cast<float> (width - 104), 1.0f);
    }

    template <typename C>
    C* add (std::unique_ptr<C> c, juce::Rectangle<int> bounds)
    {
        auto* raw = c.get();
        addAndMakeVisible (*raw);
        if (moveExpansion)
        {
            // Knob bounds carry a label margin to the left: locate the panel by the visible part.
            const auto anchor = bounds.getPosition() + juce::Point<int> (Knob::labelMargin + 1, 1);
            bounds.setPosition (moved (anchor) - (anchor - bounds.getPosition()));
        }
        raw->setBounds (bounds);
        owned.push_back (std::move (c));
        return raw;
    }

    void knob (const juce::String& id, const char* label, int size, int x, int y)
    {
        add (std::make_unique<Knob> (state, id, juce::String::fromUTF8 (label), size), Knob::boundsFor (size).translated (x, y));
    }

    void toggle (const juce::String& id, const char* label, int x, int y, int w)
    {
        add (std::make_unique<ParamToggle> (state, id, juce::String::fromUTF8 (label)), { x, y, w, 28 });
    }

    void dropdown (const juce::String& id, int x, int y, int w)
    {
        add (std::make_unique<ParamChoiceBox> (state, id), { x, y, w, 28 });
    }

    void showMatrixPage (int page)
    {
        for (size_t i = 0; i < matrixRows.size(); ++i)
            if (matrixRows[i] != nullptr)
                matrixRows[i]->setVisible (static_cast<int> (i) / 4 == page);
        for (size_t p = 0; p < matrixPages.size(); ++p)
            if (matrixPages[p] != nullptr)
                matrixPages[p]->setToggleState (static_cast<int> (p) == page, juce::dontSendNotification);
    }

    void wave (const juce::String& id, WaveIcon icon, int x, int y)
    {
        add (std::make_unique<ParamWaveButton> (state, id, icon), { x, y, 34, 26 });
    }

    void choice (const juce::String& id, std::vector<const char*> labels, std::vector<int> widths, int x, int y)
    {
        std::vector<std::unique_ptr<juce::Button>> b;
        for (auto* l : labels)
            b.push_back (std::make_unique<LedToggle> (juce::String::fromUTF8 (l)));
        int total = 0;
        for (auto w : widths)
            total += w + 8;
        auto* group = add (std::make_unique<ChoiceGroup> (state, id, std::move (b)), { x, y, total - 8, 28 });
        int bx = 0;
        for (size_t i = 0; i < widths.size(); ++i)
        {
            group->getButton (static_cast<int> (i)).setBounds (bx, 0, widths[i], 28);
            bx += widths[i] + 8;
        }
    }

    APVTS& state;
    std::vector<std::unique_ptr<juce::Component>> owned;
    std::array<EnvelopeDisplay*, 3> envelopes {};
    std::array<MatrixRow*, P::kNumMatrixSlots> matrixRows {};
    std::array<juce::Button*, 2> matrixPages {};
    VoiceActivity* voices = nullptr;
    juce::String shownPreset;
};

} // namespace augur5::ui

//==============================================================================

Augur5Editor::Augur5Editor (Augur5Processor& p) : AudioProcessorEditor (p), processor (p)
{
    setLookAndFeel (&lookAndFeel);

    canvas = std::make_unique<augur5::ui::Canvas> (p.getParameters(), [&p] (int v) { return p.getVoiceLevel (v); });
    addAndMakeVisible (*canvas);
    canvas->addMouseListener (this, true);

    canvas->presetName = [this] { return processor.getPresets().getCurrentName(); };
    canvas->onPresetNameClicked = [this] { showBrowserMenu(); };
    canvas->prevPreset->onClick = [this] { processor.getPresets().previous(); };
    canvas->nextPreset->onClick = [this] { processor.getPresets().next(); };
    canvas->undo->onClick = [this] { processor.getUndoManager().undo(); };
    canvas->redo->onClick = [this] { processor.getUndoManager().redo(); };
    canvas->browser->onClick = [this] { showBrowserMenu(); };
    canvas->settings->onClick = [this] { showSettingsMenu(); };

    setResizable (true, true);
    setResizeLimits (designWidth * 2 / 5, designHeight * 2 / 5, designWidth * 2, designHeight * 2);
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio (static_cast<double> (designWidth) / designHeight);
    // Never open wider or taller than the screen (a session may have been saved on a bigger display).
    float scale = p.getUiScale();
    if (const auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
    {
        const auto area = display->userBounds;
        scale = juce::jmin (scale, static_cast<float> (area.getWidth() - 60) / designWidth,
                            static_cast<float> (area.getHeight() - 120) / designHeight);
    }
    setScale (scale);

    startTimerHz (30);
}

Augur5Editor::~Augur5Editor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void Augur5Editor::setScale (float scale)
{
    scale = juce::jlimit (0.4f, 2.0f, scale);
    setSize (juce::roundToInt (designWidth * scale), juce::roundToInt (designHeight * scale));
}

void Augur5Editor::paint (juce::Graphics& g)
{
    g.fillAll (augur5::ui::colours::background);
}

void Augur5Editor::resized()
{
    const float scale = static_cast<float> (getWidth()) / designWidth;
    canvas->setTransform (juce::AffineTransform::scale (scale));
    canvas->setBounds (0, 0, designWidth, designHeight);
    processor.setUiScale (scale);
}

void Augur5Editor::mouseDown (const juce::MouseEvent&)
{
    // Every click-drag gesture becomes one undo step.
    processor.getUndoManager().beginNewTransaction();
}

void Augur5Editor::timerCallback()
{
    canvas->poll();
}

void Augur5Editor::showBrowserMenu()
{
    auto& presets = processor.getPresets();
    // Factory presets grouped by category (in bank order), each category as a sub-menu.
    juce::PopupMenu menu;
    juce::StringArray categories;
    for (int i = 0; i < presets.getNumFactoryPresets(); ++i)
        categories.addIfNotAlreadyThere (presets.getFactoryCategory (i));
    menu.addSectionHeader ("FACTORY");
    for (const auto& category : categories)
    {
        juce::PopupMenu sub;
        for (int i = 0; i < presets.getNumFactoryPresets(); ++i)
            if (presets.getFactoryCategory (i) == category)
                sub.addItem (presets.getFactoryName (i), true, presets.getCurrentName() == presets.getFactoryName (i),
                             [this, i] { processor.getPresets().loadFactory (i); });
        menu.addSubMenu (category, sub);
    }

    // User presets and installed expansion packs: one sub-menu per folder level (Pack > Genre > preset).
    const auto users = presets.getUserPresets();
    if (! users.isEmpty())
    {
        struct Node
        {
            std::map<juce::String, Node> folders;
            juce::Array<juce::File> files;
        };
        Node root;
        const auto base = augur5::PresetManager::getUserFolder();
        for (const auto& f : users)
        {
            auto parts = juce::StringArray::fromTokens (f.getParentDirectory().getRelativePathFrom (base).replaceCharacter ('\\', '/'), "/", "");
            parts.removeEmptyStrings();
            parts.removeString (".");
            Node* node = &root;
            for (const auto& part : parts)
                node = &node->folders[part];
            node->files.add (f);
        }
        std::function<juce::PopupMenu (const Node&)> build = [&] (const Node& node) {
            juce::PopupMenu m;
            for (const auto& [name, child] : node.folders)
                m.addSubMenu (name, build (child));
            for (const auto& f : node.files)
                m.addItem (f.getFileNameWithoutExtension(), true, processor.getPresets().getCurrentName() == f.getFileNameWithoutExtension(),
                           [this, f] { processor.getPresets().loadUser (f); });
            return m;
        };
        menu.addSectionHeader ("USER  /  EXPANSIONS");
        const auto tree = build (root);
        for (juce::PopupMenu::MenuItemIterator it (tree); it.next();)
            menu.addItem (it.getItem());
    }

    menu.addSeparator();
    menu.addItem ("Save preset...", [this] { showSaveDialog(); });
    menu.addItem ("Install expansion pack...", [this] { showInstallPackDialog(); });
    menu.addItem ("Open presets folder", [] {
        auto folder = augur5::PresetManager::getUserFolder();
        folder.createDirectory();
        folder.startAsProcess();
    });

    menu.setLookAndFeel (&lookAndFeel);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (canvas->browser).withParentComponent (this));
}

void Augur5Editor::showSettingsMenu()
{
    auto& state = processor.getParameters();
    juce::PopupMenu size;
    for (const auto s : { 0.45f, 0.55f, 0.65f, 0.72f, 0.85f, 1.0f })
        size.addItem (juce::String (juce::roundToInt (s * 100.0f)) + " %", true, std::abs (processor.getUiScale() - s) < 0.01f,
                      [this, s] { setScale (s); });

    const auto setParam = [&state] (const juce::String& id, float value) {
        if (auto* p = state.getParameter (id))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->convertTo0to1 (value));
            p->endChangeGesture();
        }
    };

    juce::PopupMenu vco;
    const int model = juce::roundToInt (state.getRawParameterValue (augur5::params::osc_model)->load());
    vco.addItem (juce::String (juce::CharPointer_UTF8 ("REV 3 \xc2\xb7 CEM3340")), true, model == 0, [setParam] { setParam (augur5::params::osc_model, 0.0f); });
    vco.addItem (juce::String (juce::CharPointer_UTF8 ("REV 1 \xc2\xb7 SSM2030")), true, model == 1, [setParam] { setParam (augur5::params::osc_model, 1.0f); });

    juce::PopupMenu bend;
    const int range = juce::roundToInt (state.getRawParameterValue (augur5::params::pb_range)->load());
    for (const int r : { 1, 2, 3, 5, 7, 12, 24 })
        bend.addItem (juce::String (r) + " st", true, range == r, [setParam, r] { setParam (augur5::params::pb_range, static_cast<float> (r)); });

    const bool vintage = state.getRawParameterValue (augur5::params::vintage_cv)->load() > 0.5f;
    const bool offlineDivine = state.getRawParameterValue (augur5::params::offline_quality)->load() > 0.5f;

    juce::PopupMenu menu;
    menu.addSubMenu ("Window size", size);
    menu.addSubMenu ("VCO model", vco);
    menu.addSubMenu ("Pitch bend range", bend);
    menu.addItem ("Vintage 7-bit knobs (Rev 3)", true, vintage,
                  [setParam, vintage] { setParam (augur5::params::vintage_cv, vintage ? 0.0f : 1.0f); });
    menu.addItem ("Render offline in DIVINE quality", true, offlineDivine,
                  [setParam, offlineDivine] { setParam (augur5::params::offline_quality, offlineDivine ? 0.0f : 1.0f); });
    menu.addSeparator();
    menu.addItem (juce::String ("AUGUR-5 v") + JucePlugin_VersionString + "  -  TONAL LAB", false, false, [] {});

    menu.setLookAndFeel (&lookAndFeel);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (canvas->settings).withParentComponent (this));
}

void Augur5Editor::showInstallPackDialog()
{
    packChooser = std::make_unique<juce::FileChooser> ("Install an AUGUR-5 expansion pack",
                                                        juce::File::getSpecialLocation (juce::File::userDesktopDirectory), "*.zip");
    packChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this] (const juce::FileChooser& fc) {
        const auto file = fc.getResult();
        if (! file.existsAsFile())
            return;
        const int count = augur5::PresetManager::installPack (file);
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::NoIcon, "Expansion pack",
                                                count > 0 ? juce::String (count) + " presets installed. Open BROWSER > USER / EXPANSIONS."
                                                          : juce::String ("This file is not an AUGUR-5 expansion pack."),
                                                "OK", this);
    });
}

void Augur5Editor::showSaveDialog()
{
    saveDialog = std::make_unique<juce::AlertWindow> ("Save preset", "Preset name:", juce::MessageBoxIconType::NoIcon, this);
    saveDialog->setLookAndFeel (&lookAndFeel);
    saveDialog->addTextEditor ("name", processor.getPresets().getCurrentName());
    saveDialog->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    saveDialog->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    saveDialog->enterModalState (true, juce::ModalCallbackFunction::create ([this] (int result) {
        if (result == 1 && saveDialog != nullptr)
            processor.getPresets().saveUser (saveDialog->getTextEditorContents ("name"));
        saveDialog.reset();
    }), false);
}
