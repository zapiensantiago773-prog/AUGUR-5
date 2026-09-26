#pragma once

#include "Util/Random.h"

#include <array>
#include <cmath>

namespace augur
{

// Slow analog drift: approximately 1/f noise (three AR(1) sections at 0.03, 0.25 and 2 Hz with 1/f
// weighting) plus a slow leaky random walk (tau = 30 s, thermal wander). No periodic LFO anywhere.
// Output is roughly unit variance; the caller scales it to cents (or semitones of cutoff).
class Drift
{
public:
    void prepare (double updateRateHz, std::uint64_t seed) noexcept
    {
        rng.setSeed (seed);
        constexpr std::array<double, 3> corners { 0.03, 0.25, 2.0 };
        for (size_t i = 0; i < corners.size(); ++i)
        {
            a[i] = static_cast<float> (std::exp (-2.0 * 3.14159265358979 * corners[i] / updateRateHz));
            b[i] = std::sqrt (1.0f - a[i] * a[i]);
        }
        walkA = static_cast<float> (std::exp (-1.0 / (30.0 * updateRateHz)));
        walkB = std::sqrt (1.0f - walkA * walkA);

        // Start from a random point of the stationary distribution instead of 0.
        for (auto& s : state)
            s = rng.nextGaussian();
        walk = rng.nextGaussian();
    }

    float next() noexcept
    {
        constexpr std::array<float, 3> weights { 1.0f, 0.55f, 0.3f };
        float sum = 0.0f;
        for (size_t i = 0; i < state.size(); ++i)
        {
            state[i] = a[i] * state[i] + b[i] * rng.nextGaussian();
            sum += weights[i] * state[i];
        }
        walk = walkA * walk + walkB * rng.nextGaussian();
        constexpr float norm = 0.62f; // 1 / sqrt(1 + 0.55^2 + 0.3^2 + 1.2^2)
        return (sum + 1.2f * walk) * norm;
    }

private:
    Random rng;
    std::array<float, 3> a {}, b {}, state {};
    float walkA = 1.0f, walkB = 0.0f, walk = 0.0f;
};

} // namespace augur
