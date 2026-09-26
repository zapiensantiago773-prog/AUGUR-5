#pragma once

#include "Util/Random.h"

#include <cmath>

namespace augur
{

// Free-running LFO (triangle, saw, square, sample & hold). Square and S&H steps go through a
// ~2 ms one-pole so hard modulation never clicks, like the slew of the original CV buffers.
class Lfo
{
public:
    enum class Wave
    {
        Triangle,
        Saw,
        Square,
        SampleHold
    };

    void prepare (double newSampleRate, std::uint64_t seed) noexcept
    {
        sampleRate = newSampleRate;
        rng.setSeed (seed);
        phase = rng.nextFloat();
        held = rng.nextBipolar();
        smoothed = 0.0f;
        slewCoeff = static_cast<float> (1.0 - std::exp (-1.0 / (0.002 * newSampleRate)));
    }

    void setPhase (double p) noexcept { phase = p - std::floor (p); }
    double getPhase() const noexcept { return phase; }

    float process (double rateHz, Wave wave) noexcept
    {
        phase += rateHz / sampleRate;
        if (phase >= 1.0)
        {
            phase -= std::floor (phase);
            held = rng.nextBipolar();
        }

        const float ph = static_cast<float> (phase);
        switch (wave)
        {
            case Wave::Triangle: return 1.0f - 4.0f * std::abs (ph - 0.5f);
            case Wave::Saw:      return 2.0f * ph - 1.0f;
            case Wave::Square:   return slew (ph < 0.5f ? 1.0f : -1.0f);
            case Wave::SampleHold: return slew (held);
        }
        return 0.0f;
    }

private:
    float slew (float target) noexcept
    {
        smoothed += (target - smoothed) * slewCoeff;
        return smoothed;
    }

    Random rng;
    double sampleRate = 48000.0;
    double phase = 0.0;
    float held = 0.0f;
    float smoothed = 0.0f;
    float slewCoeff = 1.0f;
};

} // namespace augur
