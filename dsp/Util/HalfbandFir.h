#pragma once

#include <algorithm>
#include <array>
#include <cmath>

#include "Util/Simd4.h"

namespace augur
{

// Short linear-phase half-band FIR (Kaiser, beta 10 -> ~100 dB stopband) for extra oversampling
// stages inside nonlinear effects. N = 4k + 3 taps. Kaiser transition width ~ 91.5 / (2.285 * 2 pi * (N - 1))
// of the high rate, centred on a quarter of it: 31 taps -> 0.144 .. 0.356, 27 -> 0.128 .. 0.372,
// 19 -> 0.073 .. 0.427. The higher the stage's rate, the wider the gap between the audio band and
// its images, so later stages can be shorter.
template <int N>
struct HalfbandKernel
{
    static_assert ((N - 3) % 4 == 0, "half-band length must be 4k + 3");
    static constexpr int centre = N / 2; // odd
    static constexpr int numOdd = (centre + 1) / 2;

    std::array<float, numOdd> odd {}; // odd[k]: tap at distance 2k + 1 from the centre
    float mid = 0.5f;

    HalfbandKernel() noexcept
    {
        constexpr double beta = 10.0;
        const auto i0 = [] (double x) {
            double sum = 1.0, term = 1.0;
            for (int k = 1; k < 64; ++k)
            {
                const double t = x / (2.0 * k);
                term *= t * t;
                sum += term;
            }
            return sum;
        };
        const double i0b = i0 (beta);
        double sum = 0.5;
        std::array<double, numOdd> h {};
        for (int k = 0; k < numOdd; ++k)
        {
            const int d = 2 * k + 1;
            const double x = static_cast<double> (d) / centre;
            const double arg = 3.14159265358979323846 * d * 0.5;
            h[static_cast<size_t> (k)] = 0.5 * std::sin (arg) / arg * i0 (beta * std::sqrt (std::max (0.0, 1.0 - x * x))) / i0b;
            sum += 2.0 * h[static_cast<size_t> (k)];
        }
        mid = static_cast<float> (0.5 / sum); // unity DC gain
        for (size_t k = 0; k < static_cast<size_t> (numOdd); ++k)
            odd[k] = static_cast<float> (h[k] / sum);
    }
};

namespace detail
{
inline float hbScale (float c, float x) noexcept { return c * x; }
inline simd::F4 hbScale (float c, simd::F4 x) noexcept { return simd::splat (c) * x; }
} // namespace detail

// 1 -> 2 interpolator (zero-stuffing + half-band, gain 2). Latency: centre samples at the high rate.
// T = float, or simd::F4 to run several channels at once.
template <int N, typename T = float>
class HalfbandUpsampler
{
public:
    void reset() noexcept
    {
        buf.fill (T {});
        pos = 0;
    }

    void process (T x, T& first, T& second) noexcept
    {
        using detail::hbScale;
        pos = pos == 0 ? H - 1 : pos - 1;
        buf[static_cast<size_t> (pos)] = x;
        buf[static_cast<size_t> (pos + H)] = x;
        const T* w = buf.data() + pos; // w[i] = x[m - i]
        T acc = hbScale (2.0f * kernel.odd[0], w[(K::centre - 1) / 2] + w[(K::centre + 1) / 2]);
        for (int k = 1; k < K::numOdd; ++k)
        {
            const int d = 2 * k + 1;
            acc = acc + hbScale (2.0f * kernel.odd[static_cast<size_t> (k)], w[(K::centre - d) / 2] + w[(K::centre + d) / 2]);
        }
        first = acc;
        second = hbScale (2.0f * kernel.mid, w[(K::centre - 1) / 2]);
    }

private:
    using K = HalfbandKernel<N>;
    static constexpr int H = K::centre + 1;
    inline static const K kernel {};
    std::array<T, 2 * H> buf {};
    int pos = 0;
};

// 2 -> 1 decimator (half-band). Push two high-rate samples, oldest first.
template <int N, typename T = float>
class HalfbandDownsampler
{
public:
    void reset() noexcept
    {
        buf.fill (T {});
        pos = 0;
    }

    T process (T x0, T x1) noexcept
    {
        using detail::hbScale;
        push (x0);
        push (x1);
        const T* w = buf.data() + pos; // w[j] = v[t - j], newest first
        T acc = hbScale (kernel.mid, w[K::centre]);
        for (int k = 0; k < K::numOdd; ++k)
        {
            const int d = 2 * k + 1;
            acc = acc + hbScale (kernel.odd[static_cast<size_t> (k)], w[K::centre - d] + w[K::centre + d]);
        }
        return acc;
    }

private:
    using K = HalfbandKernel<N>;
    void push (T x) noexcept
    {
        pos = pos == 0 ? N - 1 : pos - 1;
        buf[static_cast<size_t> (pos)] = x;
        buf[static_cast<size_t> (pos + N)] = x;
    }
    inline static const K kernel {};
    std::array<T, 2 * N> buf {};
    int pos = 0;
};

} // namespace augur
