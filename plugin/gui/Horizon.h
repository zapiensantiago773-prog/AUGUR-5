#pragma once

#include <juce_dsp/juce_dsp.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "../AudioTap.h"

#include <array>

namespace augur5::ui
{

// "AUGURY": the animated frequency horizon of the performance strip, white version.
//
// The output's spectrum (Hann-windowed FFT, log axis 30 Hz - 16 kHz, -72..0 dB, fast-attack / slow-release
// ballistics) is stored as a ridge every other frame; the ridge history is drawn back to front in perspective
// with fine ink lines on paper, so the sound flows into the distance under a pale sun that breathes with the
// level. A flock of birds crosses the sky (an augur read the flight of birds); they beat their wings faster
// when the instrument plays. With no audio an idle landscape keeps the panel alive.
class Horizon final : public juce::Component, private juce::Timer
{
public:
    Horizon (AudioTap& tap, std::function<double()> sampleRate);
    ~Horizon() override;

    void paint (juce::Graphics&) override;

    static constexpr int points = 112; // horizontal resolution of each ridge
    static constexpr int layers = 18;  // ridges in depth

private:
    void timerCallback() override;
    void analyse();
    void makeIdleFrame (float time, float* out) const;
    void pushLayer (const float* frame, float level);
    void paintBirds (juce::Graphics&, juce::Rectangle<float> sky, float level) const;
    void paintScale (juce::Graphics&, juce::Rectangle<float> b) const;

    AudioTap& tap;
    std::function<double()> sampleRate;

    static constexpr int fftOrder = 11, fftSize = 1 << fftOrder;
    juce::dsp::FFT fft { fftOrder };
    juce::dsp::WindowingFunction<float> window { static_cast<size_t> (fftSize), juce::dsp::WindowingFunction<float>::hann };
    std::array<float, static_cast<size_t> (fftSize)> ring {};
    int ringPos = 0;
    std::array<float, static_cast<size_t> (fftSize) * 2> fftData {};
    std::array<float, 4096> pullBuffer {};

    std::array<float, static_cast<size_t> (points)> smoothed {};
    std::array<std::array<float, static_cast<size_t> (points)>, static_cast<size_t> (layers)> history {};
    std::array<float, static_cast<size_t> (layers)> historyLevel {};
    int historyHead = 0;

    float rms = 0.0f, idleMix = 1.0f, time = 0.0f, flap = 0.0f;
    int tick = 0, silentFrames = 1000;
};

} // namespace augur5::ui
