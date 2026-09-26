#pragma once

#include "Util/Random.h"

#include <array>
#include <vector>

namespace augur
{

// Spring reverb tank: three springs, each a feedback loop around a dispersive chain (after Parker &
// Valimaki's spring model): a cascade of stretched first-order all-passes
//     H(z) = (a + z^-K) / (1 + a z^-K)
// delays low frequencies more than high ones, which turns every transient into the rising "chirp"
// of a real spring. The loop adds the spring's round-trip delay, a 4.5 kHz roll-off and a slow random
// drift of the delay (no metallic stationarity). Springs 1 and 2 feed left and right, spring 3 both.
class SpringReverb
{
public:
    static constexpr int numSprings = 3;
    static constexpr int chainLength = 48;

    void prepare (double sampleRate, std::uint64_t seed = 0x5BB1u);
    void reset() noexcept;

    // Adds the reverb of `input` (mono) into outL/outR, scaled by `level`. decaySeconds ~ 1.5..4 s
    // like real tanks; tension 0..1 shortens the springs (brighter, faster chirps, denser).
    void process (const float* input, float* outL, float* outR, int numSamples, float decaySeconds, float tension,
                  float level) noexcept;

private:
    struct Spring
    {
        std::vector<float> loop;          // round-trip delay line
        int loopMask = 0, write = 0;
        float baseDelay = 2000.0f;        // samples
        std::vector<float> apState;       // chain states: chainLength x K (ring per stage)
        int apPos = 0;
        float lp = 0.0f, lp2 = 0.0f;
        float drift = 0.0f, driftTarget = 0.0f;
    };

    float processSpring (Spring& s, float x, float feedback, float delaySamples) noexcept;

    double sampleRate = 48000.0;
    int stretch = 6;          // K: dispersion band up to ~fs / (2K)
    float apCoeff = 0.62f;
    float loopLpCoeff = 0.4f, driftCoeff = 0.0f, inHpCoeff = 0.02f, outLpCoeff = 0.5f;
    std::array<Spring, numSprings> springs;
    float inHp = 0.0f;
    std::array<float, 2> outLp {};
    Random rng;
    int driftCounter = 0;
};

} // namespace augur
