#pragma once

#include "Engine/SynthParams.h"

#include <array>

namespace augur
{

// Everything a voice needs for one control chunk. Global signals (smoothed parameters, the shared
// LFO) are computed once per chunk for the whole chunk, so voices can render any sub-range of it and
// the result never depends on how the host splits its blocks.
struct ChunkSignals
{
    static constexpr int maxSamples = 128; // 32 host samples x 4 (DIVINE oversampling)

    using Buffer = std::array<float, maxSamples>;

    int numSamples = 0;
    double sampleRate = 96000.0; // internal (oversampled) rate

    Buffer lfo {};               // shared LFO, already scaled by LFO AMOUNT
    Buffer osc1Pw {}, osc2Pw {};
    Buffer saw1 {}, pulse1 {}, saw2 {}, tri2 {}, pulse2 {}; // smoothed waveform switches
    Buffer mix1 {}, mix2 {}, mixNoise {}, drive {};
    Buffer cutoffOct {}, resonance {}, envAmount {};
    Buffer pmFilterEnv {}, pmOsc2 {};
    Buffer mixRing {}, mixSub {}, crossMod {};

    double lfo2Inc = 0.0;        // LFO 2 cycles per internal sample
    double lfo2PhaseStart = 0.0; // free-running LFO 2 phase at the start of the chunk

    float modWheel = 0.0f;
    float channelPressure = 0.0f;
    float bendSemitones = 0.0f;
    float tuneSemitones = 0.0f;

    const SynthParams* params = nullptr;
};

} // namespace augur
