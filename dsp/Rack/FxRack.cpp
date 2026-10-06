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
    fade.fill (0.0);
    fadeStep = 1.0 / (0.01 * sampleRate);
}

void FxRack::reset() noexcept
{
    for (int i = 0; i < fxCount; ++i)
        resetOne (static_cast<FxId> (i));
    fade.fill (0.0);
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
        case FxId::echo: echo.reset(); echoSpring.reset(); break;
        case FxId::reverb: reverb.reset(); springTank.reset(); break;
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

void FxRack::runOne (FxId id, double& l, double& r, const FxParams& p, double bpm) noexcept
{
    const auto beatsToHz = [bpm] (int division) { return bpm / 60.0 / syncBeats[std::clamp (division, 0, syncCount - 1)]; };
    switch (id)
    {
        case FxId::drive: drive.process (l, r, p.drive); break;
        case FxId::chorus: chorus.process (l, r, p.chorus); break;
        case FxId::phaser:
        {
            auto q = p.phaser;
            if (p.phaserSync)
                q.rateHz = beatsToHz (p.phaserDivision);
            phaser.process (l, r, q);
            break;
        }
        case FxId::flanger:
        {
            auto q = p.flanger;
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
            delay.process (&fl, &fr, 1, static_cast<float> (seconds), static_cast<float> (d.feedback), static_cast<float> (d.mix), d.pingPong);
            l = fl;
            r = fr;
            break;
        }
        case FxId::echo:
        {
            if (! p.echoHeadsOff)
            {
                auto q = p.echo;
                if (p.echoSync)
                    q.headMs = 60000.0 / bpm * syncBeats[std::clamp (p.echoDivision, 0, syncCount - 1)];
                echo.process (l, r, q);
            }
            // The unit's spring tank hears the input and the echoes (selector positions 5..12).
            if (p.echoSpring > 0.0)
            {
                double wr = 0.0;
                const double wl = springWet (echoSpring, l, r, 2.6, 0.5, wr);
                l += p.echoSpring * wl;
                r += p.echoSpring * wr;
            }
            break;
        }
        case FxId::reverb:
        {
            if (! p.reverbSpring)
            {
                reverb.process (l, r, p.reverb);
                break;
            }
            // SPRING: AUGUR's three-spring tank. SIZE sets the tension, DECAY its RT60; same equal-power MIX as the rack.
            const double tension = std::clamp ((p.reverb.size - 0.3) / 1.7, 0.0, 1.0);
            double wr = 0.0;
            const double wl = springWet (springTank, l, r, std::clamp (p.reverb.decayS, 0.3, 6.0), tension, wr);
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
    for (int slot = 0; slot < fxCount; ++slot)
    {
        const int idx = std::clamp (p.order[static_cast<size_t> (slot)], 0, fxCount - 1);
        const auto id = static_cast<FxId> (idx);
        auto& f = fade[static_cast<size_t> (idx)];
        const bool on = p.on[static_cast<size_t> (idx)];
        if (! on && f <= 0.0)
            continue;
        if (on && f <= 0.0)
            resetOne (id); // switched on from silence: clear stale state (old tails) before fading in
        f = std::clamp (f + (on ? fadeStep : -fadeStep), 0.0, 1.0);
        double l = left, r = right;
        runOne (id, l, r, p, bpm);
        left += f * (l - left);
        right += f * (r - right);
    }
}

} // namespace augur::rack
