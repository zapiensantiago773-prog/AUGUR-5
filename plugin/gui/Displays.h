#pragma once

#include "Widgets.h"

#include <array>
#include <functional>

namespace augur5::ui
{

// Shared base: repaints when any of the watched parameters moves (polled by the editor's timer).
class ParamView : public juce::Component
{
public:
    ParamView (APVTS& state, std::initializer_list<juce::String> ids);
    virtual void poll();

protected:
    float value (size_t i) const { return i < values.size() && values[i] != nullptr ? values[i]->load() : 0.0f; }
    std::vector<std::atomic<float>*> values;
    std::vector<float> last;
};

// Filter response (20 Hz - 20 kHz): the model's poles at the current cutoff / resonance, slope, mode and the HPF.
class FilterCurve final : public ParamView
{
public:
    FilterCurve (APVTS& state, bool withHpf);
    void paint (juce::Graphics&) override;

private:
    bool withHpf;
};

// One and a half cycles of LFO 2's shape, with a dot running at its rate.
class LfoShape final : public ParamView
{
public:
    explicit LfoShape (APVTS& state);
    void paint (juce::Graphics&) override;
    void poll() override;

private:
    double phase = 0.0;
};

// Every matrix slot as a line from its source (top) to its destination (bottom), thickness = amount.
class MatrixMap final : public ParamView
{
public:
    explicit MatrixMap (APVTS& state);
    void paint (juce::Graphics&) override;
};

// The arpeggiator's pattern over a C major triad: order, octaves, gate length and swing, with a running step.
class ArpPattern final : public ParamView
{
public:
    ArpPattern (APVTS& state, std::function<double()> tempo);
    void paint (juce::Graphics&) override;
    void poll() override;

private:
    std::function<double()> tempo;
    double beat = 0.0;
    int shownStep = -1;
};

// Playable keyboard (sends through the processor's lock-free GUI MIDI queue) that also shows the keys held from MIDI.
class Keys final : public juce::Component
{
public:
    Keys (std::function<void (int note, float velocity, bool on)> send, std::function<bool (int note)> isHeld);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void poll();
    void setLowestOctave (int octave); // C of this octave (MIDI octave numbering, C3 = 60) at the left
    int getLowestOctave() const noexcept { return lowestOctave; }
    static constexpr int numOctaves = 5;

private:
    int noteAt (juce::Point<float> p, float& velocity) const;
    juce::Rectangle<float> keyBounds (int note) const;
    static bool isBlack (int note) noexcept;
    int firstNote() const noexcept { return (lowestOctave + 2) * 12; }

    std::function<void (int, float, bool)> send;
    std::function<bool (int)> isHeld;
    int lowestOctave = 1, playing = -1;
    std::array<bool, 128> shownHeld {};
};

} // namespace augur5::ui
