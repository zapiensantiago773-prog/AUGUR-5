#pragma once

#include <cmath>
#include <numbers>

namespace augur
{

// Pipeline-test oscillator for Phase 0 only. The real VCO model arrives in Phase 1.
// Phase accumulator in double to keep pitch exact over long notes.
class TestSine
{
public:
    void prepare (double newSampleRate) noexcept
    {
        sampleRate = newSampleRate;
        setFrequency (frequency);
    }

    void setFrequency (double hz) noexcept
    {
        frequency = hz;
        increment = frequency / sampleRate;
    }

    void resetPhase() noexcept { phase = 0.0; }

    float process() noexcept
    {
        const double y = std::sin (2.0 * std::numbers::pi * phase);
        phase += increment;
        if (phase >= 1.0)
            phase -= 1.0;
        return static_cast<float> (y);
    }

private:
    double sampleRate = 44100.0;
    double frequency = 440.0;
    double increment = 440.0 / 44100.0;
    double phase = 0.0;
};

} // namespace augur
