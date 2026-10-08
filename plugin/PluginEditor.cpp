#include "PluginEditor.h"

#include "Parameters.h"
#include "PluginProcessor.h"
#include "gui/AuguryPage.h"
#include "gui/Displays.h"
#include "gui/FxPage.h"
#include "gui/Horizon.h"
#include "gui/Overlays.h"
#include "gui/Theme.h"
#include "gui/Widgets.h"

namespace augur5::ui
{

namespace
{
namespace P = augur5::params;
using C = juce::Colour;

juce::String u8 (const char* s) { return juce::String::fromUTF8 (s); }

void repaintAll (juce::Component& c)
{
    c.repaint();
    for (auto* child : c.getChildren())
        repaintAll (*child);
}

// Builds controls at the design's absolute coordinates inside any container (canvas or tab page).
class Builder
{
public:
    Builder (APVTS& s, juce::Component& target, int yOffset, std::vector<std::unique_ptr<juce::Component>>& store)
        : state (s), owner (target), oy (yOffset), owned (store)
    {
    }

    template <typename T>
    T* add (std::unique_ptr<T> c, juce::Rectangle<int> bounds)
    {
        auto* raw = c.get();
        owner.addAndMakeVisible (*raw);
        raw->setBounds (bounds.translated (0, -oy));
        owned.push_back (std::move (c));
        return raw;
    }
    // A knob centred on (cx, cy).
    Knob* knob (const juce::String& id, const char* label, int d, int cx, int cy, C colour = colours::accent)
    {
        return add (std::make_unique<Knob> (state, id, u8 (label), d, colour), Knob::boundsFor (d).translated (cx - d / 2 - 12, cy - d / 2 - 8));
    }
    ParamToggle* toggle (const juce::String& id, const char* label, int x, int y, int w, C colour = colours::accent)
    {
        return add (std::make_unique<ParamToggle> (state, id, u8 (label), colour), { x, y, w, 28 });
    }
    void dropdown (const juce::String& id, int x, int y, int w) { add (std::make_unique<ParamChoiceBox> (state, id), { x, y, w, 28 }); }
    void segments (const juce::String& id, std::vector<const char*> labels, int x, int y, int w, C colour = colours::accent, int gap = 6)
    {
        std::vector<std::unique_ptr<juce::Button>> b;
        for (auto* l : labels)
        {
            auto s = std::make_unique<SegmentButton> (u8 (l), colour);
            s->idText = id;
            b.push_back (std::move (s));
        }
        const int n = static_cast<int> (labels.size());
        auto* group = add (std::make_unique<ChoiceGroup> (state, id, std::move (b)), { x, y, n * w + (n - 1) * gap, 28 });
        for (int i = 0; i < n; ++i)
            group->getButton (i).setBounds (i * (w + gap), 0, w, 28);
    }
    void wave (const juce::String& id, WaveIcon icon, int x, int y, C colour = colours::accent)
    {
        add (std::make_unique<ParamWaveButton> (state, id, icon, colour), { x, y, 34, 24 });
    }

private:
    APVTS& state;
    juce::Component& owner;
    int oy;
    std::vector<std::unique_ptr<juce::Component>>& owned;
};

// One tab: the area between the header and the performance strip, painted in canvas coordinates.
class Page final : public juce::Component
{
public:
    static constexpr int top = 106, height = 650;
    std::function<void (juce::Graphics&)> painter;
    void paint (juce::Graphics& g) override
    {
        g.addTransform (juce::AffineTransform::translation (0.0f, -static_cast<float> (top)));
        if (painter)
            painter (g);
    }
};

// The FX page's view of the processor.
class ProcessorFxModel final : public FxModel
{
public:
    explicit ProcessorFxModel (Augur5Processor& p) : proc (p) {}
    std::array<int, 8> getOrder() const override { return proc.getFxOrder(); }
    void setOrder (const std::array<int, 8>& o) override { proc.setFxOrder (o); }
    float compGainReduction() const override { return proc.getEngine().getCompGainReduction(); }
    float phaserHz() const override { return proc.getEngine().getPhaserHz(); }
    float flangerMs() const override { return proc.getEngine().getFlangerMs(); }
    float echoHeadMs() const override { return proc.getEngine().getEchoHeadMs(); }
    double tempo() const override { return proc.getEngine().getTempo(); }

private:
    Augur5Processor& proc;
};
} // namespace

//==============================================================================
class Canvas final : public juce::Component
{
public:
    explicit Canvas (Augur5Processor& proc) : state (proc.getParameters()), fxModel (proc)
    {
        for (size_t i = 0; i < 4; ++i)
        {
            pages[i] = std::make_unique<Page>();
            addChildComponent (*pages[i]);
            pages[i]->setBounds (0, Page::top, Augur5Editor::designWidth, Page::height);
        }
        fxPage = std::make_unique<FxPage> (state, fxModel);
        addChildComponent (*fxPage);
        fxPage->setBounds (0, Page::top, Augur5Editor::designWidth, Page::height);
        auguryPage = std::make_unique<AuguryPage> (proc.getAugury(), [&proc] { return proc.getPresets().getCurrentName(); });
        addChildComponent (*auguryPage);
        auguryPage->setBounds (0, Page::top, Augur5Editor::designWidth, Page::height);

        Builder main (state, *pages[0], Page::top, owned), mod (state, *pages[1], Page::top, owned), arp (state, *pages[2], Page::top, owned),
            voice (state, *pages[3], Page::top, owned), fixed (state, *this, 0, owned);
        const auto amber = colours::amber, slate = colours::slate, sage = colours::sage, plum = colours::plum;

        // ================================================================ MAIN: the classic panel
        // Oscillators
        main.wave (P::osc1_saw, WaveIcon::Saw, 232, 167);
        main.wave (P::osc1_pulse, WaveIcon::Pulse, 272, 167);
        main.knob (P::osc1_freq, "FREQUENCY", 58, 130, 236);
        main.knob (P::osc1_fine, "FINE", 34, 254, 214);
        main.knob (P::osc1_pw, "PULSE WIDTH", 34, 254, 286);
        main.toggle (P::osc1_sync, "SYNC", 84, 358, 92);
        main.dropdown (P::osc1_oct, 232, 358, 66);

        main.wave (P::osc2_saw, WaveIcon::Saw, 454, 167, slate);
        main.wave (P::osc2_tri, WaveIcon::Triangle, 494, 167, slate);
        main.wave (P::osc2_pulse, WaveIcon::Pulse, 534, 167, slate);
        main.knob (P::osc2_freq, "FREQUENCY", 58, 392, 236, slate);
        main.knob (P::osc2_fine, "FINE", 34, 516, 214, slate);
        main.knob (P::osc2_pw, "PULSE WIDTH", 34, 516, 286, slate);
        main.toggle (P::osc2_lofreq, "LO FREQ", 346, 358, 84, slate);
        main.toggle (P::osc2_kbd, "KBD", 436, 358, 54, slate);
        main.dropdown (P::osc2_oct, 496, 358, 66);

        // Mixer
        main.knob (P::mix_osc1, "OSC A", 40, 650, 198);
        main.knob (P::mix_osc2, "OSC B", 40, 728, 198, slate);
        main.knob (P::mix_noise, "NOISE", 40, 650, 298, colours::caption);
        main.knob (P::mix_drive, "DRIVE", 40, 728, 298, amber);

        // Filter
        main.knob (P::flt_cutoff, "CUTOFF", 64, 852, 208);
        main.knob (P::flt_reso, "RESONANCE", 42, 968, 194);
        main.knob (P::flt_env_amt, "ENV AMOUNT", 42, 1056, 194);
        main.segments (P::flt_keytrack, { "OFF", "HALF", "FULL" }, 802, 302, 48, colours::accent, 4);
        main.dropdown (P::flt_model, 966, 302, 144);
        views.push_back (main.add (std::make_unique<FilterCurve> (state, false), { 802, 342, 308, 62 }));

        // Voices
        main.add (std::make_unique<VoiceCounter> (state), { 1154, 160, 84, 56 });
        main.toggle (P::unison, "UNISON", 1254, 166, 100);
        main.toggle (P::legato, "LEGATO", 1366, 166, 100);
        main.knob (P::glide, "GLIDE", 40, 1180, 270, slate);
        main.knob (P::master_tune, "TUNE", 40, 1262, 270, slate);
        main.knob (P::voice_detune, "DETUNE", 40, 1344, 270, slate);
        main.knob (P::analog_age, "ANALOG AGE", 40, 1426, 270, amber);
        voices = main.add (std::make_unique<VoiceActivity> (state, [&proc] (int v) { return proc.getVoiceLevel (v); }), { 1154, 330, 314, 72 });

        // Poly mod
        main.toggle (P::pm_on, "ON", 206, 440, 60);
        main.knob (P::pm_fenv_amt, "FILTER ENV", 44, 112, 530);
        main.knob (P::pm_osc2_amt, "OSC B", 44, 222, 530, slate);
        main.toggle (P::pm_dst_freqa, "FREQ A", 68, 616, 62);
        main.toggle (P::pm_dst_pwa, "PW A", 136, 616, 62);
        main.toggle (P::pm_dst_filter, "FILTER", 204, 616, 62);

        // LFO
        {
            std::vector<std::unique_ptr<juce::Button>> b;
            for (auto icon : { WaveIcon::Triangle, WaveIcon::Saw, WaveIcon::Square, WaveIcon::SampleHold })
                b.push_back (std::make_unique<WaveButton> (icon, sage));
            auto* group = main.add (std::make_unique<ChoiceGroup> (state, P::lfo_wave, std::move (b)), { 310, 472, 202, 28 });
            for (int i = 0; i < 4; ++i)
                group->getButton (i).setBounds (i * 52, 0, 46, 28);
        }
        main.knob (P::lfo_rate, "RATE", 52, 362, 560, sage);
        main.knob (P::lfo_amount, "AMOUNT", 38, 468, 548, sage);
        main.knob (P::lfo_delay, "DELAY", 38, 468, 640, sage);
        main.toggle (P::lfo_sync, "SYNC", 318, 652, 90, sage);

        // Envelopes
        {
            const char* ids[2][4] = { { P::fenv_a, P::fenv_d, P::fenv_s, P::fenv_r }, { P::aenv_a, P::aenv_d, P::aenv_s, P::aenv_r } };
            const char* labels[4] = { "ATTACK", "DECAY", "SUSTAIN", "RELEASE" };
            for (int e = 0; e < 2; ++e)
            {
                const int x0 = e == 0 ? 552 : 822;
                const auto colour = e == 0 ? colours::accent : slate;
                envelopes.push_back (main.add (std::make_unique<EnvelopeDisplay> (state, ids[e][0], ids[e][1], ids[e][2], ids[e][3], colour),
                                               { x0 + 14, 506, 230, 94 }));
                for (int k = 0; k < 4; ++k)
                    main.knob (ids[e][k], labels[k], 32, juce::roundToInt (static_cast<float> (x0) + 42.75f + 57.5f * static_cast<float> (k)), 652, colour);
            }
        }

        // Amplifier
        main.add (std::make_unique<LevelFader> (state, P::amp_level, "LEVEL"), { 1126, 474, 48, 254 });
        main.knob (P::amp_velocity, "VEL \xe2\x80\xba AMP", 40, 1240, 524);
        main.knob (P::flt_velocity, "VEL \xe2\x80\xba FILTER", 40, 1330, 524);
        main.knob (P::at_amount, "AFTERTOUCH", 40, 1420, 524);
        main.knob (P::voice_spread, "STEREO SPREAD", 40, 1240, 630, slate);
        main.knob (P::voice_pan, "PAN", 40, 1330, 630, slate);

        pages[0]->painter = [] (juce::Graphics& g) {
            drawSection (g, { 52, 116, 540, 299 }, "OSCILLATORS", colours::accent);
            drawSubSection (g, { 68, 160, 246, 240 }, "OSC A", colours::accent);
            drawSubSection (g, { 330, 160, 246, 240 }, "OSC B", colours::slate);
            drawCaption (g, "OCTAVE", 232.0f, 344.0f, 70.0f);
            drawCaption (g, "OCTAVE", 496.0f, 344.0f, 70.0f);
            drawSection (g, { 604, 116, 170, 299 }, "MIXER", colours::accent);
            drawSection (g, { 786, 116, 340, 299 }, "FILTER", colours::accent);
            drawCaption (g, "KEYBOARD TRACK", 802.0f, 288.0f, 150.0f);
            drawCaption (g, "MODEL", 966.0f, 288.0f, 140.0f);
            drawSection (g, { 1138, 116, 346, 299 }, "VOICES", colours::slate);
            drawSection (g, { 52, 428, 230, 320 }, "POLY MOD", colours::accent);
            drawCaption (g, "SOURCES", 68.0f, 478.0f);
            drawCaption (g, "DESTINATIONS", 68.0f, 600.0f);
            drawNote (g, "Per voice and at audio rate: the filter envelope and OSC B move OSC A's pitch, its pulse width and the cutoff.",
                      { 68.0f, 660.0f, 200.0f, 80.0f });
            drawSection (g, { 294, 428, 230, 320 }, "LFO", colours::sage);
            drawNote (g, "Routed with the MOD WHEEL and the matrix (MOD tab).", { 310.0f, 694.0f, 200.0f, 44.0f });
            drawSection (g, { 536, 428, 560, 320 }, "ENVELOPES", colours::accent);
            drawSubSection (g, { 552, 472, 258, 262 }, "FILTER", colours::accent);
            drawSubSection (g, { 822, 472, 258, 262 }, "AMPLIFIER", colours::slate);
            drawSection (g, { 1108, 428, 376, 320 }, "AMPLIFIER", colours::accent);
        };

        // ================================================================ MOD
        for (int slot = 1; slot <= P::kNumMatrixSlots; ++slot)
            mod.add (std::make_unique<MatrixRow> (state, slot), { 68, 180 + 38 * (slot - 1), 828, 30 });
        mod.dropdown (P::lfo2_wave, 940, 176, 150);
        mod.toggle (P::lfo2_sync, "SYNC", 940, 218, 70, sage);
        mod.toggle (P::lfo2_retrig, "RETRIG", 1018, 218, 72, sage);
        mod.knob (P::lfo2_rate, "RATE", 46, 1150, 204, sage);
        views.push_back (mod.add (std::make_unique<LfoShape> (state), { 1196, 158, 272, 132 }));
        envelopes.push_back (mod.add (std::make_unique<EnvelopeDisplay> (state, P::menv_a, P::menv_d, P::menv_s, P::menv_r, plum), { 940, 364, 236, 124 }));
        {
            const char* ids[4] = { P::menv_a, P::menv_d, P::menv_s, P::menv_r };
            const char* labels[4] = { "ATTACK", "DECAY", "SUSTAIN", "RELEASE" };
            for (int k = 0; k < 4; ++k)
                mod.knob (ids[k], labels[k], 32, 1222 + 60 * k, 412, plum);
        }
        views.push_back (mod.add (std::make_unique<MatrixMap> (state), { 72, 526, 1392, 210 }));
        pages[1]->painter = [] (juce::Graphics& g) {
            drawSection (g, { 52, 116, 860, 388 }, "MODULATION MATRIX", colours::accent);
            drawCaption (g, "SOURCE", 92.0f, 162.0f);
            drawCaption (g, "DESTINATION", 422.0f, 162.0f);
            drawCaption (g, "AMOUNT", 738.0f, 162.0f);
            drawSection (g, { 924, 116, 560, 188 }, "LFO 2", colours::sage);
            drawCaption (g, "WAVE", 940.0f, 162.0f);
            drawSection (g, { 924, 316, 560, 188 }, "MOD ENVELOPE", colours::plum);
            drawSection (g, { 52, 516, 1432, 232 }, "ROUTING", colours::slate);
        };

        // ================================================================ ARP
        arp.toggle (P::arp_on, "ARP", 68, 160, 80);
        arp.toggle (P::arp_latch, "LATCH", 156, 160, 90);
        arp.segments (P::arp_mode, { "UP", "DOWN", "UP-DOWN", "RANDOM", "ORDER" }, 68, 220, 92);
        arp.dropdown (P::arp_rate, 68, 280, 120);
        arp.segments (P::arp_oct, { "1", "2", "3", "4" }, 200, 280, 40);
        arp.knob (P::arp_gate, "GATE", 48, 614, 186);
        arp.knob (P::arp_swing, "SWING", 48, 700, 186);
        views.push_back (arp.add (std::make_unique<ArpPattern> (state, [&proc] { return proc.getEngine().getTempo(); }), { 780, 160, 688, 240 }));
        keys = arp.add (std::make_unique<Keys> ([&proc] (int note, float vel, bool on) {
                            proc.sendUiMidi (on ? 0x90 : 0x80, note, on ? juce::jlimit (1, 127, juce::roundToInt (vel * 127.0f)) : 0);
                        },
                                                [&proc] (int note) { return proc.isKeyHeld (note); }),
                        { 72, 486, 1392, 248 });
        keys->setLowestOctave (1);
        {
            auto* down = arp.add (std::make_unique<ArrowButton> (false), { 1330, 438, 30, 30 });
            auto* up = arp.add (std::make_unique<ArrowButton> (true), { 1440, 438, 30, 30 });
            down->onClick = [this] { shiftKeys (-1); };
            up->onClick = [this] { shiftKeys (1); };
        }
        pages[2]->painter = [this] (juce::Graphics& g) {
            drawSection (g, { 52, 116, 700, 300 }, "ARPEGGIATOR", colours::accent);
            drawCaption (g, "MODE", 68.0f, 206.0f);
            drawCaption (g, "RATE", 68.0f, 266.0f);
            drawCaption (g, "OCTAVES", 200.0f, 266.0f);
            drawNote (g, "Plays the held keys in time with the host, or free-running at 120 BPM when the transport is stopped. "
                         "LATCH keeps the chord after the keys are released; GATE sets the note length, SWING delays every second step.",
                      { 68.0f, 330.0f, 660.0f, 70.0f });
            drawSection (g, { 764, 116, 720, 300 }, "PATTERN", colours::slate);
            drawSection (g, { 52, 428, 1432, 320 }, "KEYBOARD", colours::accent);
            const int lo = keys != nullptr ? keys->getLowestOctave() : 1;
            drawTracked (g, "C" + juce::String (lo) + u8 (" \xe2\x80\x93 C") + juce::String (lo + Keys::numOctaves), { 1362.0f, 438.0f, 78.0f, 30.0f },
                         Fonts::jost (10.0f, true, 0.12f), colours::label, juce::Justification::centred);
            drawTracked (g, u8 ("click and drag to play  \xc2\xb7  lower on the key = louder"), { 260.0f, 442.0f, 600.0f, 16.0f }, Fonts::jost (9.5f, false, 0.05f),
                         colours::caption, juce::Justification::centredLeft);
        };

        // ================================================================ VOICE
        voice.knob (P::mix_sub, "SUB", 48, 120, 214);
        voice.knob (P::mix_ring, "RING MOD", 48, 232, 214, slate);
        voice.knob (P::osc_xmod, "FM B \xe2\x80\xba A", 48, 344, 214, plum);
        voice.segments (P::sub_oct, { "-1 OCT", "-2 OCT" }, 68, 310, 80);
        voice.segments (P::flt_slope, { "24 dB", "12 dB" }, 550, 174, 70);
        voice.segments (P::flt_mode, { "LP", "BP", "HP" }, 720, 174, 52);
        voice.knob (P::hpf_cutoff, "HPF", 48, 950, 196);
        views.push_back (voice.add (std::make_unique<FilterCurve> (state, true), { 550, 252, 438, 148 }));
        voice.segments (P::osc_model, { "REV 3  \xc2\xb7  CEM3340", "REV 1  \xc2\xb7  SSM2030" }, 1032, 174, 215, colours::accent);
        voice.segments (P::quality, { "ECO", "GREAT", "DIVINE" }, 1032, 230, 141, slate);
        voice.toggle (P::offline_quality, "RENDER OFFLINE IN DIVINE", 1032, 270, 436, slate);
        voice.segments (P::voice_mode, { "POLY", "DUO" }, 1032, 330, 87);
        voice.dropdown (P::pb_range, 1230, 330, 110);
        voice.toggle (P::vintage_cv, "VINTAGE 7-BIT KNOBS", 1032, 370, 436, amber);
        for (int v = 1; v <= P::kNumTrims; ++v)
        {
            const int x0 = 68 + (v - 1) * 176;
            const int index = v - 1;
            trimLeds.push_back (voice.add (std::make_unique<Indicator> ("VOICE " + juce::String (v), colours::accent,
                                                                         [&proc, index] { return std::sqrt (proc.getVoiceLevel (index)); }),
                                           { x0 + 40, 482, 86, 28 }));
            voice.knob (P::trimTune (v), "TUNE", 44, x0 + 83, 560, slate);
            voice.knob (P::trimCut (v), "CUTOFF", 44, x0 + 83, 656);
        }
        pages[3]->painter = [] (juce::Graphics& g) {
            drawSection (g, { 52, 116, 470, 300 }, "OSCILLATOR EXTRAS", colours::accent);
            drawCaption (g, "SUB OCTAVE", 68.0f, 296.0f);
            drawNote (g, "SUB: a square one or two octaves under OSC A.  RING MOD: OSC A x OSC B.  FM: linear, through-zero, OSC B into OSC A.",
                      { 68.0f, 350.0f, 440.0f, 56.0f });
            drawSection (g, { 534, 116, 470, 300 }, "FILTER EXTRAS", colours::accent);
            drawCaption (g, "SLOPE", 550.0f, 160.0f);
            drawCaption (g, "MODE  (MULTIMODE MODEL)", 720.0f, 160.0f, 200.0f);
            drawSection (g, { 1016, 116, 468, 300 }, "ENGINE", colours::slate);
            drawCaption (g, "VCO CIRCUIT", 1032.0f, 160.0f);
            drawCaption (g, "SOUND QUALITY  (OVERSAMPLING)", 1032.0f, 216.0f, 300.0f);
            drawCaption (g, "VOICE MODE", 1032.0f, 316.0f);
            drawCaption (g, "PITCH BEND (SEMITONES)", 1230.0f, 316.0f, 200.0f);
            drawSection (g, { 52, 428, 1432, 320 }, "VOICE CALIBRATION", colours::slate);
            drawTracked (g, "each voice's tuning and cutoff trim, like the trimmers on the voice cards", { 330.0f, 442.0f, 700.0f, 16.0f },
                         Fonts::jost (9.5f, false, 0.05f), colours::caption, juce::Justification::centredLeft);
            for (int v = 0; v < 8; ++v)
                drawSubSection (g, { 68.0f + 176.0f * static_cast<float> (v), 472.0f, 166.0f, 262.0f }, {}, colours::slate);
        };

        // ================================================================ fixed: header, tabs, performance strip
        const char* tabNames[Augur5Editor::numTabs] = { "MAIN", "MOD", "ARP", "VOICE", "FX", "AUGURY" };
        for (int t = 0; t < Augur5Editor::numTabs; ++t)
        {
            auto* b = fixed.add (std::make_unique<TabButton> (tabNames[t]), { 792 + 57 * t, 36, 54, 28 });
            b->onClick = [this, t] { showTab (t); };
            tabs[static_cast<size_t> (t)] = b;
        }
        prevPreset = fixed.add (std::make_unique<ArrowButton> (false), { 504, 35, 30, 30 });
        nextPreset = fixed.add (std::make_unique<ArrowButton> (true), { 746, 35, 30, 30 });
        undo = fixed.add (std::make_unique<HeaderButton> ("", std::vector<juce::Path> { svgPath ("M5 7 H12 A4 4 0 0 1 12 15 H7"), svgPath ("M7 4 L4 7 L7 10") }),
                          { 1142, 36, 30, 28 });
        redo = fixed.add (std::make_unique<HeaderButton> ("", std::vector<juce::Path> { svgPath ("M13 7 H6 A4 4 0 0 0 6 15 H11"), svgPath ("M11 4 L14 7 L11 10") }),
                          { 1172, 36, 30, 28 });
        {
            // (The preset name opens the browser: no separate button, as in MANTIS-37.)
            juce::Path ring;
            ring.addEllipse (5.0f, 5.0f, 8.0f, 8.0f);
            settings = fixed.add (std::make_unique<HeaderButton> ("", std::vector<juce::Path> {
                                                                          ring, svgPath ("M9 1 V4 M9 14 V17 M1 9 H4 M14 9 H17 M3.3 3.3 L5.4 5.4 M12.6 12.6 L14.7 14.7 M3.3 14.7 L5.4 12.6 M12.6 5.4 L14.7 3.3") }),
                                  { 1202, 36, 30, 28 });
        }
        undo->setTooltip ("Undo");
        redo->setTooltip ("Redo");
        settings->setTooltip ("Settings");

        wheels.push_back (fixed.add (std::make_unique<Wheel> (true, [&proc] { return proc.getBendShown(); }, [&proc] (float v) {
                                         const int value = juce::jlimit (0, 16383, juce::roundToInt ((v + 1.0f) * 8192.0f));
                                         proc.sendUiMidi (0xE0, value & 127, value >> 7);
                                     }),
                                     { 80, 798, 26, 117 }));
        wheels.push_back (fixed.add (std::make_unique<Wheel> (false, [&proc] { return proc.getModWheelShown(); }, [&proc] (float v) {
                                         proc.sendUiMidi (0xB0, 1, juce::jlimit (0, 127, juce::roundToInt (v * 127.0f)));
                                     }),
                                     { 132, 798, 26, 117 }));
        indicators.push_back (fixed.add (std::make_unique<Indicator> ("BEND", colours::accent, [&proc] { return std::abs (proc.getBendShown()); }),
                                         { 196, 813, 48, 28 }));
        indicators.push_back (fixed.add (std::make_unique<Indicator> ("MOD", slate, [&proc] { return proc.getModWheelShown(); }), { 262, 813, 48, 28 }));
        indicators.push_back (fixed.add (std::make_unique<Indicator> ("AT", plum, [&proc] { return proc.getPressureShown(); }), { 196, 863, 48, 28 }));
        indicators.push_back (fixed.add (std::make_unique<Indicator> ("GATE", sage, [&proc] { return proc.isGateShown() ? 1.0f : 0.0f; }),
                                         { 262, 863, 48, 28 }));
        horizon = fixed.add (std::make_unique<Horizon> (proc.getAudioTap(), [&proc] { return proc.getSampleRate(); }), { 388, 778, 760, 186 });
        fixed.knob (P::master_volume, "MASTER", 44, 1212, 838);
        {
            const char* cats[] = { "BASS", "LEAD", "PAD", "PLUCK", "KEYS", "ARP" };
            const char* names[] = { "Bass", "Lead", "Pad", "Pluck", "Keys", "Arp" };
            for (int i = 0; i < 6; ++i)
            {
                const juce::String name (names[i]);
                fixed.add (std::make_unique<ActionButton> (cats[i], i % 2 == 0 ? colours::accent : slate, [this, name] {
                    if (onCategory)
                        onCategory (name);
                }),
                           { 1262 + 62 * (i % 2), 800 + 38 * (i / 2), 56, 28 });
            }
        }
        fixed.add (std::make_unique<ActionButton> ("IDs", colours::caption, [this] {
            showIds = ! showIds;
            repaintAll (*this);
        }),
                   { 1440, 996, 44, 18 })
            ->setTooltip ("Show parameter IDs (for automation)");

        showTab (0);
        setSize (Augur5Editor::designWidth, Augur5Editor::designHeight);
    }

