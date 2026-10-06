#include "Rack/Flanger.h"

#include "Rack/RackFastMath.h"

#include <cmath>

namespace augur::rack
{

void Flanger::prepare (double sampleRate)
{
    sr = sampleRate;
    for (int c = 0; c < 2; ++c)
    {
        wetLine[static_cast<size_t> (c)].prepare (0.03 * sr);
        dryLine[static_cast<size_t> (c)].prepare (0.03 * sr);
        damp[static_cast<size_t> (c)].setCutoff (9000.0, sr); // BBD bandwidth in the loop (NV)
    }
    mixS.prepare (sr, 0.02);
    fbS.prepare (sr, 0.02);
    depthS.prepare (sr, 0.05);
    manualS.prepare (sr, 0.05);
    manualS.snap (2.0);
    reset();
}

void Flanger::reset() noexcept
{
    for (int c = 0; c < 2; ++c)
    {
        wetLine[static_cast<size_t> (c)].reset();
        dryLine[static_cast<size_t> (c)].reset();
        damp[static_cast<size_t> (c)].reset();
        fb[static_cast<size_t> (c)] = 0.0;
    }
    phase = 0.0;
}

void Flanger::process (double& left, double& right, const FlangerParams& p) noexcept
{
    const double mix = mixS.next (std::clamp (p.mix, 0.0, 1.0));
    const double feedback = fbS.next (std::clamp (p.feedback, -0.95, 0.95));
    const double depth = depthS.next (std::clamp (p.depth, 0.0, 1.0));
    const double manual = manualS.next (std::clamp (p.manualMs, 0.1, 10.0));
    phase += std::clamp (p.rateHz, 0.005, 20.0) / sr;
    phase -= std::floor (phase);
    const double off = std::clamp (p.spreadDeg, 0.0, 180.0) / 360.0;

    double io[2] { left, right };
    for (int c = 0; c < 2; ++c)
    {
        const auto uc = static_cast<size_t> (c);
        double ph = phase + (c == 1 ? off : 0.0);
        ph -= std::floor (ph);
        const double tri = ph < 0.5 ? 4.0 * ph - 1.0 : 3.0 - 4.0 * ph;
        double ms;
        if (p.throughZero)
        {
            // Dry held at MANUAL; the swept path travels MANUAL * (1 +/- depth): at depth 1 it reaches zero
            // relative delay and crosses to the other side.
            ms = manual * (1.0 + depth * tri);
        }
        else
        {
            // Exponential sweep over up to 5 octaves of delay below MANUAL... and a little above.
            ms = manual * std::exp2 (depth * 2.5 * (tri - 0.6));
        }
        ms = std::clamp (ms, 0.02, 25.0);
        lastMs[uc] = ms;

        const double in = io[c] + feedback * fast::tanh (fb[uc]);
        wetLine[uc].write (in);
        const double wet = damp[uc].lp (wetLine[uc].read (std::max (1.0, ms * 0.001 * sr)));
        fb[uc] = wet;
        double dry = io[c];
        if (p.throughZero)
        {
            dryLine[uc].write (io[c]);
            dry = dryLine[uc].read (std::max (1.0, manual * 0.001 * sr));
        }
        // (Negative regeneration alone moves the comb peaks to the odd harmonics: the hollow "jet".)
        const double w = wet;
        // mix 1 = dry and wet at equal level (deepest comb). Through-zero sums them in opposite polarity, so the
        // zero crossing of the relative delay is a complete cancellation, as with two tape machines.
        io[c] = (1.0 - 0.5 * mix) * dry + 0.5 * mix * (p.throughZero ? -w : w);
    }
    left = io[0];
    right = io[1];
}

} // namespace augur::rack
