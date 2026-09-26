#pragma once

#include <cstdint>

namespace augur
{

// SplitMix64: tiny, fast, seedable and statistically solid for audio noise and analog variation.
// Same seed -> same sequence on every platform, which keeps voices and presets reproducible.
class Random
{
public:
    explicit Random (std::uint64_t seed = 0x9E3779B97F4A7C15ull) noexcept { setSeed (seed); }

    void setSeed (std::uint64_t seed) noexcept { state = seed; }

    std::uint64_t nextU64() noexcept
    {
        std::uint64_t z = (state += 0x9E3779B97F4A7C15ull);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        return z ^ (z >> 31);
    }

    // [0, 1)
    float nextFloat() noexcept { return static_cast<float> (nextU64() >> 40) * (1.0f / 16777216.0f); }

    // [-1, 1)
    float nextBipolar() noexcept { return nextFloat() * 2.0f - 1.0f; }

    // Approximately unit-variance Gaussian (Irwin-Hall, 4 terms): cheap and bounded, good enough for analog spread.
    float nextGaussian() noexcept
    {
        const float sum = nextFloat() + nextFloat() + nextFloat() + nextFloat();
        return (sum - 2.0f) * 1.7320508f;
    }

private:
    std::uint64_t state = 0;
};

// Derives independent, stable seeds for sub-components (voice N, oscillator M, ...).
inline std::uint64_t deriveSeed (std::uint64_t base, std::uint64_t salt) noexcept
{
    Random r (base ^ (salt * 0xD1B54A32D192ED03ull));
    return r.nextU64();
}

} // namespace augur
