#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "Theme.h"

#include <array>
#include <functional>

namespace augur5::ui
{

using APVTS = juce::AudioProcessorValueTreeState;

//==============================================================================
// Rotary knob: body diameter = size, bounds (size + 60) x (size + 42). The label sits under it; while the mouse
// is over the knob or it is being turned, the label shows the exact value ("1.20 kHz", "45 %", "-6.0 dB").
// Drag vertically; Shift = fine; double-click = default. `colour` = value arc (bipolar ranges fill from the centre).
class Knob final : public juce::Component, private juce::Slider::Listener
{
public:
    Knob (APVTS& state, const juce::String& paramId, const juce::String& label, int size, juce::Colour colour = colours::accent);
    ~Knob() override;

    static constexpr int labelMargin = 18;
    static juce::Rectangle<int> boundsFor (int size) { return { -labelMargin, 0, size + 24 + 2 * labelMargin, size + 42 }; }
    bool hitTest (int x, int y) override { return slider.getBounds().contains (x, y); }
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }

private:
    void sliderValueChanged (juce::Slider*) override { repaint(); }
    void sliderDragStarted (juce::Slider*) override { repaint(); }
    void sliderDragEnded (juce::Slider*) override { repaint(); }

    struct FineSlider final : juce::Slider
    {
        void mouseDown (const juce::MouseEvent& e) override;
    };
    FineSlider slider;
    juce::String label, paramId;
    juce::RangedAudioParameter* param = nullptr;
    int size;
    juce::Colour colour;
    std::unique_ptr<APVTS::SliderAttachment> attachment;
};

//==============================================================================
// Rounded switch with an LED dot, lit in its section colour when on.
class LedToggle : public juce::Button
{
public:
    explicit LedToggle (const juce::String& label, juce::Colour colour = colours::accent);
    void paintButton (juce::Graphics&, bool over, bool down) override;
    juce::String idText; // shown instead of the label in IDs mode

protected:
    juce::Colour colour;
};

class ParamToggle final : public LedToggle
{
public:
    ParamToggle (APVTS& state, const juce::String& paramId, const juce::String& label, juce::Colour colour = colours::accent);

private:
    std::unique_ptr<APVTS::ButtonAttachment> attachment;
};

// Momentary action button.
class ActionButton final : public LedToggle
{
public:
    ActionButton (const juce::String& label, juce::Colour colour, std::function<void()> action);
};

// Tab of the header (MAIN, MOD, ARP, VOICE, FX, AUGURY): ink text, accent underline when selected.
class TabButton final : public juce::Button
{
public:
    explicit TabButton (const juce::String& text) : juce::Button (text) {}
    void paintButton (juce::Graphics&, bool over, bool down) override;
};

// Segment of a compact choice row (OFF | HALF | FULL, -2 .. +2): centred text, tinted when selected.
class SegmentButton final : public juce::Button
{
public:
    explicit SegmentButton (const juce::String& text, juce::Colour c = colours::accent) : juce::Button (text), colour (c) {}
    void paintButton (juce::Graphics&, bool over, bool down) override;
    juce::String idText;

private:
    juce::Colour colour;
};

//==============================================================================
enum class WaveIcon
{
    Saw,
    Pulse,
    Triangle,
    Square,
    SampleHold
};

class WaveButton final : public juce::Button
{
public:
    explicit WaveButton (WaveIcon icon, juce::Colour colour = colours::accent);
    void paintButton (juce::Graphics&, bool over, bool down) override;

private:
    juce::Path path;
    juce::Colour colour;
};

class ParamWaveButton final : public juce::Component
{
public:
    ParamWaveButton (APVTS& state, const juce::String& paramId, WaveIcon icon, juce::Colour colour = colours::accent);
    void resized() override { button.setBounds (getLocalBounds()); }

private:
    WaveButton button;
    std::unique_ptr<APVTS::ButtonAttachment> attachment;
};

