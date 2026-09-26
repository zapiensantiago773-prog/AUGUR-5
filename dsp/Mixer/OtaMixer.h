#pragma once

#include "Util/FastMath.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace augur::ota
{

// Prophet-5 Rev 3 voice mixer (SD432, U465 CA3280 with Id unused = bare differential pair):
//   saw   -> 150 K / 330 R -> (+) input      10 V   -> 21.95 mV
//   tri   -> level-shifted to +-5 V (TL082, gain 2) -> 150 K / 330 R -> (+) input (OSC B only)
//   pulse -> 200 K / 330 R -> (-) input      12.9 V -> 21.25 mV
// I_out = I_abc * tanh(Vd / 2Vt), 2Vt = 52 mV. MIX sets I_abc (linear level); the waveform levels set
// the (fixed, mild, asymmetric) curvature. Selecting saw + pulse therefore gives saw - pulse.
inline constexpr float twoVt = 0.052f;
inline constexpr float kSaw = 10.0f * 330.0f / 150330.0f / twoVt;   // x per saw unit (0..1 = 0..10 V)
inline constexpr float kPulse = 12.9f * 330.0f / 200330.0f / twoVt; // x per pulse unit (0..1 = 0..12.9 V)
inline constexpr float kTriPerVolt = 330.0f / 150330.0f / twoVt;    // x per volt of level-shifted triangle

// Output normalised so a lone full-level saw peaks at 1.
inline float norm() noexcept
{
    static const float n = 1.0f / std::tanh (kSaw);
    return n;
}

inline float mix (float saw, float tri, float pulse, float gSaw, float gTri, float gPulse) noexcept
{
    const float x = kSaw * gSaw * saw + kTriPerVolt * gTri * (10.0f * tri - 5.0f) - kPulse * gPulse * pulse;
    return fastmath::tanh (x) * norm();
}

namespace detail
{
inline double lnCosh (double x) noexcept
{
    const double a = std::abs (x);
    return a + std::log1p (std::exp (-2.0 * a)) - 0.69314718055994531;
}
} // namespace detail

// Mean of mix() over one cycle for ideal shapes (saw = phase, triangle peaking mid-cycle, pulse high
// while phase < w): exact integral of tanh over the piecewise-linear argument. Used to AC-couple the
// mixer without a start-up transient.
inline float dc (float gSaw, float gTri, float gPulse, float w) noexcept
{
    w = std::clamp (w, 0.0f, 1.0f);
    std::array<double, 4> bp { 0.0, 0.5, static_cast<double> (w), 1.0 };
    std::sort (bp.begin(), bp.end());

    const double a = static_cast<double> (kSaw * gSaw);
    const double c = static_cast<double> (kTriPerVolt * gTri);
    const double b = static_cast<double> (kPulse * gPulse);

    double sum = 0.0;
    for (size_t i = 0; i + 1 < bp.size(); ++i)
    {
        const double lo = bp[i], hi = bp[i + 1];
        if (hi - lo <= 1.0e-9)
            continue;
        const double mid = 0.5 * (lo + hi);
        const bool rising = mid < 0.5;
        // tri volts: -5 + 20*phase (rising half), 15 - 20*phase (falling half)
        const double triOffset = rising ? -5.0 : 15.0;
        const double triSlope = rising ? 20.0 : -20.0;
        const double pulse = mid < w ? 1.0 : 0.0;
        const double alpha = c * triOffset - b * pulse;
        const double beta = a + c * triSlope;
        if (std::abs (beta) > 1.0e-9)
            sum += (detail::lnCosh (alpha + beta * hi) - detail::lnCosh (alpha + beta * lo)) / beta;
        else
            sum += std::tanh (alpha + beta * mid) * (hi - lo);
    }
    return static_cast<float> (sum) * norm();
}

} // namespace augur::ota
