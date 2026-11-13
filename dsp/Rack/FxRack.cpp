#include "Rack/FxRack.h"

#include "Engine/SynthParams.h"

#include <cmath>

namespace augur::rack
{

void FxRack::prepare (double sampleRate, std::uint64_t seed)
{
    drive.prepare (sampleRate);
    chorus.prepare (sampleRate, seed ^ 0x11u);
    phaser.prepare (sampleRate);
    flanger.prepare (sampleRate);
    delay.prepare (sampleRate);
    echo.prepare (sampleRate, seed ^ 0x22u);
    echoSpring.prepare (sampleRate, seed ^ 0x44u);
    reverb.prepare (sampleRate, seed ^ 0x33u);
    springTank.prepare (sampleRate, seed ^ 0x55u);
    comp.prepare (sampleRate);
    springMixS.prepare (sampleRate, 0.02);
    echoSpringS.prepare (sampleRate, 0.02);
    echoSpringBass.prepare (sampleRate);
    tankBass.prepare (sampleRate);
    fade.fill (0.0);
    fadeStep = 1.0 / (0.01 * sampleRate);
    preRollSamples = std::max (2, static_cast<int> (0.01 * sampleRate));
    preRoll.fill (0);
    modeSwap.fill (false);
    fresh = true;
}

void FxRack::reset() noexcept
{
    for (int i = 0; i < fxCount; ++i)
        resetOne (static_cast<FxId> (i));
    fade.fill (0.0);
    modeSwap.fill (false);
    preRoll.fill (0);
    fresh = true;
}

void FxRack::resetOne (FxId id) noexcept
{
    switch (id)
    {
        case FxId::drive: drive.reset(); break;
        case FxId::chorus: chorus.reset(); break;
        case FxId::phaser: phaser.reset(); break;
        case FxId::flanger: flanger.reset(); break;
        case FxId::delay: delay.reset(); break;
        case FxId::echo: echo.reset(); echoSpring.reset(); echoSpringBass.reset(); break;
        case FxId::reverb: reverb.reset(); springTank.reset(); tankBass.reset(); break;
        case FxId::comp: comp.reset(); break;
        case FxId::count: break;
    }
}

double FxRack::springWet (SpringReverb& tank, double l, double r, double decay, double tension, double& outR) noexcept
{
    const float in = static_cast<float> (0.5 * (l + r));
    float wl = 0.0f, wr = 0.0f;
    tank.process (&in, &wl, &wr, 1, static_cast<float> (decay), static_cast<float> (tension), 1.0f);
    outR = wr;
    return wl;
}

int FxRack::modeOf (FxId id, const FxParams& p) noexcept
{
    switch (id)
    {
        case FxId::drive: return static_cast<int> (p.drive.model);
        case FxId::chorus: return static_cast<int> (p.chorus.mode);
        case FxId::phaser: return p.phaser.stages * 4 + static_cast<int> (p.phaser.lfo);
        case FxId::flanger: return p.flanger.throughZero ? 1 : 0;
        case FxId::delay: return p.delay.pingPong ? 1 : 0;
        case FxId::echo: return std::clamp (p.echo.mode, 0, 6) + (p.echoHeadsOff ? 8 : 0);
        case FxId::reverb: return static_cast<int> (p.reverb.type) + (p.reverbSpring ? 8 : 0);
        case FxId::comp:
        case FxId::count: break;
    }
    return 0;
}

void FxRack::runOne (FxId id, double& l, double& r, const FxParams& p, double bpm, int mode) noexcept
{
    const auto beatsToHz = [bpm] (int division) { return bpm / 60.0 / syncBeats[std::clamp (division, 0, syncCount - 1)]; };
    switch (id)
    {
        case FxId::drive:
        {
            auto q = p.drive;
            q.model = static_cast<DriveModel> (mode);
            drive.process (l, r, q);
            break;
        }
        case FxId::chorus:
        {
            auto q = p.chorus;
            q.mode = static_cast<ChorusMode> (mode);
            chorus.process (l, r, q);
            break;
        }
        case FxId::phaser:
        {
            auto q = p.phaser;
            q.stages = mode / 4;
            q.lfo = static_cast<PhaserLfo> (mode % 4);
            if (p.phaserSync)
                q.rateHz = beatsToHz (p.phaserDivision);
            phaser.process (l, r, q);
            break;
        }
        case FxId::flanger:
        {
            auto q = p.flanger;
            q.throughZero = mode != 0;
            if (p.flangerSync)
                q.rateHz = beatsToHz (p.flangerDivision);
            flanger.process (l, r, q);
            break;
        }
        case FxId::delay:
        {
            const auto& d = p.delay;
            const double seconds = d.sync ? delaySyncBeats[static_cast<size_t> (std::clamp (d.division, 0, 11))] * 60.0 / bpm : d.timeSeconds;
            float fl = static_cast<float> (l), fr = static_cast<float> (r);
            delay.process (&fl, &fr, 1, static_cast<float> (seconds), static_cast<float> (d.feedback), static_cast<float> (d.mix), mode != 0);
            l = fl;
            r = fr;
            break;
        }
        case FxId::echo:
        {
            if (mode < 8)
            {
                auto q = p.echo;
                q.mode = mode;
                if (p.echoSync)
                    q.headMs = 60000.0 / bpm * syncBeats[std::clamp (p.echoDivision, 0, syncCount - 1)];
                echo.process (l, r, q);
            }
            // The unit's spring tank hears the input and the echoes (selector positions 5..12).
            const double springLevel = echoSpringS.next (std::clamp (p.echoSpring, 0.0, 1.0));
            if (springLevel > 1.0e-6)
            {
                double wr = 0.0;
                double wl = springWet (echoSpring, l, r, 2.6, 0.5, wr);
                echoSpringBass.process (wl, wr);
                l += springLevel * wl;
                r += springLevel * wr;
            }
            break;
        }
        case FxId::reverb:
        {
            if (mode < 8)
            {
                auto q = p.reverb;
                q.type = static_cast<ReverbType> (mode);
                reverb.process (l, r, q);
                break;
            }
            // SPRING: AUGUR's three-spring tank. SIZE sets the tension, DECAY its RT60; same equal-power MIX as the rack.
            const double tension = std::clamp ((p.reverb.size - 0.3) / 1.7, 0.0, 1.0);
            double wr = 0.0;
            double wl = springWet (springTank, l, r, std::clamp (p.reverb.decayS, 0.3, 6.0), tension, wr);
            tankBass.process (wl, wr);
            const double mix = springMixS.next (std::clamp (p.reverb.mix, 0.0, 1.0));
            const double gd = std::cos (0.5 * pi * mix), gw = std::sin (0.5 * pi * mix);
            l = gd * l + gw * 1.6 * wl;
            r = gd * r + gw * 1.6 * wr;
            break;
        }
        case FxId::comp: comp.process (l, r, p.comp); break;
        case FxId::count: break;
    }
}

void FxRack::process (double& left, double& right, const FxParams& p, double bpm) noexcept
{
    bpm = std::clamp (bpm, 20.0, 999.0);
    if (! std::isfinite (left) || ! std::isfinite (right))
        left = right = 0.0; // never let a bad sample into the delay lines
    if (fresh)
    {
        // Just prepared or reset (a new sound, a new session): every unit is empty and silent, and the engine brings
        // the sound in under its own fade. The units that are on are simply on, from the first sample; fading them
        // in here left the first 10 ms of a new sound's first note without its effects.
        fresh = false;
        for (int i = 0; i < fxCount; ++i)
        {
            const auto ui = static_cast<size_t> (i);
            fade[ui] = p.on[ui] ? 1.0 : 0.0;
            heldMode[ui] = modeOf (static_cast<FxId> (i), p);
        }
    }
    for (int slot = 0; slot < fxCount; ++slot)
    {
        const int idx = std::clamp (p.order[static_cast<size_t> (slot)], 0, fxCount - 1);
        const auto id = static_cast<FxId> (idx);
        auto& f = fade[static_cast<size_t> (idx)];
        auto& held = heldMode[static_cast<size_t> (idx)];
        auto& swapping = modeSwap[static_cast<size_t> (idx)];
        const bool on = p.on[static_cast<size_t> (idx)];
        const int want = modeOf (id, p);
        auto& roll = preRoll[static_cast<size_t> (idx)];
        if (! on && f <= 0.0)
        {
            swapping = false;
            roll = 0;
            continue;
        }
        if (f <= 0.0 && roll == 0)
        {
            // Faded out. Switched on from silence: clear stale state (old tails) before fading in. At the bottom of a
            // mode change the unit keeps what it holds (the tape and the delay lines keep their echoes).
            // DRIVE, CHORUS and REVERB start a new circuit from rest themselves. Whatever starts empty gets the pre-roll
            // below; a unit that keeps its memory must keep hearing its input whole (a gap would be a step on the tape).
            bool cleared = id == FxId::drive || id == FxId::chorus;
            if (! swapping || id == FxId::reverb)
            {
                resetOne (id); // (REVERB: another tank starts empty, not with the tail it held the last time it was chosen)
                cleared = true;
            }
            else if (id == FxId::echo && ((held ^ want) & 8) != 0)
            {
                echo.reset(); // the heads come back on (or go): the tape stood still meanwhile, no stale echoes
                cleared = true;
            }
            swapping = false;
            held = want;
            roll = cleared ? preRollSamples : 0;
        }
        // A selector moved while the unit sounds (a new circuit, other heads, another tank): switching it in place
        // clicked (second difference 6 .. 100 times the signal's own). The unit fades out on the mode it has, changes
        // in silence and fades back in: 10 ms each way. Before it fades in (also when it is switched on) it runs 10 ms
        // unheard while its input rises from nothing: a signal cut in mid-cycle into empty delay lines was a step that
        // every tap, head and repeat played back later (the echo's first repeat, the reverb's early reflections).
        const bool change = held != want;
        swapping = swapping || change;
        double inGain = 1.0;
        if (roll > 0 && ! change && --roll > 0)
            inGain = 1.0 - static_cast<double> (roll) / preRollSamples; // filling up: still silent
        else
        {
            roll = 0;
            f = std::clamp (f + (on && ! change ? fadeStep : -fadeStep), 0.0, 1.0);
        }
        double l = left * inGain, r = right * inGain;
        runOne (id, l, r, p, bpm, held);
        left += f * (l - left);
        right += f * (r - right);
    }
    if (! std::isfinite (left) || ! std::isfinite (right))
    {
        reset();
        left = right = 0.0;
    }
}

} // namespace augur::rack