//==============================================================================
// Radio group driving a choice parameter.
class ChoiceGroup final : public juce::Component
{
public:
    ChoiceGroup (APVTS& state, const juce::String& paramId, std::vector<std::unique_ptr<juce::Button>> buttons);
    juce::Button& getButton (int i) { return *buttons[static_cast<size_t> (i)]; }
    int getNumButtons() const { return static_cast<int> (buttons.size()); }

private:
    void update (float value);
    std::vector<std::unique_ptr<juce::Button>> buttons;
    std::unique_ptr<juce::ParameterAttachment> attachment;
};

// Drop-down bound to a choice (or small integer) parameter.
class ParamChoiceBox final : public juce::Component
{
public:
    ParamChoiceBox (APVTS& state, const juce::String& paramId);
    void resized() override { box.setBounds (getLocalBounds()); }

private:
    juce::ComboBox box;
    std::unique_ptr<APVTS::ComboBoxAttachment> attachment;
};

//==============================================================================
// Envelope curve drawn from the actual A/D/S/R values (RC shapes like the engine).
class EnvelopeDisplay final : public juce::Component
{
public:
    EnvelopeDisplay (APVTS& state, const juce::String& a, const juce::String& d, const juce::String& s, const juce::String& r,
                     juce::Colour colour = colours::accent);
    void paint (juce::Graphics&) override;
    void poll();

private:
    std::array<juce::RangedAudioParameter*, 4> params {};
    std::array<float, 4> last { -1.0f, -1.0f, -1.0f, -1.0f };
    juce::Colour colour;
};

class VoiceActivity final : public juce::Component
{
public:
    VoiceActivity (APVTS& state, std::function<float (int)> levelOf);
    void paint (juce::Graphics&) override;
    void poll();

private:
    std::atomic<float>* voiceCount = nullptr;
    std::function<float (int)> levelOf;
    std::array<float, 16> shown {};
    int shownCount = 0;
};

class LevelFader final : public juce::Component
{
public:
    LevelFader (APVTS& state, const juce::String& paramId, const juce::String& label);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    juce::Slider slider;
    juce::String label;
    std::unique_ptr<APVTS::SliderAttachment> attachment;
};

class VoiceCounter final : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit VoiceCounter (APVTS& state);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct StepButton final : juce::Button
    {
        explicit StepButton (const juce::String& t) : juce::Button (t) {}
        void paintButton (juce::Graphics&, bool over, bool down) override;
    };
    StepButton minus { "-" }, plus { "+" };
    std::unique_ptr<juce::ParameterAttachment> attachment;
    int value = 5;
};

// One slot of the modulation matrix: number, SOURCE -> DESTINATION, bipolar AMOUNT with its value.
class MatrixRow final : public juce::Component
{
public:
    MatrixRow (APVTS& state, int slot);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void refreshActive();

    int slot;
    bool active = true;
    juce::ComboBox source, dest;
    juce::Slider amount;
    std::unique_ptr<APVTS::ComboBoxAttachment> srcAttachment, dstAttachment;
    std::unique_ptr<APVTS::SliderAttachment> amtAttachment;
};

//==============================================================================
// Pitch / mod wheel: shows the incoming MIDI and can be dragged (the pitch wheel springs back to the centre).
class Wheel final : public juce::Component
{
public:
    Wheel (bool pitch, std::function<float()> get, std::function<void (float)> set);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void poll();

private:
    bool pitch;
    std::function<float()> get;
    std::function<void (float)> set;
    float shown = 0.0f, dragStart = 0.0f;
};

// BEND / MOD / AT / GATE activity LED.
class Indicator final : public juce::Component
{
public:
    Indicator (const juce::String& label, juce::Colour colour, std::function<float()> level);
    void paint (juce::Graphics&) override;
    void poll();

private:
    juce::String label;
    juce::Colour colour;
    std::function<float()> level;
    float shown = 0.0f;
};

//==============================================================================
class HeaderButton final : public juce::Button
{
public:
    HeaderButton (const juce::String& label, std::vector<juce::Path> icon);
    void paintButton (juce::Graphics&, bool over, bool down) override;

private:
    std::vector<juce::Path> icon;
};

class ArrowButton final : public juce::Button
{
public:
    explicit ArrowButton (bool pointsRight);
    void paintButton (juce::Graphics&, bool over, bool down) override;

private:
    juce::Path path;
};

} // namespace augur5::ui