    std::function<juce::String()> presetName;
    std::function<void()> onPresetNameClicked;
    std::function<void (const juce::String&)> onCategory;
    ArrowButton* prevPreset = nullptr;
    ArrowButton* nextPreset = nullptr;
    HeaderButton *undo = nullptr, *redo = nullptr, *settings = nullptr;

    void showEffect (int id)
    {
        showTab (4);
        fxPage->select (id);
    }

    void showTab (int t)
    {
        current = juce::jlimit (0, Augur5Editor::numTabs - 1, t);
        for (size_t i = 0; i < pages.size(); ++i)
            pages[i]->setVisible (static_cast<int> (i) == current);
        fxPage->setVisible (current == 4);
        auguryPage->setVisible (current == 5);
        for (size_t i = 0; i < tabs.size(); ++i)
            tabs[i]->setToggleState (static_cast<int> (i) == current, juce::dontSendNotification);
    }

    void poll()
    {
        for (auto* w : wheels)
            w->poll();
        for (auto* i : indicators)
            i->poll();
        switch (current)
        {
            case 0:
                voices->poll();
                for (auto* e : envelopes)
                    e->poll();
                break;
            case 2: keys->poll(); break;
            case 3:
                for (auto* l : trimLeds)
                    l->poll();
                break;
            case 4: fxPage->poll(); break;
            case 5: auguryPage->poll(); break;
            default: break;
        }
        if (current == 1)
            for (auto* e : envelopes)
                e->poll();
        for (auto* v : views)
            if (v->isShowing())
                v->poll();
        const auto name = presetName ? presetName() : juce::String();
        if (name != shownPreset)
        {
            shownPreset = name;
            repaint (presetBox().toNearestInt());
        }
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (e.eventComponent == this && presetBox().reduced (40.0f, 0.0f).contains (e.position) && onPresetNameClicked)
            onPresetNameClicked();
    }

