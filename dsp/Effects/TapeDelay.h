#pragma once

#include "Util/Lr4.h"

#include <vector>

namespace augur
{

// Stereo delay with a tape-like feedback path: band-limited (HP 90 Hz, LP 5.5 kHz), soft saturation
// and slight cross-feed for width. Delay-time changes glide (pitch bend like tape) instead of clicking; MIX and
// FEEDBACK glide over 20 ms (no zipper when automated). The repeats' bass comes out in the middle, the ping-pong and
// the width stay above stereoBassHz (D-057).
class TapeDelay
{
public:
    static constexpr double maxSeconds = 2.5;

    void prepare (double sampleRate);
    void reset() noexcept;

    // Send-style: output = dry + wet * mix. Ping-pong: the input enters the left line only and each
    // repeat crosses to the other side.
    void process (float* left, float* right, int numSamples, float timeSeconds, float feedback, float mix,
                  bool pingPong = false) noexcept;

private:
    float read (const std::vector<float>& buf, float delaySamples) const noexcept;

    std::vector<float> bufL, bufR;
    int writePos = 0;
    int mask = 0;
    double sampleRate = 48000.0;
    float smoothedDelay = 0.0f, delayCoeff = 0.0f;
    float lpCoeff = 0.5f, hpCoeff = 0.01f;
    float lpL = 0.0f, lpR = 0.0f, hpL = 0.0f, hpR = 0.0f;
    float mixNow = 0.0f, fbNow = 0.0f, paramCoeff = 1.0f;
    bool paramsFresh = true; // after prepare / reset the first values are taken as they are
    MonoBass monoBass;
};

} // namespace augur
