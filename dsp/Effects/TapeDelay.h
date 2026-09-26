#pragma once

#include <vector>

namespace augur
{

// Stereo delay with a tape-like feedback path: band-limited (HP 90 Hz, LP 5.5 kHz), soft saturation
// and slight cross-feed for width. Delay-time changes glide (pitch bend like tape) instead of clicking.
class TapeDelay
{
public:
    static constexpr double maxSeconds = 2.5;

    void prepare (double sampleRate);
    void reset() noexcept;

    // Send-style: output = dry + wet * mix.
    void process (float* left, float* right, int numSamples, float timeSeconds, float feedback, float mix) noexcept;

private:
    float read (const std::vector<float>& buf, float delaySamples) const noexcept;

    std::vector<float> bufL, bufR;
    int writePos = 0;
    int mask = 0;
    double sampleRate = 48000.0;
    float smoothedDelay = 0.0f, delayCoeff = 0.0f;
    float lpCoeff = 0.5f, hpCoeff = 0.01f;
    float lpL = 0.0f, lpR = 0.0f, hpL = 0.0f, hpR = 0.0f;
};

} // namespace augur
