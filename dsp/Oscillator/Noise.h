#pragma once

#include "Util/Random.h"

namespace augur
{

// Analog-style noise: white with a gentle pink tilt (Paul Kellet's economy pink filter blended in),
// like a reverse-biased junction followed by a band-limited amplifier.
class NoiseSource
{
public:
    void seed (std::uint64_t s) noexcept
    {
        rng.setSeed (s);
        b0 = b1 = b2 = 0.0f;
    }

    float white() noexcept { return rng.nextBipolar(); }

    float analog() noexcept
    {
        const float w = white();
        b0 = 0.99765f * b0 + w * 0.0990460f;
        b1 = 0.96300f * b1 + w * 0.2965164f;
        b2 = 0.57000f * b2 + w * 1.0526913f;
        const float pink = (b0 + b1 + b2 + w * 0.1848f) * 0.25f;
        return 0.55f * w + 0.45f * pink;
    }

private:
    Random rng;
    float b0 = 0.0f, b1 = 0.0f, b2 = 0.0f;
};

} // namespace augur
