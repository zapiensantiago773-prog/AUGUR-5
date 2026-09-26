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

        wave (P::osc2_saw, WaveIcon::Saw, 470, 174);
        wave (P::osc2_tri, WaveIcon::Triangle, 510, 174);
        wave (P::osc2_pulse, WaveIcon::Pulse, 550, 174);
        knob (P::osc2_freq, "FREQUENCY", 64, 350, 227);
        knob (P::osc2_fine, "FINE", 36, 451, 255);
        knob (P::osc2_pw, "WIDTH", 36, 524, 255);
        toggle (P::osc2_lofreq, "LO FREQ", 350, 352, 84);
        toggle (P::osc2_kbd, "KBD", 442, 352, 64);

        // Mixer
        knob (P::mix_osc1, "OSC 1", 36, 648, 199);
        knob (P::mix_osc2, "OSC 2", 36, 731, 199);
        knob (P::mix_noise, "NOISE", 36, 648, 285);
        knob (P::mix_drive, "DRIVE", 36, 731, 285);

        // Filter
        knob (P::flt_cutoff, "CUTOFF", 64, 842, 162);
        knob (P::flt_reso, "RESONANCE", 44, 942, 182);
        knob (P::flt_env_amt, "ENV AMT", 44, 1022, 182);
        choice (P::flt_model, { "REV 3 \xc2\xb7 CEM", "REV 1 \xc2\xb7 SSM" }, { 120, 120 }, 842, 309);
        choice (P::flt_keytrack, { "OFF", "HALF", "FULL" }, { 76, 76, 76 }, 842, 364);

        // Amplifier
        knob (P::amp_velocity, "VEL \xe2\x80\xba AMP", 44, 1134, 162);
        knob (P::flt_velocity, "VEL \xe2\x80\xba FILTER", 44, 1234, 162);
        knob (P::at_amount, "AFTERTOUCH", 44, 1334, 162);
        voices = add (std::make_unique<VoiceActivity> (state, std::move (voiceLevel)), { 1134, 326, 268, 66 });
        add (std::make_unique<LevelFader> (state), { 1418, 162, 50, 230 });

        // Mod matrix
        for (int slot = 1; slot <= 4; ++slot)
            add (std::make_unique<MatrixRow> (state, slot), { 68, juce::roundToInt (487.0f + 62.33f * static_cast<float> (slot - 1)), 438, 30 });

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

        // Header
        prevPreset = add (std::make_unique<ArrowButton> (false), { 624, 27, 30, 30 });
        nextPreset = add (std::make_unique<ArrowButton> (true), { 882, 27, 30, 30 });

        const auto rectPath = [] { juce::Path p; p.addRoundedRectangle (3.0f, 3.0f, 12.0f, 12.0f, 1.5f); return p; };
        const auto circlePath = [] { juce::Path p; p.addEllipse (6.5f, 6.5f, 5.0f, 5.0f); return p; };
        undo = add (std::make_unique<HeaderButton> ("UNDO", std::vector<juce::Path> { svgPath ("M5 7 H12 A4 4 0 0 1 12 15 H7"), svgPath ("M7 4 L4 7 L7 10") }), { 1150, 29, 44, 42 });
        redo = add (std::make_unique<HeaderButton> ("REDO", std::vector<juce::Path> { svgPath ("M13 7 H6 A4 4 0 0 0 6 15 H11"), svgPath ("M11 4 L14 7 L11 10") }), { 1216, 29, 44, 42 });
        browser = add (std::make_unique<HeaderButton> ("BROWSER", std::vector<juce::Path> { rectPath(), svgPath ("M6 7 H12 M6 10 H12 M6 13 H10") }), { 1282, 29, 60, 42 });
        settings = add (std::make_unique<HeaderButton> ("SETTINGS", std::vector<juce::Path> { circlePath(), svgPath ("M9 2 V4 M9 14 V16 M2 9 H4 M14 9 H16 M4 4 L5.5 5.5 M12.5 12.5 L14 14 M4 14 L5.5 12.5 M12.5 5.5 L14 4") }), { 1364, 29, 64, 42 });

        setSize (1536, 1024);
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
        drawWalnut (g, { 0.0f, 0.0f, 24.0f, 1024.0f }, false);
        drawWalnut (g, { 1512.0f, 0.0f, 24.0f, 1024.0f }, true);
        {
            juce::ColourGradient bg (juce::Colour (0xff1c1c1f), 768.0f, 0.0f, juce::Colour (0xff0f0f11), 768.0f, 1100.0f, true);
            bg.addColour (0.6, juce::Colour (0xff121214));
            g.setGradientFill (bg);
            g.fillRect (24.0f, 0.0f, 1488.0f, 1024.0f);
            g.setColour (juce::Colour (0xff2a2a2e));
            g.fillRect (24.0f, 0.0f, 1488.0f, 1.0f);
        }

        paintHeader (g);

        // Row 1
        drawPanel (g, { 52, 118, 560, 290 }, "OSCILLATORS");
        drawSubPanel (g, { 68, 162, 258, 230 }, "OSC 1");
        drawSubPanel (g, { 338, 162, 258, 230 }, "OSC 2");
        drawPanel (g, { 624, 118, 190, 290 }, "MIXER");
        drawPanel (g, { 826, 118, 280, 290 }, "FILTER");
        drawCaption (g, "MODEL", 842, 292);
        drawCaption (g, "KEY TRACK", 842, 347);
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

        // Footer
        g.setColour (juce::Colour (0xff1f1f23));
        g.fillRect (52.0f, 970.0f, 1432.0f, 1.0f);
        drawLogo (g, 52.0f, 984.0f, 30.0f / 34.0f, colours::headerButton);
        drawTracked (g, juce::String (juce::CharPointer_UTF8 ("AUGUR-5 \xe2\x80\x9c" "3340\xe2\x80\x9d")), { 96, 984, 400, 18 },
                     Fonts::michroma (11.0f, 0.24f), colours::headerButton, juce::Justification::centredLeft);
        drawTracked (g, "ANALOG SOUL  /  DIGITAL PRECISION", { 1084, 984, 400, 18 }, Fonts::jost (10.0f, false, 0.26f),
                     colours::caption, juce::Justification::centredRight);
    }

