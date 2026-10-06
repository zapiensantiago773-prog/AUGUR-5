#include "Rack/Chorus.h"

#include "Rack/RackFastMath.h"

#include <cmath>

namespace augur::rack
{

void Chorus::prepare (double sampleRate, std::uint64_t seed)
{
    sr = sampleRate;
    rng.seedWith (seed);
    for (auto& b : bbd)
        b.line.prepare (0.05 * sr);
    crossHpL.setCutoff (200.0, sr); // Dimension D: the cross-fed wet signal goes through a high-pass
    crossHpR.setCutoff (200.0, sr);
    bassL.setCutoff (180.0, sr);    // ... and the dry signal gets a bass lift to make up for it
    bassR.setCutoff (180.0, sr);
    mixS.prepare (sr, 0.02);
    depthS.prepare (sr, 0.05);
    widthS.prepare (sr, 0.05);
    rateS.prepare (sr, 0.05);
    rateS.snap (1.0);
    depthS.snap (1.0);
    reset();
}

void Chorus::reset() noexcept
{
    for (auto& b : bbd)
    {
        b.line.reset();
        b.pre.reset();
        b.post.reset();
    }
    crossHpL.reset();
    crossHpR.reset();
    bassL.reset();
    bassR.reset();
    phase = vibPhase = 0.0;
}

// One BBD: band-limit at the input, delay, reconstruct, compander headroom, hiss.
double Chorus::Bbd::run (double in, double delayMs, double rate, double toneHz, double hissAmount, Rng& r) noexcept
{
    pre.set (toneHz, 0.707, rate);
    post.set (toneHz, 0.707, rate);
    line.write (fast::tanh (pre.process (in).lp * 0.7) / 0.7); // compander: soft ceiling
    const double y = line.read (delayMs * 0.001 * rate);
    return post.process (y + hissAmount * 0.0015 * r.gauss()).lp;
}

// Dimension D's LFO: a triangle clipped into a trapezoid, so the delay rests at each end instead of
// turning around abruptly (no audible pitch warble).
double Chorus::trapezoid (double ph) const noexcept
{
    return std::clamp (1.6 * triangle (ph), -1.0, 1.0);
}

void Chorus::process (double& left, double& right, const ChorusParams& p) noexcept
{
    if (p.mode != lastMode)
    {
        lastMode = p.mode;
        reset();
    }
    const double mix = mixS.next (std::clamp (p.mix, 0.0, 1.0));
    const double depth = depthS.next (std::clamp (p.depth, 0.0, 2.0));
    const double width = widthS.next (std::clamp (p.width, 0.0, 1.5));
    const double rateMul = rateS.next (std::clamp (p.rate, 0.25, 4.0));
    const double toneHz = 2500.0 * std::pow (6.0, std::clamp (p.tone, 0.0, 1.0)); // 2.5 .. 15 kHz
    const double hiss = std::clamp (p.hiss, 0.0, 1.0);
    const double mono = 0.5 * (left + right);
    double wetL = 0.0, wetR = 0.0, dryL = left, dryR = right;

    switch (p.mode)
    {
        case ChorusMode::juno1:
        case ChorusMode::juno2:
        case ChorusMode::juno12:
        {
            // One LFO, two BBDs; the right line gets the inverted modulation (180 degrees).
            double hz = 0.513, lo = 1.66, hi = 5.35;
            if (p.mode == ChorusMode::juno2)
                hz = 0.863;
            else if (p.mode == ChorusMode::juno12)
            {
                hz = 9.75;
                lo = 3.3;
                hi = 3.7;
            }
            phase += hz * rateMul / sr;
            phase -= std::floor (phase);
            const double m = p.mode == ChorusMode::juno12 ? std::sin (2.0 * pi * phase) : triangle (phase);
            const double centre = 0.5 * (lo + hi), span = 0.5 * (hi - lo) * depth;
            wetL = bbd[0].run (mono, centre + span * m, sr, toneHz, hiss, rng);
            wetR = bbd[1].run (mono, centre - span * m, sr, toneHz, hiss, rng);
            break;
        }
        case ChorusMode::dimension:
        {
            phase += 0.25 * rateMul / sr; // slow sweep (NV)
            phase -= std::floor (phase);
            const double m = trapezoid (phase);
            const double span = 2.0 * depth; // 8 .. 12 ms at depth 1
            const double a = bbd[0].run (left, 10.0 + span * m, sr, toneHz, hiss, rng);
            const double b = bbd[1].run (right, 10.0 - span * m, sr, toneHz, hiss, rng);
            // own channel + inverted, high-passed cross-feed; dry gets a bass lift (Roland's description)
            wetL = a - crossHpL.hp (b);
            wetR = b - crossHpR.hp (a);
            dryL = left + 0.3 * bassL.lp (left);
            dryR = right + 0.3 * bassR.lp (right);
            break;
        }
        case ChorusMode::ensemble:
        {
            // Two 3-phase generators: slow "chorus" (~0.6 Hz) and fast "vibrato" (~6 Hz), 120 degrees apart
            // per BBD; each line gets a blend of both.
            phase += 0.6 * rateMul / sr;
            vibPhase += 6.0 * rateMul / sr;
            phase -= std::floor (phase);
            vibPhase -= std::floor (vibPhase);
            double out[3];
            for (int i = 0; i < 3; ++i)
            {
                const double off = i / 3.0;
                const double slow = std::sin (2.0 * pi * (phase + off));
                const double fast = std::sin (2.0 * pi * (vibPhase + off));
                const double d = 6.0 + depth * (2.5 * slow + 0.35 * fast); // ms (NV)
                out[i] = bbd[static_cast<size_t> (i)].run (mono, d, sr, toneHz, hiss, rng);
            }
            wetL = out[0] + 0.5 * out[1];
            wetR = out[2] + 0.5 * out[1];
            wetL *= 0.67;
            wetR *= 0.67;
            break;
        }
    }

    // Width: M/S on the wet signal.
    const double mid = 0.5 * (wetL + wetR), side = 0.5 * (wetL - wetR) * width;
    wetL = mid + side;
    wetR = mid - side;
    // Constant-power blend: at 50 % dry and wet are equal, as on the Juno's output summing.
    const double gd = std::cos (0.5 * pi * mix), gw = std::sin (0.5 * pi * mix);
    left = dryL * gd + wetL * gw;
    right = dryR * gd + wetR * gw;
}

} // namespace augur::rack
