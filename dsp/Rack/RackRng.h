#pragma once

#include <cstdint>

namespace augur::rack
{

// PCG32 (O'Neill): small, fast, reproducible on every platform. Used for analog tolerances and noise.
class Rng
{
public:
    explicit Rng (std::uint64_t seed = 0x853c49e6748fea9bull) noexcept { seedWith (seed); }

    void seedWith (std::uint64_t seed) noexcept
    {
        state = 0;
        next();
        state += seed;
        next();
    }

    std::uint32_t next() noexcept
    {
        const std::uint64_t old = state;
        state = old * 6364136223846793005ull + 1442695040888963407ull;
        const auto xorshifted = static_cast<std::uint32_t> (((old >> 18u) ^ old) >> 27u);
        const auto rot = static_cast<std::uint32_t> (old >> 59u);
        return (xorshifted >> rot) | (xorshifted << ((32u - rot) & 31u));
    }

    double uniform() noexcept { return next() * (1.0 / 4294967296.0); } // [0, 1)
    double bipolar() noexcept { return uniform() * 2.0 - 1.0; }          // [-1, 1)

    // Unit-variance, bounded (sum of 6 uniforms): analog spreads never produce absurd outliers.
    double gauss() noexcept
    {
        double s = 0.0;
        for (int i = 0; i < 6; ++i)
            s += uniform();
        return (s - 3.0) * 1.41421356237309505;
    }

private:
    std::uint64_t state = 0;
};

inline std::uint64_t mixSeed (std::uint64_t a, std::uint64_t b) noexcept
{
    std::uint64_t z = a ^ (b + 0x9e3779b97f4a7c15ull + (a << 6) + (a >> 2));
    z = (z ^ (z >> 33)) * 0xff51afd7ed558ccdull;
    z = (z ^ (z >> 33)) * 0xc4ceb9fe1a85ec53ull;
    return z ^ (z >> 33);
}

} // namespace augur::rack
