#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

namespace augur::rack
{

inline constexpr double pi = 3.14159265358979323846;

inline double dbToGain (double db) noexcept { return std::pow (10.0, db / 20.0); }

// One-pole smoother for continuous controls (zipper-free knobs).
struct Smooth
{
    double v = 0.0, c = 1.0;
    bool fresh = true; // after prepare the first value is taken as is: nothing glides in from a previous preset
    void prepare (double sr, double seconds) noexcept
    {
        c = 1.0 - std::exp (-1.0 / (seconds * sr));
        fresh = true;
    }
    double next (double target) noexcept
    {
        if (fresh)
        {
            fresh = false;
            return v = target;
        }
        return v += c * (target - v);
    }
    void snap (double x) noexcept { v = x; }
};

// Topology-preserving one-pole (trapezoidal): low-pass and high-pass from one state.
struct OnePole
{
    double s = 0.0, g = 0.0;
    void setCutoff (double hz, double sr) noexcept { const double t = std::tan (pi * std::clamp (hz, 1.0, 0.45 * sr) / sr); g = t / (1.0 + t); }
    double lp (double x) noexcept
    {
        const double v = (x - s) * g;
        const double y = v + s;
        s = y + v;
        return y;
    }
    double hp (double x) noexcept { return x - lp (x); }
    void reset() noexcept { s = 0.0; }
};

// State-variable filter (TPT, Zavalishin): stable under per-sample modulation.
struct Svf
{
    double ic1 = 0.0, ic2 = 0.0, g = 0.1, k = 1.414, a1 = 0.0, a2 = 0.0, a3 = 0.0;
    void set (double hz, double q, double sr) noexcept
    {
        g = std::tan (pi * std::clamp (hz, 1.0, 0.45 * sr) / sr);
        k = 1.0 / std::max (0.05, q);
        a1 = 1.0 / (1.0 + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }
    struct Out
    {
        double lp, bp, hp;
    };
    Out process (double x) noexcept
    {
        const double v3 = x - ic2;
        const double v1 = a1 * ic1 + a2 * v3;
        const double v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0 * v1 - ic1;
        ic2 = 2.0 * v2 - ic2;
        return { v2, v1, x - k * v1 - v2 };
    }
    void reset() noexcept { ic1 = ic2 = 0.0; }
};

// Shelf filters built on the one-pole (gain in dB).
struct Shelf
{
    OnePole p;
    double gain = 1.0;
    bool high = false;
    void set (bool highShelf, double hz, double db, double sr) noexcept
    {
        high = highShelf;
        p.setCutoff (hz, sr);
        gain = dbToGain (db);
    }
    double process (double x) noexcept
    {
        const double lo = p.lp (x);
        return high ? lo + gain * (x - lo) : gain * lo + (x - lo);
    }
};

// Power-of-two circular delay line with fractional (Hermite) reads.
class DelayLine
{
public:
    void prepare (double maxSamples)
    {
        int size = 4;
        while (size < static_cast<int> (maxSamples) + 8)
            size <<= 1;
        buf.assign (static_cast<size_t> (size), 0.0f);
        mask = size - 1;
        pos = 0;
    }
    void reset() noexcept
    {
        std::fill (buf.begin(), buf.end(), 0.0f);
        pos = 0;
    }
    void write (double x) noexcept
    {
        buf[static_cast<size_t> (pos)] = static_cast<float> (x);
        pos = (pos + 1) & mask;
    }
    // Delay in samples behind the newest written sample (>= 1).
    double read (double delay) const noexcept
    {
        delay = std::clamp (delay, 1.0, static_cast<double> (mask - 4));
        const double rp = static_cast<double> (pos - 1) - delay;
        const double fl = std::floor (rp);
        const double t = rp - fl;
        const int i = static_cast<int> (fl);
        const double y0 = at (i - 1), y1 = at (i), y2 = at (i + 1), y3 = at (i + 2);
        const double c1 = 0.5 * (y2 - y0), c2 = y0 - 2.5 * y1 + 2.0 * y2 - 0.5 * y3, c3 = 0.5 * (y3 - y0) + 1.5 * (y1 - y2);
        return ((c3 * t + c2) * t + c1) * t + y1;
    }
    double readInt (int delay) const noexcept { return at (pos - 1 - delay); }
    int capacity() const noexcept { return mask; }

private:
    double at (int i) const noexcept { return buf[static_cast<size_t> (i & mask)]; }
    std::vector<float> buf;
    int mask = 0, pos = 0;
};

// Halfband FIR (Kaiser-windowed, odd taps only + centre) used for 2x up / down sampling. Taps chosen per stage
// from the transition it must cover (see Drive).
class HalfbandFir
{
public:
    void design (int taps, double attenuationDb)
    {
        n = taps | 1;
        half = (n - 1) / 2;
        const double beta = 0.1102 * (attenuationDb - 8.7);
        const auto i0 = [] (double x) {
            double s = 1.0, t = 1.0;
            for (int k = 1; k < 60; ++k)
            {
                const double h = x / (2.0 * k);
                t *= h * h;
                s += t;
            }
            return s;
        };
        coeffs.clear();
        double sum = 0.0;
        for (int d = 1; d <= half; d += 2)
        {
            const double r = static_cast<double> (d) / half;
            const double w = i0 (beta * std::sqrt (std::max (0.0, 1.0 - r * r))) / i0 (beta);
            const double c = std::sin (0.5 * pi * d) / (pi * d) * w;
            coeffs.push_back (c);
            sum += 2.0 * c;
        }
        for (auto& c : coeffs)
            c *= 0.5 / sum;
        hist.assign (static_cast<size_t> (nextPow2 (n + 2)), 0.0);
        mask = static_cast<int> (hist.size()) - 1;
        pos = 0;
    }
    void reset() noexcept
    {
        std::fill (hist.begin(), hist.end(), 0.0);
        pos = 0;
    }
    // Full-rate filtering of one sample (used by both directions).
    double filter (double x) noexcept
    {
        hist[static_cast<size_t> (pos & mask)] = x;
        ++pos;
        const auto at = [&] (int back) { return hist[static_cast<size_t> ((pos - 1 - back) & mask)]; };
        double y = 0.5 * at (half);
        for (size_t k = 0; k < coeffs.size(); ++k)
        {
            const int d = 2 * static_cast<int> (k) + 1;
            y += coeffs[k] * (at (half - d) + at (half + d));
        }
        return y;
    }
    // Upsample: one input -> two outputs (zero stuffing, gain 2).
    void up (double x, double& a, double& b) noexcept
    {
        a = 2.0 * filter (x);
        b = 2.0 * filter (0.0);
    }
    // Downsample: two inputs -> one output.
    double down (double a, double b) noexcept
    {
        filter (a);
        return filter (b);
    }
    int latency() const noexcept { return half; } // in samples of the high rate

private:
    static int nextPow2 (int v)
    {
        int p = 1;
        while (p < v)
            p <<= 1;
        return p;
    }
    std::vector<double> coeffs, hist;
    int n = 0, half = 0, mask = 0, pos = 0;
};

// Tempo-synced note values (beats).
inline constexpr double syncBeats[] { 0.125, 1.0 / 6.0, 0.1875, 0.25, 1.0 / 3.0, 0.375, 0.5, 2.0 / 3.0, 0.75, 1.0, 4.0 / 3.0, 1.5, 2.0, 3.0, 4.0, 8.0 };
inline constexpr const char* syncNames[] { "1/32", "1/16T", "1/32.", "1/16", "1/8T", "1/16.", "1/8", "1/4T", "1/8.", "1/4", "1/2T", "1/4.", "1/2", "1/2.", "1/1", "2/1" };
inline constexpr int syncCount = static_cast<int> (sizeof (syncBeats) / sizeof (double));

} // namespace augur::rack
