#include "Oscillator/Ssm2030.h"

#include <algorithm>
#include <cmath>

namespace augur
{

namespace
{
enum class Event
{
    None,
    PwEdge,
    TriPeak,
    Wrap,
    DeadEnd,
    Sync
};
} // namespace

void Ssm2030Vco::prepare (double newSampleRate, std::uint64_t seed) noexcept
{
    table = &BlepTable::get();
    sampleRate = newSampleRate;
    invSampleRate = 1.0 / newSampleRate;
    rng.setSeed (seed);
    sawRing.reset();
    triRing.reset();
    pulseRing.reset();
    phase = rng.nextFloat(); // free-running
    dead = 0.0;
    jitter = 1.0;
    incPrevious = -1.0;
    resetD = -1.0f;
}

double Ssm2030Vco::phiFromC (double c) const noexcept
{
    const double k = unit.curvature;
    if (std::abs (k) < 1.0e-6)
        return c;
    const double b = 1.0 + k;
    return (b - std::sqrt (std::max (0.0, b * b - 4.0 * k * c))) / (2.0 * k);
}

double Ssm2030Vco::staticFrequency (double pitch) const noexcept
{
    const double f = unit.expo.frequency (pitch);
    return 1.0 / (1.0 / f + static_cast<double> (unit.deadTimeSeconds));
}

// Bipolar internal shapes (-1..1); converted to the chip's positive-going outputs on the way out.
Ssm2030Vco::WaveState Ssm2030Vco::evaluate (double ph, bool inDead, double inc, float w, float p) const noexcept
{
    const double k = unit.curvature;
    const double c = ph + k * ph * (1.0 - ph);
    const double dc = inDead ? 0.0 : (1.0 + k * (1.0 - 2.0 * ph)) * inc;

    WaveState s;
    s.saw = static_cast<float> (2.0 * c - 1.0);
    s.dsaw = static_cast<float> (2.0 * dc);
    if (c < p)
    {
        s.tri = static_cast<float> (-1.0 + 2.0 * c / p);
        s.dtri = static_cast<float> (2.0 * dc / p);
    }
    else
    {
        s.tri = static_cast<float> (1.0 - 2.0 * (c - p) / (1.0 - p));
        s.dtri = static_cast<float> (-2.0 * dc / (1.0 - p));
    }
    s.pulse = c < w ? 1.0f : -1.0f;
    return s;
}

VcoOutputs Ssm2030Vco::process (float pitch, float pw, float syncD) noexcept
{
    // Phase increment linear across the sample (continuous exponential FM, see Cem3340Vco).
    const double f = std::max (0.0, static_cast<double> (unit.expo.fastFrequency (pitch)));
    const double i1 = std::min (0.45, f * invSampleRate * jitter);
    const double i0 = incPrevious < 0.0 ? i1 : incPrevious;
    incPrevious = i1;
    const double di = i1 - i0;
    const double deadSamples = static_cast<double> (unit.deadTimeSeconds) * sampleRate;

    const float w = std::clamp (pw + unit.pwOffset, 0.01f, 0.99f);
    const float p = std::clamp (unit.triPeak, 0.05f, 0.95f);
    const double phiW = phiFromC (w);
    const double phiP = phiFromC (p);
    const double k = unit.curvature;

    bool syncPending = syncD >= 0.0f;
    resetD = -1.0f;

    // Fast path: no event can happen inside this sample.
    if (dead <= 0.0 && ! syncPending)
    {
        const double phaseEnd = phase + 0.5 * (i0 + i1);
        double nextTarget = 1.0;
        if (phase < phiW) nextTarget = std::min (nextTarget, phiW);
        if (phase < phiP) nextTarget = std::min (nextTarget, phiP);
        if (phaseEnd < nextTarget)
        {
            phase = phaseEnd;
            const auto s = evaluate (phase, false, i1, w, p);
            VcoOutputs out;
            out.saw = 0.5f * (sawRing.push (s.saw) + 1.0f) * unit.sawLevel;
            out.tri = 0.5f * (triRing.push (s.tri) + 1.0f) * unit.triLevel;
            out.pulse = 0.5f * (pulseRing.push (s.pulse) + 1.0f) * unit.pulseLevel;
            return out;
        }
    }

    const auto incAt = [&] (double tt) { return i0 + di * tt; };
    const auto integral = [&] (double ta, double tb) { return i0 * (tb - ta) + 0.5 * di * (tb * tb - ta * ta); };
    const auto reachTime = [&] (double tt, double dist) {
        if (dist <= 0.0)
            return tt;
        const double r = incAt (tt);
        const double disc = r * r + 2.0 * di * dist;
        if (disc < 0.0)
            return 2.0;
        const double den = r + std::sqrt (disc);
        return den > 1.0e-300 ? tt + 2.0 * dist / den : 2.0;
    };

    const double syncT = syncPending ? 1.0 - static_cast<double> (syncD) : 2.0;

    const auto addAll = [&] (float d, const WaveState& before, const WaveState& after) {
        sawRing.add (*table, d, after.saw - before.saw, after.dsaw - before.dsaw);
        triRing.add (*table, d, after.tri - before.tri, after.dtri - before.dtri);
        pulseRing.add (*table, d, after.pulse - before.pulse, 0.0f);
    };

    double t = 0.0;
    for (int guard = 0; guard < 16; ++guard)
    {
        double tNext = 1.0;
        Event ev = Event::None;

        if (dead > 0.0)
        {
            const double tc = t + dead;
            if (tc < tNext) { tNext = tc; ev = Event::DeadEnd; }
        }
        else
        {
            if (phase < phiW)
            {
                const double tc = reachTime (t, phiW - phase);
                if (tc < tNext) { tNext = tc; ev = Event::PwEdge; }
            }
            if (phase < phiP)
            {
                const double tc = reachTime (t, phiP - phase);
                if (tc < tNext) { tNext = tc; ev = Event::TriPeak; }
            }
            const double tw = phase >= 1.0 ? t : reachTime (t, 1.0 - phase);
            if (tw < tNext) { tNext = tw; ev = Event::Wrap; }
        }

        if (syncPending && syncT >= t && syncT <= tNext)
        {
            tNext = syncT;
            ev = Event::Sync;
        }

        if (dead > 0.0)
            dead = std::max (0.0, dead - (tNext - t));
        else
            phase += integral (t, tNext);
        t = tNext;

        if (ev == Event::None)
            break;

        const double inc = incAt (t);
        const float d = static_cast<float> (std::clamp (1.0 - t, 0.0, 1.0));
        switch (ev)
        {
            case Event::PwEdge:
                phase = phiW;
                pulseRing.add (*table, d, -2.0f, 0.0f);
                break;

            case Event::TriPeak:
            {
                phase = phiP;
                const double dc = (1.0 + k * (1.0 - 2.0 * phiP)) * inc;
                triRing.add (*table, d, 0.0f, static_cast<float> (-2.0 * dc / (1.0 - p) - 2.0 * dc / p));
                break;
            }

            case Event::Wrap:
            case Event::Sync:
            {
                const auto before = evaluate (std::min (phase, 1.0), dead > 0.0, inc, w, p);
                phase = 0.0;
                dead = deadSamples;
                if (ev == Event::Wrap)
                    jitter = 1.0 + static_cast<double> (unit.cycleJitter * rng.nextGaussian());
                else
                    syncPending = false;
                addAll (d, before, evaluate (0.0, dead > 0.0, inc, w, p));
                resetD = d;
                break;
            }

            case Event::DeadEnd:
            {
                dead = 0.0;
                const auto s = evaluate (0.0, false, inc, w, p);
                sawRing.add (*table, d, 0.0f, s.dsaw);
                triRing.add (*table, d, 0.0f, s.dtri);
                break;
            }

            case Event::None:
                break;
        }
    }

    const auto s = evaluate (std::min (phase, 1.0), dead > 0.0, i1, w, p);
    VcoOutputs out;
    out.saw = 0.5f * (sawRing.push (s.saw) + 1.0f) * unit.sawLevel;
    out.tri = 0.5f * (triRing.push (s.tri) + 1.0f) * unit.triLevel;
    out.pulse = 0.5f * (pulseRing.push (s.pulse) + 1.0f) * unit.pulseLevel;
    return out;
}

} // namespace augur
