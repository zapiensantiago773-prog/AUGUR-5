#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace augur::test
{

inline constexpr double sampleRates[] = { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 };

// Frequency from positive-going zero crossings, with linear interpolation of each crossing time.
inline double measureFrequency (const std::vector<float>& x, double sampleRate, std::size_t skip = 0)
{
    double first = -1.0, last = -1.0;
    int crossings = 0;
    for (std::size_t n = std::max<std::size_t> (skip, 1); n < x.size(); ++n)
    {
        if (x[n - 1] < 0.0f && x[n] >= 0.0f)
        {
            const double frac = x[n - 1] / (x[n - 1] - x[n]);
            const double t = static_cast<double> (n - 1) + frac;
            if (first < 0.0)
                first = t;
            last = t;
            ++crossings;
        }
    }
    if (crossings < 2)
        return 0.0;
    return (crossings - 1) * sampleRate / (last - first);
}

inline float maxStep (const std::vector<float>& x, std::size_t from = 1, std::size_t to = static_cast<std::size_t> (-1))
{
    float m = 0.0f;
    for (std::size_t n = std::max<std::size_t> (from, 1); n < std::min (to, x.size()); ++n)
        m = std::max (m, std::abs (x[n] - x[n - 1]));
    return m;
}

} // namespace augur::test