    void paint (juce::Graphics& g) override
    {
        const float w = static_cast<float> (getWidth()), h = static_cast<float> (getHeight());
        g.setGradientFill (juce::ColourGradient (C (0xffefece7), 0.0f, 0.0f, colours::background, 0.0f, h, false));
        g.fillAll();

        // Header
        drawAugurLogo (g, 52.0f, 22.0f);
        const auto box = presetBox();
        g.setColour (juce::Colours::black.withAlpha (0.04f));
        g.fillRoundedRectangle (box.translated (0.0f, 1.5f), 7.0f);
        g.setColour (juce::Colours::white);
        g.fillRoundedRectangle (box, 7.0f);
        g.setColour (colours::panelBorder);
        g.drawRoundedRectangle (box.reduced (0.5f), 7.0f, 1.0f);
        drawTracked (g, presetName ? presetName() : shownPreset, box.reduced (40.0f, 0.0f), Fonts::jost (14.0f, false, 0.04f), colours::text,
                     juce::Justification::centred);
        drawTracked (g, "PRESET", { box.getX(), 78.0f, box.getWidth(), 11.0f }, Fonts::jost (8.0f, false, 0.3f), colours::caption, juce::Justification::centred);
        // The maker, in the header's corner as on MANTIS-37 and PYTHIA 32.
        g.setColour (colours::panelBorder);
        g.fillRect (1262.0f, 32.0f, 1.0f, 40.0f);
        drawTonalLabLogo (g, { 1276.0f, 24.0f, 208.0f, 52.0f }, TonalLabLogo::horizontal,
                          juce::RectanglePlacement::xRight | juce::RectanglePlacement::yMid);
        g.setColour (colours::panelBorder);
        g.fillRect (52.0f, 100.0f, w - 104.0f, 1.0f);

        // Performance strip (every tab): wheels, activity, the horizon in the middle, output and categories.
        drawSection (g, { 52, 764, 1432, 224 }, "", colours::accent);
        drawCaption (g, "PITCH", 78.0f, 924.0f, 50.0f);
        drawCaption (g, "MOD", 134.0f, 924.0f, 50.0f);
        g.setColour (colours::hairline);
        g.fillRect (372.0f, 784.0f, 1.0f, 184.0f);
        g.fillRect (1164.0f, 784.0f, 1.0f, 184.0f);
        g.fillRect (1390.0f, 784.0f, 1.0f, 184.0f);
        drawCaption (g, "OUTPUT", 1180.0f, 792.0f, 70.0f);
        drawCaption (g, "SOUNDS", 1262.0f, 784.0f, 110.0f);
        drawTracked (g, u8 ("click a sound family to browse it  \xc2\xb7  the horizon is the live spectrum, 30 Hz \xe2\x80\x93 16 kHz"),
                     { 388.0f, 969.0f, 760.0f, 12.0f }, Fonts::jost (8.0f, false, 0.08f), colours::caption, juce::Justification::centred);
        // The maker's plate: TONAL LAB, stacked, as on MANTIS-37.
        drawTonalLabLogo (g, { 1401.0f, 806.0f, 72.0f, 140.0f }, TonalLabLogo::stacked);

        // Footer
        drawTracked (g, "ANALOG SOUL  /  DIGITAL PRECISION", { 52.0f, 996.0f, 400.0f, 18.0f }, Fonts::jost (8.5f, false, 0.3f), colours::caption,
                     juce::Justification::centredLeft);
    }

private:
    static juce::Rectangle<float> presetBox() { return { 500.0f, 28.0f, 280.0f, 44.0f }; }

