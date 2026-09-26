#pragma once

namespace augur
{

// Stereo phaser: six first-order all-pass stages swept exponentially between ~120 Hz and ~4 kHz by a
// shared LFO (the right channel's sweep is offset by a quarter cycle for width), with feedback
// around the chain, like the classic OTA/JFET phasers. Sweep and feedback are smoothed per sample.
class Phaser
{
public:
    static constexpr int numStages = 6;

    void prepare (double sampleRate) noexcept;
    void reset() noexcept;

    // rateHz 0.02..10, depth 0..1 (sweep width), feedback 0..0.9, mix 0..1 (0.5 = classic notch depth).
    void process (float* left, float* right, int numSamples, float rateHz, float depth, float feedback, float mix) noexcept;

private:
    struct Channel
    {
        float state[numStages] {};
        float last = 0.0f;
        float process (float x, float a, float feedback) noexcept;
    };

    double sampleRate = 48000.0;
    double phase = 0.0;
    float piOverFs = 0.0f;
    Channel left, right;
};

} // namespace augur
