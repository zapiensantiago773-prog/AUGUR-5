#pragma once

#include "Util/Random.h"

#include <cmath>

namespace augur
{

// LFO 2: one per voice. Retriggered from phase 0 on every note (poly) or started from the engine's
// free-running phase (all voices move together, but each can still be rate-modulated on its own).
// Square and random steps go through a ~1.5 ms one-pole so hard modulation never clicks.
class PolyLfo
{
public:
    enum class Wave
    {
        Sine,
        Triangle,
        SawUp,
        SawDown,
        Square,
        SampleHold,
        SmoothRandom
    };

    void prepare (double sampleRate, std::uint64_t seed) noexcept
    {
        rng.setSeed (seed);
        slewCoeff = static_cast<float> (1.0 - std::exp (-1.0 / (0.0015 * sampleRate)));
        restart (0.0);
    }

    void restart (double startPhase) noexcept
    {
        phase = startPhase - std::floor (startPhase);
        previous = held;
        held = rng.nextBipolar();
    }

    // inc: cycles per sample.
    float next (double inc, Wave wave) noexcept
    {
        phase += inc;
        if (phase >= 1.0)
        {
            phase -= std::floor (phase);
            previous = held;
            held = rng.nextBipolar();
        }
        const float ph = static_cast<float> (phase);
        switch (wave)
        {
            case Wave::Sine:
            {
                // Cubic-shaped triangle: within 2 % of a sine, plenty for an LFO.
                const float x = ph < 0.5f ? ph * 4.0f - 1.0f : 3.0f - ph * 4.0f; // triangle -1..1
                return x * (1.5f - 0.5f * x * x);
            }
            case Wave::Triangle:     return 1.0f - 4.0f * std::abs (ph - 0.5f);
            case Wave::SawUp:        return 2.0f * ph - 1.0f;
            case Wave::SawDown:      return 1.0f - 2.0f * ph;
            case Wave::Square:       return slew (ph < 0.5f ? 1.0f : -1.0f);
            case Wave::SampleHold:   return slew (held);
            case Wave::SmoothRandom:
            {
                const float t = ph * ph * (3.0f - 2.0f * ph); // smoothstep between two random points
                return previous + (held - previous) * t;
            }
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
    double phase = 0.0;
    float held = 0.0f, previous = 0.0f, smoothed = 0.0f;
    float slewCoeff = 1.0f;
};

} // namespace augur