    void shiftKeys (int delta)
    {
        keys->setLowestOctave (keys->getLowestOctave() + delta);
        pages[2]->repaint();
    }

    APVTS& state;
    ProcessorFxModel fxModel;
    std::vector<std::unique_ptr<juce::Component>> owned;
    std::array<std::unique_ptr<Page>, 4> pages;
    std::unique_ptr<FxPage> fxPage;
    std::unique_ptr<AuguryPage> auguryPage;
    std::array<TabButton*, Augur5Editor::numTabs> tabs {};
    int current = 0;
    std::vector<EnvelopeDisplay*> envelopes;
    std::vector<ParamView*> views;
    std::vector<Wheel*> wheels;
    std::vector<Indicator*> indicators, trimLeds;
    VoiceActivity* voices = nullptr;
    Keys* keys = nullptr;
    Horizon* horizon = nullptr;
    juce::String shownPreset;
};

} // namespace augur5::ui

//==============================================================================

Augur5Editor::Augur5Editor (Augur5Processor& p) : AudioProcessorEditor (p), processor (p)
{
    setLookAndFeel (&lookAndFeel);
    canvas = std::make_unique<augur5::ui::Canvas> (p);
    addAndMakeVisible (*canvas);
    canvas->addMouseListener (this, true);

    auto& presets = processor.getPresets();
    canvas->presetName = [&presets] { return presets.getCurrentName(); };
    canvas->onPresetNameClicked = [this] { browser->open(); };
    canvas->onCategory = [this] (const juce::String& c) { browser->openOnCategory (c); };
    canvas->prevPreset->onClick = [&presets] { presets.previous(); };
    canvas->nextPreset->onClick = [&presets] { presets.next(); };
    canvas->undo->onClick = [this] { processor.getUndoManager().undo(); };
    canvas->redo->onClick = [this] { processor.getUndoManager().redo(); };
    canvas->settings->onClick = [this] { settingsPanel->open(); };

    // Overlay panels inside the scaled canvas: sharp and proportional at every size.
    augur5::ui::PresetBrowser::Actions browserActions;
    browserActions.save = [this] { showSaveDialog(); };
    browserActions.install = [this] { showInstallPackDialog(); };
    browserActions.addFolder = [this] { showAddFolderDialog(); };
    browser = std::make_unique<augur5::ui::PresetBrowser> (presets, std::move (browserActions));

    augur5::ui::SettingsPanel::Actions actions;
    actions.currentScale = [this] { return static_cast<float> (getWidth()) / designWidth; };
    actions.setScale = [this] (float s) {
        processor.setUiScaleChosen (true);
        setScale (s);
    };
    actions.fitScale = [this] {
        processor.setUiScaleChosen (false);
        setScale (fitScale());
    };
    actions.save = [this] { showSaveDialog(); };
    actions.install = [this] { showInstallPackDialog(); };
    actions.addFolder = [this] { showAddFolderDialog(); };
    actions.openFolder = [] {
        auto folder = augur5::PresetManager::getUserFolder();
        folder.createDirectory();
        folder.startAsProcess();
    };
    actions.refreshIds = [this] { augur5::ui::repaintAll (*canvas); };
    settingsPanel = std::make_unique<augur5::ui::SettingsPanel> (processor.getParameters(), std::move (actions));
    for (auto* o : { static_cast<juce::Component*> (browser.get()), static_cast<juce::Component*> (settingsPanel.get()) })
    {
        canvas->addChildComponent (*o);
        o->setBounds (0, 0, designWidth, designHeight);
    }

    setResizable (true, true);
    setResizeLimits (designWidth * 2 / 5, designHeight * 2 / 5, designWidth * 2, designHeight * 2);
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio (static_cast<double> (designWidth) / designHeight);
    // Open as large as the screen allows (the whole panel visible) unless a size was chosen in SETTINGS; never
    // larger than the screen.
    setScale (p.isUiScaleChosen() ? juce::jmin (p.getUiScale(), fitScale()) : fitScale());
    startTimerHz (30);
}

