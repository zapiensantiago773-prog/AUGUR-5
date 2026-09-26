#pragma once

#include "Util/Random.h"

#include <array>

namespace augur
{

// Per-voice component tolerances, as unit-normal deviates. Generated once from a seed, so every
// instance of the synth is the same "unit" (same voice personalities) and renders are reproducible.
// The voice maps them to physical tolerances, scaled by ANALOG AGE and VOICE DETUNE.
struct VoiceFingerprint
{
    static constexpr int oscDeviates = 16;

    std::array<std::array<float, oscDeviates>, 2> osc {}; // per VCO chip (meaning assigned in SynthVoice)
    float oscDetune[2] {};  // deliberate per-voice detune (VOICE DETUNE)
    float cutoff = 0.0f;    // filter frequency offset
    float resonance = 0.0f; // resonance gain spread
    float envTime[2] {};    // filter / amp envelope RC spread
    float vcaGain = 0.0f;
    float pan = 0.0f;

    static VoiceFingerprint generate (std::uint64_t seed) noexcept
    {
        Random r (seed);
        VoiceFingerprint f {};
        for (auto& chip : f.osc)
            for (auto& d : chip)
                d = r.nextGaussian();
        for (auto& d : f.oscDetune)
            d = r.nextGaussian();
        f.cutoff = r.nextGaussian();
        f.resonance = r.nextGaussian();
        for (auto& d : f.envTime)
            d = r.nextGaussian();
        f.vcaGain = r.nextGaussian();
        f.pan = r.nextGaussian();
        return f;
    }
};

} // namespace augur
