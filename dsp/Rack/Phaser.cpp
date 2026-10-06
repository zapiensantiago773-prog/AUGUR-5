#include "Rack/Phaser.h"

#include "Rack/RackFastMath.h"

#include <cmath>

namespace augur::rack
{

void Phaser::prepare (double sampleRate)
{
    sr = sampleRate;
    envCoeffA = 1.0 - std::exp (-1.0 / (0.005 * sr));
    envCoeffR = 1.0 - std::exp (-1.0 / (0.25 * sr));
    mixS.prepare (sr, 0.02);
    fbS.prepare (sr, 0.02);
    depthS.prepare (sr, 0.05);
    centreS.prepare (sr, 0.05);
    centreS.snap (std::log2 (600.0));
    reset();
}

void Phaser::reset() noexcept
{
    for (auto& c : chain)
    {
        c.s.fill (0.0);
        c.fb = 0.0;
    }
    env = 0.0;
    phase = 0.0; // a preset (or the effect switched on) always starts its sweep from the same point
}

// First-order all-pass sections (TPT): ap = 2 lp - x. The regeneration is taken from the cascade output one
// sample late and soft-limited, like the op-amp around a Small Stone's OTAs.
double Phaser::run (Chain& c, double x, double g, int stages, double feedback) noexcept
{
    const double gg = g / (1.0 + g);
    double v = x + feedback * fast::tanh (c.fb);
    for (int i = 0; i < stages; ++i)
    {
        auto& st = c.s[static_cast<size_t> (i)];
        const double d = (v - st) * gg;
        const double lp = d + st;
        st = lp + d;
        v = 2.0 * lp - v;
    }
    c.fb = v;
    return v;
}

void Phaser::process (double& left, double& right, const PhaserParams& p) noexcept
{
    const int stages = std::clamp (p.stages, 2, maxStages) & ~1;
    const double mix = mixS.next (std::clamp (p.mix, 0.0, 1.0));
    const double fb = fbS.next (std::clamp (p.feedback, -0.95, 0.95));
    const double depth = depthS.next (std::clamp (p.depth, 0.0, 1.0));
    const double centre = centreS.next (std::log2 (std::clamp (p.centreHz, 50.0, 8000.0)));

    double mod[2];
    if (p.lfo == PhaserLfo::envelope)
    {
        const double level = std::abs (0.5 * (left + right));
        env += (level > env ? envCoeffA : envCoeffR) * (level - env);
        const double m = std::clamp (2.0 * env, 0.0, 1.0) * 2.0 - 1.0;
        mod[0] = mod[1] = m;
    }
    else
    {
        phase += std::clamp (p.rateHz, 0.005, 20.0) / sr;
        phase -= std::floor (phase);
        const double off = std::clamp (p.spreadDeg, 0.0, 180.0) / 360.0;
        for (int c = 0; c < 2; ++c)
        {
            double ph = phase + (c == 1 ? off : 0.0);
            ph -= std::floor (ph);
            mod[c] = p.lfo == PhaserLfo::sine ? std::sin (2.0 * pi * ph) : (ph < 0.5 ? 4.0 * ph - 1.0 : 3.0 - 4.0 * ph);
        }
    }

    double io[2] { left, right };
    for (int c = 0; c < 2; ++c)
    {
        // Exponential sweep: +/- 2.5 octaves at full depth around MANUAL.
        const double hz = std::exp2 (centre + 2.5 * depth * mod[c]);
        lastHz[static_cast<size_t> (c)] = hz;
        const double g = std::tan (pi * std::clamp (hz, 10.0, 0.4 * sr) / sr);
        const double shifted = run (chain[static_cast<size_t> (c)], io[c], g, stages, fb);
        // mix 0.5: dry + shifted equally (notches); towards 1: only the shifted path (vibrato-like)
        io[c] = (1.0 - mix) * io[c] + mix * shifted;
    }
    left = io[0];
    right = io[1];
}

} // namespace augur::rack