Augur5Editor::~Augur5Editor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

float Augur5Editor::fitScale() const
{
    float scale = 1.0f;
    if (const auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
    {
        const auto area = display->userBounds; // without the taskbar; minus the window's title bar
        scale = juce::jmin (static_cast<float> (area.getWidth() - 24) / designWidth, static_cast<float> (area.getHeight() - 48) / designHeight);
    }
    return juce::jlimit (0.4f, 1.25f, scale);
}

void Augur5Editor::setScale (float scale)
{
    scale = juce::jlimit (0.4f, 2.0f, scale);
    setSize (juce::roundToInt (designWidth * scale), juce::roundToInt (designHeight * scale));
}

void Augur5Editor::showTab (int tab)
{
    canvas->showTab (tab);
}

void Augur5Editor::showEffect (int id)
{
    canvas->showEffect (id);
}

void Augur5Editor::showOverlay (int which)
{
    if (which == 0)
        browser->open();
    else
        settingsPanel->open();
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
    processor.getUndoManager().beginNewTransaction(); // every click-drag gesture is one undo step
}

void Augur5Editor::timerCallback()
{
    canvas->poll();
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
                                                count > 0 ? juce::String (count) + " presets installed. Open PRESETS to find them under the pack's name."
                                                          : juce::String ("This file is not an AUGUR-5 expansion pack."),
                                                "OK", this);
    });
}

void Augur5Editor::showAddFolderDialog()
{
    packChooser = std::make_unique<juce::FileChooser> ("Add a folder of AUGUR-5 presets",
                                                        juce::File::getSpecialLocation (juce::File::userDesktopDirectory));
    packChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories, [this] (const juce::FileChooser& fc) {
        const auto folder = fc.getResult();
        if (! folder.isDirectory())
            return;
        const int count = augur5::PresetManager::importFolder (folder);
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::NoIcon, "Presets folder",
                                                count > 0 ? juce::String (count) + " presets added from \"" + folder.getFileName()
                                                                + "\". Open PRESETS to find them under that name."
                                                          : juce::String ("No AUGUR-5 presets (.augur5) were found in that folder."),
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
