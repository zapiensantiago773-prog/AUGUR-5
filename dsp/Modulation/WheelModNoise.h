#pragma once

#include "Util/Random.h"

#include <array>
#include <cmath>
#include <cstdint>

namespace augur
{

// The WHEEL-MOD noise source: the Prophet-5 mixes its LFO with pink noise (service manual 2-3). White noise whose
// density does not depend on the sample rate feeds three one-poles a decade apart (15 / 150 / 1500 Hz) with
// gains^2 ~ 1 / corner: -3 dB/octave within ~2 dB from ~10 Hz to ~2 kHz, each band a third of the power,
// 0.4 rms in all at any sample rate.
class WheelModNoise
{
public:
    void prepare (double sampleRate, std::uint64_t seed) noexcept
    {
        rng.setSeed (seed);
        state.fill (0.0f);
        for (size_t k = 0; k < 3; ++k)
            coeff[k] = static_cast<float> (1.0 - std::exp (-2.0 * 3.14159265358979 * corners[k] / sampleRate));
        // Uniform white noise has a variance of 1/3 per sample: scale it so its density is the same at any rate.
        scale = static_cast<float> (std::sqrt (sampleRate / 48000.0));
    }

    float next() noexcept
    {
        const float white = rng.nextBipolar() * scale;
        float out = 0.0f;
        for (size_t k = 0; k < 3; ++k)
        {
            state[k] += (white * gains[k] - state[k]) * coeff[k];
            out += state[k];
        }
        return out;
    }

private:
    static constexpr double corners[3] { 15.0, 150.0, 1500.0 };
    // gain_k^2 ~ 1 / corner_k, scaled for 0.4 rms in all (measured: the three bands share their input, so they add
    // with some correlation; test "WHEEL-MOD noise").
    static constexpr float gains[3] { 9.18f, 2.905f, 0.918f };

    Random rng;
    std::array<float, 3> state {}, coeff {};
    float scale = 1.0f;
};

} // namespace augur
