#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace augur::fastmath
{

inline constexpr float pi = 3.14159265358979f;

// Padé [7/6] tanh. Max error ~1e-6 for |x| < 4.9, clamped outside. Used by every analog nonlinearity.
inline float tanh (float x) noexcept
{
    x = std::clamp (x, -4.97f, 4.97f);
    const float x2 = x * x;
    const float num = x * (135135.0f + x2 * (17325.0f + x2 * (378.0f + x2)));
    const float den = 135135.0f + x2 * (62370.0f + x2 * (3150.0f + 28.0f * x2));
    return std::clamp (num / den, -1.0f, 1.0f);
}

// tanh(x) / x, the secant gain used to linearise nonlinear stages inside the implicit filter solver.
// Same Padé approximant with the x factored out: one division, no special case near 0.
inline float tanhRatio (float x) noexcept
{
    const float ax = std::abs (x);
    if (ax > 4.97f)
        return 1.0f / ax;
    const float x2 = x * x;
    return (135135.0f + x2 * (17325.0f + x2 * (378.0f + x2))) / (135135.0f + x2 * (62370.0f + x2 * (3150.0f + 28.0f * x2)));
}

// Padé tan, accurate for 0 <= x <= 1.45 (cutoff prewarping up to 0.46 * fs).
inline float tan (float x) noexcept
{
    x = std::clamp (x, 0.0f, 1.45f);
    const float x2 = x * x;
    const float num = x * (135135.0f - x2 * (17325.0f - x2 * (378.0f - x2)));
    const float den = 135135.0f - x2 * (62370.0f - x2 * (3150.0f - 28.0f * x2));
    return num / den;
}

// 2^x with ~1e-7 relative error (0.0002 cents): centred 6th-order polynomial + exponent bits.
inline float exp2 (float x) noexcept
{
    x = x < -126.0f ? -126.0f : (x > 126.0f ? 126.0f : x);
    // floor() without a library call: truncate, then step down for negative non-integers.
    std::int32_t i = static_cast<std::int32_t> (x);
    i -= (static_cast<float> (i) > x) ? 1 : 0;
    const float g = x - static_cast<float> (i) - 0.5f;
    const float p = 1.0f + g * (0.69314718f + g * (0.24022651f + g * (0.05550411f + g * (0.00961813f + g * (0.00133336f + g * 0.00015404f)))));
    const std::int32_t bits = (i + 127) << 23;
    float scale;
    std::memcpy (&scale, &bits, sizeof (scale));
    return p * scale * 1.41421356f;
}

// log2(x) for x > 0, ~1e-7 absolute error: exponent bits + atanh series on the mantissa in [0.707, 1.414).
inline float log2 (float x) noexcept
{
    x = x < 1.0e-30f ? 1.0e-30f : x;
    std::int32_t bits;
    std::memcpy (&bits, &x, sizeof (bits));
    std::int32_t e = ((bits >> 23) & 0xff) - 127;
    bits = (bits & 0x007fffff) | 0x3f800000;
    float m;
    std::memcpy (&m, &bits, sizeof (m));
    if (m > 1.41421356f)
    {
        m *= 0.5f;
        ++e;
    }
    const float t = (m - 1.0f) / (m + 1.0f), t2 = t * t;
    const float ln = 2.0f * t * (1.0f + t2 * (1.0f / 3.0f + t2 * (0.2f + t2 * (1.0f / 7.0f + t2 * (1.0f / 9.0f)))));
    return static_cast<float> (e) + ln * 1.44269504f;
}

inline float semitonesToHz (float midiPitch) noexcept
{
    return 440.0f * exp2 ((midiPitch - 69.0f) * (1.0f / 12.0f));
}

} // namespace augur::fastmath