private:
    static float fxBoxX (int f) { return 552.0f + 223.33f * static_cast<float> (f); }
    static juce::Rectangle<float> presetBox() { return { 618.0f, 22.0f, 300.0f, 40.0f }; }

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

        drawLogo (g, 1450.0f, 40.0f, 1.0f, juce::Colour (0xffc9c1b5));

        g.setColour (juce::Colour (0xff26262b));
        g.fillRect (52.0f, 100.0f, 1432.0f, 1.0f);
    }

    template <typename C>
    C* add (std::unique_ptr<C> c, juce::Rectangle<int> bounds)
    {
        auto* raw = c.get();
        addAndMakeVisible (*raw);
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
    std::array<EnvelopeDisplay*, 2> envelopes {};
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
    setResizeLimits (designWidth / 2, designHeight / 2, designWidth * 2, designHeight * 2);
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio (static_cast<double> (designWidth) / designHeight);
    setScale (p.getUiScale());

    startTimerHz (30);
}

Augur5Editor::~Augur5Editor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void Augur5Editor::setScale (float scale)
{
    scale = juce::jlimit (0.5f, 2.0f, scale);
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
    juce::PopupMenu menu;
    menu.addSectionHeader ("FACTORY");
    for (int i = 0; i < presets.getNumFactoryPresets(); ++i)
        menu.addItem (presets.getFactoryName (i), [this, i] { processor.getPresets().loadFactory (i); });

    const auto users = presets.getUserPresets();
    if (! users.isEmpty())
    {
        menu.addSectionHeader ("USER");
        for (const auto& f : users)
            menu.addItem (f.getFileNameWithoutExtension(), [this, f] { processor.getPresets().loadUser (f); });
    }

    menu.addSeparator();
    menu.addItem ("Save preset...", [this] { showSaveDialog(); });
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
    for (const auto s : { 0.6f, 0.75f, 1.0f, 1.25f, 1.5f })
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

    juce::PopupMenu menu;
    menu.addSubMenu ("Window size", size);
    menu.addSubMenu ("VCO model", vco);
    menu.addSubMenu ("Pitch bend range", bend);
    menu.addSeparator();
    menu.addItem (juce::String ("AUGUR-5 v") + JucePlugin_VersionString, false, false, [] {});

    menu.setLookAndFeel (&lookAndFeel);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (canvas->settings).withParentComponent (this));
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
