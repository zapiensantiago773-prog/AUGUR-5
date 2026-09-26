#pragma once

#include <algorithm>
#include <cmath>
#include <complex>
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

inline double rms (const std::vector<float>& x, std::size_t from = 0)
{
    double s = 0.0;
    for (std::size_t i = from; i < x.size(); ++i)
        s += static_cast<double> (x[i]) * x[i];
    return std::sqrt (s / static_cast<double> (std::max<std::size_t> (1, x.size() - from)));
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
        const double ang = -2.0 * 3.14159265358979323846 / static_cast<double> (len);
        const std::complex<double> wl (std::cos (ang), std::sin (ang));
        for (std::size_t i = 0; i < n; i += len)
        {
            std::complex<double> w (1.0);
            for (std::size_t j = 0; j < len / 2; ++j)
            {
                const auto u = a[i + j], v = a[i + j + len / 2] * w;
                a[i + j] = u + v;
                a[i + j + len / 2] = u - v;
                w *= wl;
            }
        }
    }
}

inline double besselI0 (double x)
{
    double sum = 1.0, term = 1.0;
    for (int k = 1; k < 200; ++k)
    {
        const double t = x / (2.0 * k);
        term *= t * t;
        sum += term;
        if (term < 1.0e-25 * sum)
            break;
    }
    return sum;
}

// Magnitude spectrum in dB relative to the strongest bin, Kaiser (beta 20, sidelobes < -150 dB) window.
inline std::vector<double> spectrumDb (const std::vector<float>& x, std::size_t start, std::size_t n)
{
    std::vector<std::complex<double>> a (n);
    const double i0b = besselI0 (20.0);
    for (std::size_t i = 0; i < n; ++i)
    {
        const double r = 2.0 * static_cast<double> (i) / static_cast<double> (n - 1) - 1.0;
        a[i] = x[start + i] * besselI0 (20.0 * std::sqrt (std::max (0.0, 1.0 - r * r))) / i0b;
    }
    fft (a);
    std::vector<double> mag (n / 2);
    double peak = 1.0e-30;
    for (std::size_t i = 0; i < n / 2; ++i)
        peak = std::max (peak, mag[i] = std::abs (a[i]));
    for (auto& m : mag)
        m = 20.0 * std::log10 (std::max (m / peak, 1.0e-15));
    return mag;
}

// Worst non-harmonic component below maxHz, in dB relative to the strongest harmonic.
inline double worstAliasDb (const std::vector<float>& x, double sampleRate, double f0, double maxHz, std::size_t start, std::size_t n)
{
    const auto db = spectrumDb (x, start, n);
    const double binHz = sampleRate / static_cast<double> (n);
    constexpr double guardBins = 14.0; // Kaiser-20 main lobe half-width is ~6.5 bins
    double worst = -300.0;
    for (std::size_t b = 0; b < db.size(); ++b)
    {
        const double f = static_cast<double> (b) * binHz;
        if (f > maxHz || f < 20.0)
            continue;
        const double h = std::round (f / f0);
        if (std::abs (f - h * f0) < guardBins * binHz)
            continue; // harmonic (or DC) region
        worst = std::max (worst, db[b]);
    }
    return worst;
}

} // namespace augur::test
