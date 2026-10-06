#pragma once

#include <cmath>

namespace augur::rack::fast
{

// tanh from Lambert's continued fraction truncated at order 7 (max error ~2e-7 inside |x| < 4.97, where
// it reaches 0.99990; clamped outside). Every saturating transistor stage of the instrument uses it.
inline double tanh (double x) noexcept
{
    if (x > 4.97)
        return 0.99990920;
    if (x < -4.97)
        return -0.99990920;
    const double x2 = x * x;
    return x * (135135.0 + x2 * (17325.0 + x2 * (378.0 + x2))) / (135135.0 + x2 * (62370.0 + x2 * (3150.0 + 28.0 * x2)));
}

inline float tanh (float x) noexcept { return static_cast<float> (tanh (static_cast<double> (x))); }

} // namespace augur::rack::fast
