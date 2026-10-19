#pragma once

#include "Util/Random.h"

#include <cmath>

namespace augur
{

// Free-running LFO (triangle, saw, square, sample & hold, and the Prophet's combinations of the first three).
// Square and S&H steps go through a ~2 ms one-pole so hard modulation never clicks, like the slew of the
// original CV buffers.
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

    // LFO WAVE as stored (0..7): the four single waves, then the Prophet's combinations of SAW / TRI / SQUARE.
    static constexpr int numWaveSettings = 8;
    static constexpr int waveMask (int setting) noexcept
    {
        constexpr int masks[numWaveSettings] { tri, saw, sqr, sh, saw | tri, saw | sqr, tri | sqr, saw | tri | sqr };
        return masks[setting < 0 ? 0 : (setting >= numWaveSettings ? numWaveSettings - 1 : setting)];
    }
    static constexpr int tri = 1, saw = 2, sqr = 4, sh = 8;

    struct Shapes
    {
        float centred; // the selected waves summed, each centred on zero (the matrix's LFO source)
        float prophet; // the same with the Prophet-5's polarities: triangle bipolar, saw and square 0..1 (WHEEL-MOD)
    };

    // One sample of every selected wave. A single wave gives exactly what process() gives.
    Shapes processShapes (double rateHz, int setting) noexcept
    {
        const int mask = waveMask (setting);
        if (mask == sh)
        {
            const float v = process (rateHz, Wave::SampleHold);
            return { v, v };
        }
        advance (rateHz);
        const float ph = static_cast<float> (phase);
        Shapes out { 0.0f, 0.0f };
        if ((mask & tri) != 0)
        {
            const float t = 1.0f - 4.0f * std::abs (ph - 0.5f);
            out.centred += t;
            out.prophet += t;
        }
        if ((mask & saw) != 0)
        {
            out.centred += 2.0f * ph - 1.0f;
            out.prophet += ph;
        }
        if ((mask & sqr) != 0)
        {
            const float q = slew (ph < 0.5f ? 1.0f : -1.0f);
            out.centred += q;
            out.prophet += 0.5f * (q + 1.0f);
        }
        return out;
    }

    float process (double rateHz, Wave wave) noexcept
    {
        advance (rateHz);
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
    void advance (double rateHz) noexcept
    {
        phase += rateHz / sampleRate;
        if (phase >= 1.0)
        {
            phase -= std::floor (phase);
            held = rng.nextBipolar();
        }
    }

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
