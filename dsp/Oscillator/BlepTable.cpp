#include "Oscillator/BlepTable.h"

#include <algorithm>
#include <cmath>

namespace augur
{

namespace
{
double besselI0 (double x)
{
    double sum = 1.0, term = 1.0;
    for (int k = 1; k < 64; ++k)
    {
        const double t = x / (2.0 * k);
        term *= t * t;
        sum += term;
        if (term < 1.0e-20 * sum)
            break;
    }
    return sum;
}
} // namespace

const BlepTable& BlepTable::get()
{
    static const BlepTable table;
    return table;
}

BlepTable::BlepTable()
{
    constexpr double beta = 11.2;
    constexpr double cutoff = 0.48; // cycles per sample
    constexpr double pi = 3.14159265358979323846;
    constexpr int n = taps * phases + 1;
    const double dt = 1.0 / phases;
    const double i0b = besselI0 (beta);

    std::vector<double> h (n), step (n), ramp (n);
    for (int i = 0; i < n; ++i)
    {
        const double t = -zeroCrossings + i * dt;
        const double x = t / zeroCrossings;
        const double window = besselI0 (beta * std::sqrt (std::max (0.0, 1.0 - x * x))) / i0b;
        const double arg = pi * 2.0 * cutoff * t;
        const double sinc = std::abs (arg) < 1.0e-12 ? 1.0 : std::sin (arg) / arg;
        h[static_cast<size_t> (i)] = 2.0 * cutoff * sinc * window;
    }

    // Band-limited step = running integral of the kernel, normalised to end exactly at 1.
    step[0] = 0.0;
    for (size_t i = 1; i < static_cast<size_t> (n); ++i)
        step[i] = step[i - 1] + 0.5 * (h[i - 1] + h[i]) * dt;
    const double norm = step[static_cast<size_t> (n - 1)];
    for (auto& s : step)
        s /= norm;

    // Band-limited ramp = integral of the band-limited step.
    ramp[0] = 0.0;
    for (size_t i = 1; i < static_cast<size_t> (n); ++i)
        ramp[i] = ramp[i - 1] + 0.5 * (step[i - 1] + step[i]) * dt;

    // Residuals relative to the ideal (naive) step and ramp, stored per tap segment. The ideal step is
    // constant over a segment (0 before the event, 1 after), so the jump at t = 0 sits exactly on a
    // segment boundary and is never interpolated across.
    blep.assign (static_cast<size_t> (taps * segment + 1), 0.0f);
    blamp.assign (static_cast<size_t> (taps * segment + 1), 0.0f);
    for (int j = 0; j < taps; ++j)
    {
        const double idealStep = j >= zeroCrossings ? 1.0 : 0.0;
        for (int k = 0; k <= phases; ++k)
        {
            const auto src = static_cast<size_t> (j * phases + k);
            const double t = -zeroCrossings + static_cast<double> (src) * dt;
            const auto dst = static_cast<size_t> (j * segment + k);
            blep[dst] = static_cast<float> (step[src] - idealStep);
            blamp[dst] = static_cast<float> (ramp[src] - std::max (t, 0.0));
        }
    }
}

} // namespace augur
