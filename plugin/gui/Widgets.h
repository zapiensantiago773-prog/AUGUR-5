#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "Theme.h"

#include <functional>

namespace augur5::ui
{

using APVTS = juce::AudioProcessorValueTreeState;

//==============================================================================
// Rotary knob with ticks and label. Bounds: (size + 24) x (size + 42): room below for the enlarged label.
// Drag vertically; Shift = fine; double-click = default; value bubble while dragging.
class Knob final : public juce::Component
{
public:
    Knob (APVTS& state, const juce::String& paramId, const juce::String& label, int size);

    // Labels may be wider than the knob: the component carries a side margin that ignores clicks.
    static constexpr int labelMargin = 18;
    static juce::Rectangle<int> boundsFor (int size) { return { -labelMargin, 0, size + 24 + 2 * labelMargin, size + 42 }; }
    bool hitTest (int x, int y) override { return slider.getBounds().contains (x, y); }
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct FineSlider final : juce::Slider
    {
        void mouseDown (const juce::MouseEvent& e) override;
        void mouseDrag (const juce::MouseEvent& e) override;
    };

    FineSlider slider;
    juce::String label;
    int size;
    std::unique_ptr<APVTS::SliderAttachment> attachment;
};

//==============================================================================
// Rounded LED toggle (mockup "Toggle"): 28 px high, LED + label, orange when on.
class LedToggle : public juce::Button
{
public:
    explicit LedToggle (const juce::String& label);
    void paintButton (juce::Graphics&, bool over, bool down) override;
};

// LED toggle bound to a boolean parameter.
class ParamToggle final : public LedToggle
{
public:
    ParamToggle (APVTS& state, const juce::String& paramId, const juce::String& label);

private:
    std::unique_ptr<APVTS::ButtonAttachment> attachment;
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

// 34 x 26 waveform button.
class WaveButton final : public juce::Button
{
public:
    explicit WaveButton (WaveIcon icon);
    void paintButton (juce::Graphics&, bool over, bool down) override;

private:
    juce::Path path;
};

class ParamWaveButton final : public juce::Component
{
public:
    ParamWaveButton (APVTS& state, const juce::String& paramId, WaveIcon icon);
    void resized() override { button.setBounds (getLocalBounds()); }

private:
    WaveButton button;
    std::unique_ptr<APVTS::ButtonAttachment> attachment;
};

//==============================================================================
// Radio group driving a choice parameter (filter model, key track, LFO wave).
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

//==============================================================================
// Drop-down bound to a choice (or small integer) parameter: for selectors with more options than
// fit as LED buttons (filter model, arp rate, LFO 2 wave, quality...).
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
// Small square LED switch in the effect headers.
class FxLed final : public juce::Button
{
public:
    FxLed (APVTS& state, const juce::String& paramId);
    void paintButton (juce::Graphics&, bool over, bool down) override;

private:
    std::unique_ptr<APVTS::ButtonAttachment> attachment;
};

//==============================================================================
// Envelope curve drawn from the actual A/D/S/R values (RC shapes like the engine).
class EnvelopeDisplay final : public juce::Component
{
public:
    EnvelopeDisplay (APVTS& state, const juce::String& a, const juce::String& d, const juce::String& s, const juce::String& r);
    void paint (juce::Graphics&) override;
    void poll(); // repaints when a value changed

private:
    std::array<juce::RangedAudioParameter*, 4> params {};
    std::array<float, 4> last { -1.0f, -1.0f, -1.0f, -1.0f };
};

//==============================================================================
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

//==============================================================================
class LevelFader final : public juce::Component
{
public:
    explicit LevelFader (APVTS& state);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    juce::Slider slider;
    std::unique_ptr<APVTS::SliderAttachment> attachment;
};

//==============================================================================
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

//==============================================================================
class MatrixRow final : public juce::Component
{
public:
    MatrixRow (APVTS& state, int slot);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    int slot;
    juce::ComboBox source, dest;
    juce::Slider amount;
    std::unique_ptr<APVTS::ComboBoxAttachment> srcAttachment, dstAttachment;
    std::unique_ptr<APVTS::SliderAttachment> amtAttachment;
};

//==============================================================================
// Header icon button (UNDO / REDO / BROWSER / SETTINGS): 18 px icon over a 9 px label.
class HeaderButton final : public juce::Button
{
public:
    HeaderButton (const juce::String& label, std::vector<juce::Path> icon, bool fillFirst = false);
    void paintButton (juce::Graphics&, bool over, bool down) override;

private:
    std::vector<juce::Path> icon;
};

// Chevron arrow for the preset box.
class ArrowButton final : public juce::Button
{
public:
    explicit ArrowButton (bool pointsRight);
    void paintButton (juce::Graphics&, bool over, bool down) override;

private:
    juce::Path path;
};

} // namespace augur5::ui
