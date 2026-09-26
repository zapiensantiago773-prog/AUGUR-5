#include "Oscillator/Cem3340.h"

#include <algorithm>
#include <cmath>

namespace augur
{

void Cem3340Vco::prepare (double newSampleRate, std::uint64_t seed) noexcept
{
    table = &BlepTable::get();
    sampleRate = newSampleRate;
    twoOverSampleRate = 2.0 / newSampleRate;
    rng.setSeed (seed);
    sawRing.reset();
    triRing.reset();
    pulseRing.reset();

    // Free-running: every chip powers up somewhere in its cycle.
    v = rng.nextFloat();
    dir = rng.nextFloat() < 0.5f ? 1 : -1;
    hold = 0.0;
    pendingFall = -1.0;
    wHistory = { 0.5f, 0.5f, 0.5f };
    basePrevious = -1.0;
    resetD = -1.0f;
    comparatorHigh = pulseHigh = saw (v, dir) < 0.5;
}

double Cem3340Vco::sawSlope (int direction, double su, double sd) const noexcept
{
    return direction > 0 ? 0.5 * su : 0.5 * static_cast<double> (unit.converterGain) * sd;
}

double Cem3340Vco::staticFrequency (double pitch) const noexcept
{
    const double f = unit.expo.frequency (pitch);
    const double e = unit.asymmetry;
    const double su = 2.0 * f * (1.0 - 0.5 * e);
    const double sd = 2.0 * f * (1.0 + 0.5 * e);
    const double td = unit.comparatorDelay;
    // Each half travels between the overshoot levels (1 + su*td) and (-sd*td).
    const double span = 1.0 + (su + sd) * td;
    return 1.0 / (span * (1.0 / su + 1.0 / sd));
}

VcoOutputs Cem3340Vco::processEvents (double b0, double b1, float wNew, float syncD) noexcept
{
    basePrevious = b1;
    const double db = b1 - b0;

    const double e = unit.asymmetry;
    const double kUp = 1.0 - 0.5 * e; // charge current relative to nominal
    const double kDn = 1.0 + 0.5 * e; // discharge current
    const double tdS = static_cast<double> (unit.comparatorDelay) * sampleRate;
    const double holdS = static_cast<double> (unit.syncHold) * sampleRate;
    const double fallS = static_cast<double> (unit.pulseFallDelay) * sampleRate;

    // PWM threshold: Catmull-Rom cubic through the CV samples, evaluated one sample late, so the
    // comparator crossing of audio-rate PWM lands where the continuous CV would put it.
    const double p0 = wHistory[0], p1 = wHistory[1], p2 = wHistory[2];
    const double p3 = wNew;
    wHistory = { static_cast<float> (p1), static_cast<float> (p2), wNew };

    bool syncPending = syncD >= 0.0f;
    resetD = -1.0f;

    const double c1 = 0.5 * (p2 - p0);
    const double c2 = p0 - 2.5 * p1 + 2.0 * p2 - 0.5 * p3;
    const double c3 = 0.5 * (p3 - p0) + 1.5 * (p1 - p2);
    const auto wAt = [&] (double tt) { return ((c3 * tt + c2) * tt + c1) * tt + p1; };
    const auto wSlopeAt = [&] (double tt) { return (3.0 * c3 * tt + 2.0 * c2) * tt + c1; };

    const auto rateAt = [&] (double tt) { return b0 + db * tt; };
    const auto integral = [&] (double ta, double tb) { return b0 * (tb - ta) + 0.5 * db * (tb * tb - ta * ta); };
    // Time at which `dist` (in rate units) has been covered starting from tt; 2 = not within this sample.
    const auto reachTime = [&] (double tt, double dist) {
        if (dist <= 0.0)
            return tt;
        const double r = rateAt (tt);
        const double disc = r * r + 2.0 * db * dist;
        if (disc < 0.0)
            return 2.0;
        const double den = r + std::sqrt (disc);
        return den > 1.0e-300 ? tt + 2.0 * dist / den : 2.0;
    };

    const double syncT = syncPending ? 1.0 - static_cast<double> (syncD) : 2.0;
    const auto currentSaw = [&] { return hold > 0.0 ? 0.0 : saw (v, dir); };

    const auto setComparator = [&] (bool high, double tt) {
        comparatorHigh = high;
        const float d = static_cast<float> (std::clamp (1.0 - tt, 0.0, 1.0));
        if (high)
        {
            if (pendingFall >= 0.0)
                pendingFall = -1.0; // the slow fall never completed: the pulse simply stays high
            else if (! pulseHigh)
            {
                pulseRing.add (*table, d, 1.0f, 0.0f); // fast rise
                pulseHigh = true;
            }
        }
        else if (fallS <= 0.0)
        {
            if (pulseHigh)
            {
                pulseRing.add (*table, d, -1.0f, 0.0f);
                pulseHigh = false;
            }
        }
        else
        {
            pendingFall = fallS; // slow fall: equivalent to a step delayed by half the fall time
        }
    };

    const auto recheck = [&] (double tt) {
        const bool high = currentSaw() < wAt (tt);
        if (high != comparatorHigh)
            setComparator (high, tt);
    };

    recheck (0.0);

    double t = 0.0;
    for (int guard = 0; guard < 24; ++guard)
    {
        double tNext = 1.0;
        Ev ev = Ev::None;
        double turnLevel = 0.0;
        const auto consider = [&] (double tc, Ev candidate) {
            if (tc < tNext)
            {
                tNext = std::max (tc, t);
                ev = candidate;
            }
        };

        if (hold > 0.0)
            consider (t + hold, Ev::HoldEnd);
        else if (dir > 0)
        {
            turnLevel = 1.0 + kUp * rateAt (t) * tdS; // the comparator reacts late: the triangle overshoots
            consider (v >= turnLevel ? t : reachTime (t, (turnLevel - v) / kUp), Ev::TurnTop);
        }
        else
        {
            turnLevel = -kDn * rateAt (t) * tdS;
            consider (v <= turnLevel ? t : reachTime (t, (v - turnLevel) / kDn), Ev::TurnBottom);
        }

        if (pendingFall >= 0.0)
            consider (t + pendingFall, Ev::FallDue);

        {
            // Comparator: saw (following the charging rate) against the cubic threshold. A sign change of
            // saw - w before the next event means a crossing; locate it with safeguarded Newton steps.
            const double s0 = currentSaw();
            const double ks = hold > 0.0 ? 0.0 : (dir > 0 ? 0.5 * kUp : 0.5 * static_cast<double> (unit.converterGain) * kDn);
            const auto diff = [&] (double tt) { return s0 + ks * integral (t, tt) - wAt (tt); };
            const double dEnd = diff (tNext);
            if (comparatorHigh ? dEnd >= 0.0 : dEnd < 0.0)
            {
                double lo = t, hi = tNext;
                double x = hi;
                const double d0 = diff (t);
                if (d0 != dEnd)
                    x = t + (tNext - t) * d0 / (d0 - dEnd); // secant start
                for (int it = 0; it < 6; ++it)
                {
                    const double fx = diff (x);
                    if (std::abs (fx) < 1.0e-13)
                        break; // converged
                    if ((fx >= 0.0) == comparatorHigh) hi = x; else lo = x;
                    const double slope = ks * rateAt (x) - wSlopeAt (x);
                    double next = std::abs (slope) > 1.0e-12 ? x - fx / slope : 0.5 * (lo + hi);
                    if (next < lo || next > hi)
                        next = 0.5 * (lo + hi); // Newton left the bracket: bisect instead
                    x = next;
                }
                consider (std::clamp (x, t, tNext), Ev::Comparator);
            }
        }

        if (syncPending && syncT >= t && syncT <= tNext)
        {
            tNext = syncT;
            ev = Ev::Sync;
        }

        const double dt = tNext - t;
        if (hold > 0.0)
            hold = std::max (0.0, hold - dt);
        else
            v += (dir > 0 ? kUp : -kDn) * integral (t, tNext);
        if (pendingFall >= 0.0)
            pendingFall = std::max (0.0, pendingFall - dt);
        t = tNext;

        if (ev == Ev::None)
            break;

        const double su = kUp * rateAt (t), sd = kDn * rateAt (t); // slopes at the event, per sample
        const float d = static_cast<float> (std::clamp (1.0 - t, 0.0, 1.0));
        switch (ev)
        {
            case Ev::TurnTop:
            {
                v = turnLevel;
                const double before = saw (v, 1), after = saw (v, -1);
                sawRing.add (*table, d, static_cast<float> (after - before),
                             static_cast<float> (sawSlope (-1, su, sd) - sawSlope (1, su, sd)));
                triRing.add (*table, d, 0.0f, static_cast<float> (-sd - su));
                dir = -1;
                recheck (t);
                break;
            }

            case Ev::TurnBottom:
            {
                v = turnLevel;
                const double before = saw (v, -1), after = saw (v, 1);
                sawRing.add (*table, d, static_cast<float> (after - before),
                             static_cast<float> (sawSlope (1, su, sd) - sawSlope (-1, su, sd)));
                triRing.add (*table, d, 0.0f, static_cast<float> (su + sd));
                dir = 1;
                resetD = d;
                recheck (t);
                break;
            }

            case Ev::HoldEnd:
                hold = 0.0; // released at 0 V, charging upwards
                sawRing.add (*table, d, 0.0f, static_cast<float> (sawSlope (1, su, sd)));
                triRing.add (*table, d, 0.0f, static_cast<float> (su));
                recheck (t);
                break;

            case Ev::Comparator:
                setComparator (! comparatorHigh, t);
                break;

            case Ev::FallDue:
                pendingFall = -1.0;
                if (pulseHigh)
                {
                    pulseRing.add (*table, d, -1.0f, 0.0f);
                    pulseHigh = false;
                }
                break;

            case Ev::Sync:
            {
                // Prophet-5 "conventional hard sync": the PNP pulls the timing capacitor to 0 V and keeps
                // it there until its base recovers; the core restarts charging upwards.
                syncPending = false;
                const bool held = hold > 0.0;
                const double sb = held ? 0.0 : saw (v, dir);
                const double tb = held ? 0.0 : v;
                const double ksb = held ? 0.0 : sawSlope (dir, su, sd);
                const double ktb = held ? 0.0 : (dir > 0 ? su : -sd);
                v = 0.0;
                dir = 1;
                hold = holdS;
                const double ksa = holdS > 0.0 ? 0.0 : sawSlope (1, su, sd);
                const double kta = holdS > 0.0 ? 0.0 : su;
                sawRing.add (*table, d, static_cast<float> (-sb), static_cast<float> (ksa - ksb));
                triRing.add (*table, d, static_cast<float> (-tb), static_cast<float> (kta - ktb));
                resetD = d;
                recheck (t);
                break;
            }

            case Ev::None:
                break;
        }
    }

    return pushOutputs();
}

} // namespace augur
