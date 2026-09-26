#pragma once

#include <array>
#include <cmath>

namespace augur
{

// 2:1 decimator: linear-phase half-band FIR (Kaiser, beta 10 -> ~100 dB stopband).
// 127 taps: passband to ~0.2245 fs_in (19.8 kHz at 88.2 kHz), stopband from ~0.2755 fs_in, so
// nothing that folds below 20 kHz survives above -100 dB. Only the non-zero taps are evaluated.
class HalfbandDecimator
{
public:
    static constexpr int numTaps = 127;
    static constexpr int centre = numTaps / 2;

    HalfbandDecimator() noexcept { design(); }

    void reset() noexcept
    {
        buffer.fill (0.0f);
        pos = 0;
    }

    // Push two input samples (oldest first) and return one output sample.
    float process (float x0, float x1) noexcept
    {
        push (x0);
        push (x1);

        const float* w = buffer.data() + pos; // w[0] oldest ... w[numTaps - 1] newest
        float acc = centreCoeff * w[centre];
        for (size_t k = 0; k < numOdd; ++k)
            acc += oddCoeffs[k] * (w[oddIndex[k]] + w[numTaps - 1 - oddIndex[k]]);
        return acc;
    }

private:
    static constexpr size_t numOdd = (centre + 1) / 2; // taps at odd distance from the centre, one side

    void push (float x) noexcept
    {
        buffer[static_cast<size_t> (pos)] = x;
        buffer[static_cast<size_t> (pos + numTaps)] = x;
        pos = (pos + 1) % numTaps;
    }

    void design() noexcept
    {
        constexpr double beta = 10.0;
        const double i0b = besselI0 (beta);
        std::array<double, numOdd> h {};
        double sum = 0.5;
        size_t k = 0;
        for (int i = 0; i < centre; ++i)
        {
            const int n = centre - i;
            if ((n & 1) == 0)
                continue; // even distances are exactly zero in a half-band filter
            const double x = static_cast<double> (n) / centre;
            const double window = besselI0 (beta * std::sqrt (1.0 - x * x)) / i0b;
            const double arg = 3.14159265358979323846 * n * 0.5;
            h[k] = 0.5 * std::sin (arg) / arg * window;
            oddIndex[k] = i;
            sum += 2.0 * h[k];
            ++k;
        }
        // Normalise for exactly unity DC gain.
        centreCoeff = static_cast<float> (0.5 / sum);
        for (size_t j = 0; j < numOdd; ++j)
            oddCoeffs[j] = static_cast<float> (h[j] / sum);
        reset();
    }

    static double besselI0 (double x) noexcept
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

    std::array<float, numTaps * 2> buffer {};
    std::array<int, numOdd> oddIndex {};
    std::array<float, numOdd> oddCoeffs {};
    float centreCoeff = 0.5f;
    int pos = 0;
};

} // namespace augur
