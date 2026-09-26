#pragma once

#include "Util/Random.h"

namespace augur
{

// Per-voice component tolerances, as unit-normal deviates. Generated once from a seed, so every
// instance of the synth is the same "unit" (same voice personalities) and renders are reproducible.
// The engine scales these by ANALOG AGE and VOICE DETUNE.
struct VoiceFingerprint
{
    float oscTune[2];     // base tuning error
    float oscScale[2];    // volts-per-octave scale error
    float curvature[2];   // ramp bow spread
    float deadTime[2];    // discharge time spread
    float hfTrim[2];      // HF trim calibration error
    float pwOffset[2];    // comparator offset
    float waveLevel[3];   // saw / tri / pulse level spread
    float dc[2];
    float triPeak;
    float cutoff;         // filter frequency offset
    float resonance;      // resonance gain spread
    float envTime[2];     // filter / amp envelope RC spread
    float vcaGain;
    float pan;

    static VoiceFingerprint generate (std::uint64_t seed) noexcept
    {
        Random r (seed);
        VoiceFingerprint f {};
        for (int i = 0; i < 2; ++i)
        {
            f.oscTune[i] = r.nextGaussian();
            f.oscScale[i] = r.nextGaussian();
            f.curvature[i] = r.nextGaussian();
            f.deadTime[i] = r.nextGaussian();
            f.hfTrim[i] = r.nextGaussian();
            f.pwOffset[i] = r.nextGaussian();
            f.dc[i] = r.nextGaussian();
            f.envTime[i] = r.nextGaussian();
        }
        for (auto& l : f.waveLevel)
            l = r.nextGaussian();
        f.triPeak = r.nextGaussian();
        f.cutoff = r.nextGaussian();
        f.resonance = r.nextGaussian();
        f.vcaGain = r.nextGaussian();
        f.pan = r.nextGaussian();
        return f;
    }
};

} // namespace augur
