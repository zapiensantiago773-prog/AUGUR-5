#pragma once

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstddef>
#include <vector>

namespace augur::rackmeasure
{

inline constexpr double pi = 3.14159265358979323846;

inline double cents (double f, double ref) { return 1200.0 * std::log2 (f / ref); }
inline double pitchOf (double hz) { return 69.0 + 12.0 * std::log2 (hz / 440.0); }

// Mean frequency from rising zero crossings (linearly interpolated), skipping `from` samples.
inline double frequency (const std::vector<float>& x, double sr, std::size_t from = 0, std::size_t to = 0)
{
    to = to == 0 ? x.size() : std::min (to, x.size());
    double first = -1.0, last = -1.0;
    long count = 0;
    for (std::size_t n = std::max<std::size_t> (from, 1); n < to; ++n)
        if (x[n - 1] < 0.0f && x[n] >= 0.0f)
        {
            const double tc = static_cast<double> (n - 1) + x[n - 1] / (x[n - 1] - x[n]);
            if (first < 0.0)
                first = tc;
            last = tc;
            ++count;
        }
    return count < 2 ? 0.0 : static_cast<double> (count - 1) * sr / (last - first);
}

inline double rms (const std::vector<float>& x, std::size_t from = 0)
{
    double s = 0.0;
    for (std::size_t i = from; i < x.size(); ++i)
        s += static_cast<double> (x[i]) * x[i];
    return std::sqrt (s / static_cast<double> (std::max<std::size_t> (1, x.size() - from)));
}

inline double i0 (double x)
{
    double sum = 1.0, term = 1.0;
    for (int k = 1; k < 300; ++k)
    {
        const double h = x / (2.0 * k);
        term *= h * h;
        sum += term;
        if (term < 1.0e-30 * sum)
            break;
    }
    return sum;
}

inline void fft (std::vector<std::complex<double>>& a)
{
    const std::size_t n = a.size();
    for (std::size_t i = 1, j = 0; i < n; ++i)
    {
        std::size_t bit = n >> 1;
        for (; j & bit; bit >>= 1)
            j ^= bit;
        j ^= bit;
        if (i < j)
            std::swap (a[i], a[j]);
    }
    for (std::size_t len = 2; len <= n; len <<= 1)
    {
        const std::complex<double> wl = std::polar (1.0, -2.0 * pi / static_cast<double> (len));
        for (std::size_t i = 0; i < n; i += len)
        {
            std::complex<double> w (1.0);
            for (std::size_t k = 0; k < len / 2; ++k, w *= wl)
            {
                const auto u = a[i + k], v = a[i + k + len / 2] * w;
                a[i + k] = u + v;
                a[i + k + len / 2] = u - v;
            }
        }
    }
}

// dB spectrum relative to its strongest bin; Kaiser window beta 20 (sidelobes below -150 dB).
inline std::vector<double> spectrum (const std::vector<float>& x, std::size_t start, std::size_t n)
{
    std::vector<std::complex<double>> a (n);
    const double norm = i0 (20.0);
    for (std::size_t i = 0; i < n; ++i)
    {
        const double r = 2.0 * static_cast<double> (i) / static_cast<double> (n - 1) - 1.0;
        a[i] = x[start + i] * i0 (20.0 * std::sqrt (std::max (0.0, 1.0 - r * r))) / norm;
    }
    fft (a);
    std::vector<double> db (n / 2);
    double peak = 1.0e-300;
    for (std::size_t i = 0; i < db.size(); ++i)
        peak = std::max (peak, db[i] = std::abs (a[i]));
    for (auto& v : db)
        v = 20.0 * std::log10 (std::max (v / peak, 1.0e-16));
    return db;
}

// Strongest component in 20 Hz..maxHz that is not on the harmonic grid of `grid` Hz (dB re. peak).
inline double worstAlias (const std::vector<float>& x, double sr, double grid, double maxHz = 20000.0, std::size_t start = 8192, std::size_t n = 65536)
{
    const auto db = spectrum (x, start, n);
    const double bin = sr / static_cast<double> (n);
    constexpr double guard = 14.0; // bins: Kaiser-20 main lobe is ~6.5 bins wide on each side
    double worst = -300.0;
    for (std::size_t b = 0; b < db.size(); ++b)
    {
        const double f = static_cast<double> (b) * bin;
        if (f < 20.0 || f > maxHz)
            continue;
        if (std::abs (f - std::round (f / grid) * grid) < guard * bin)
            continue;
        worst = std::max (worst, db[b]);
    }
    return worst;
}

// Level (dB re. peak) of the strongest bin within +/-4 bins of each harmonic k*f0, k = 1..count.
inline std::vector<double> harmonics (const std::vector<float>& x, double sr, double f0, int count, std::size_t start = 8192, std::size_t n = 65536)
{
    const auto db = spectrum (x, start, n);
    const double bin = sr / static_cast<double> (n);
    std::vector<double> h (static_cast<std::size_t> (count + 1), -300.0);
    for (int k = 1; k <= count; ++k)
    {
        const auto c = static_cast<long> (std::lround (k * f0 / bin));
        for (long b = c - 4; b <= c + 4; ++b)
            if (b > 0 && b < static_cast<long> (db.size()))
                h[static_cast<std::size_t> (k)] = std::max (h[static_cast<std::size_t> (k)], db[static_cast<std::size_t> (b)]);
    }
    return h;
}

} // namespace augur::rackmeasure
