#include "Oscillator/Vco.h"

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

void Vco::prepare (double newSampleRate, std::uint64_t seed) noexcept
{
    table = &BlepTable::get();
    sampleRate = newSampleRate;
    invSampleRate = 1.0 / newSampleRate;
    rng.setSeed (seed);
    ring.fill (0.0f);
    writeIndex = 0;
    phase = rng.nextFloat(); // free-running: every voice starts somewhere different
    dead = 0.0;
    jitter = 1.0;
    resetD = -1.0f;
}

void Vco::setPhase (double newPhase) noexcept
{
    phase = std::clamp (newPhase, 0.0, 0.999999);
    dead = 0.0;
}

double Vco::phiFromC (double c) const noexcept
{
    const double k = character.curvature;
    if (std::abs (k) < 1.0e-6)
        return c;
    const double b = 1.0 + k;
    return (b - std::sqrt (std::max (0.0, b * b - 4.0 * k * c))) / (2.0 * k);
}

Vco::WaveState Vco::evaluate (double ph, bool inDead, double inc, float w, float p) const noexcept
{
    const double k = character.curvature;
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

void Vco::addEvent (float d, float step, float slope) noexcept
{
    if (step == 0.0f && slope == 0.0f)
        return;
    table->accumulate (ring.data(), ringMask, writeIndex - static_cast<unsigned> (latencySamples), d, step, slope);
}

float Vco::process (float freqHz, float pw, float gSaw, float gTri, float gPulse, float syncD) noexcept
{
    const float ks = gSaw * character.sawLevel;
    const float kt = gTri * character.triLevel;
    const float kp = gPulse * character.pulseLevel;

    const double f = std::max (0.0, static_cast<double> (freqHz));
    const double deadSeconds = character.deadTimeSeconds;
    // The ramp must run faster than 1/f by the (trimmed) dead time so the total period stays 1/f.
    const double denom = std::max (0.25, 1.0 - f * deadSeconds * character.hfTrim);
    const double inc = std::min (0.45, f * invSampleRate / denom * jitter);
    const double deadSamples = deadSeconds * sampleRate;

    const float w = std::clamp (pw + character.pwOffset, 0.02f, 0.98f);
    const float p = std::clamp (character.triPeak, 0.05f, 0.95f);
    // Thresholds only matter for waveforms that are on (their events are skipped otherwise).
    const bool usePulse = kp != 0.0f;
    const bool useTri = kt != 0.0f;
    const double phiW = usePulse ? phiFromC (w) : 2.0;
    const double phiP = useTri ? phiFromC (p) : 2.0;
    const double invInc = inc > 0.0 ? 1.0 / inc : 0.0;
    const double k = character.curvature;

    bool syncPending = syncD >= 0.0f;
    const double syncT = syncPending ? 1.0 - static_cast<double> (syncD) : 2.0;
    resetD = -1.0f;

    const auto value = [&] (const WaveState& s) { return ks * s.saw + kt * s.tri + kp * s.pulse; };
    const auto slope = [&] (const WaveState& s) { return ks * s.dsaw + kt * s.dtri; };

    double t = 0.0;
    for (int guard = 0; guard < 16; ++guard)
    {
        double tNext = 1.0;
        Event ev = Event::None;

        if (dead > 0.0)
        {
            const double tc = t + dead;
            if (tc < tNext)
            {
                tNext = tc;
                ev = Event::DeadEnd;
            }
        }
        else if (inc > 0.0)
        {
            if (phase < phiW)
            {
                const double tc = t + (phiW - phase) * invInc;
                if (tc < tNext) { tNext = tc; ev = Event::PwEdge; }
            }
            if (phase < phiP)
            {
                const double tc = t + (phiP - phase) * invInc;
                if (tc < tNext) { tNext = tc; ev = Event::TriPeak; }
            }
            const double tw = phase >= 1.0 ? t : t + (1.0 - phase) * invInc;
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
            phase += (tNext - t) * inc;
        t = tNext;

        if (ev == Event::None)
            break;

        const float d = static_cast<float> (std::clamp (1.0 - t, 0.0, 1.0));
        switch (ev)
        {
            case Event::PwEdge:
                phase = phiW;
                addEvent (d, -2.0f * kp, 0.0f);
                break;

            case Event::TriPeak:
            {
                phase = phiP;
                const double dc = (1.0 + k * (1.0 - 2.0 * phiP)) * inc;
                const double before = 2.0 * dc / p;
                const double after = -2.0 * dc / (1.0 - p);
                addEvent (d, 0.0f, kt * static_cast<float> (after - before));
                break;
            }

            case Event::Wrap:
            case Event::Sync:
            {
                const auto before = evaluate (std::min (phase, 1.0), dead > 0.0, inc, w, p);
                phase = 0.0;
                dead = deadSamples;
                if (ev == Event::Wrap)
                    jitter = 1.0 + static_cast<double> (character.cycleJitter * rng.nextGaussian());
                else
                    syncPending = false;
                const auto after = evaluate (0.0, dead > 0.0, inc, w, p);
                addEvent (d, value (after) - value (before), slope (after) - slope (before));
                resetD = d;
                break;
            }

            case Event::DeadEnd:
            {
                dead = 0.0;
                addEvent (d, 0.0f, slope (evaluate (0.0, false, inc, w, p)));
                break;
            }

            case Event::None:
                break;
        }
    }

    const auto s = evaluate (std::min (phase, 1.0), dead > 0.0, inc, w, p);
    ring[writeIndex & ringMask] += value (s) + character.dcOffset;

    const unsigned outIndex = (writeIndex - static_cast<unsigned> (latencySamples)) & ringMask;
    const float out = ring[outIndex];
    ring[outIndex] = 0.0f;
    ++writeIndex;
    return out;
}

} // namespace augur
