#pragma once

#include <vector>

namespace augur
{

// Stereo BBD-style chorus: mono input into one modulated delay line, read with opposite modulation
// on each side (the classic "two BBDs in anti-phase" topology), with the BBD's band-limit and
// a touch of saturation on the wet path.
class Chorus
{
public:
    void prepare (double sampleRate);
    void reset() noexcept;

    // mix 0..1 (0 = dry only). In-place on L/R.
    void process (float* left, float* right, int numSamples, float rateHz, float depth, float mix) noexcept;

private:
    float read (float delaySamples) const noexcept;

    std::vector<float> buffer;
    int writePos = 0;
    int mask = 0;
    double sampleRate = 48000.0;
    double phase = 0.0;
    float lpL = 0.0f, lpR = 0.0f, lpCoeff = 0.5f;
};

} // namespace augur
