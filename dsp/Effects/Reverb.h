#pragma once

#include <array>
#include <vector>

namespace augur
{

// 8-line feedback delay network with a Hadamard mixing matrix, per-line damping and exact RT60
// gains (g = 10^(-3 * L / (RT60 * fs))). Two lines are gently modulated to avoid metallic ringing.
class Reverb
{
public:
    static constexpr int numLines = 8;

    void prepare (double sampleRate);
    void reset() noexcept;

    // Send-style: output = dry + wet * mix. size 0..1, decay = RT60 seconds.
    void process (float* left, float* right, int numSamples, float size, float decaySeconds, float mix) noexcept;

private:
    struct Line
    {
        std::vector<float> buf;
        int mask = 0;
        float lp = 0.0f;
    };

    std::array<Line, numLines> lines;
    std::vector<float> preDelay;
    int preMask = 0;
    int writePos = 0;
    double sampleRate = 48000.0;
    double modPhase = 0.0;
    float dampCoeff = 0.5f;
};

} // namespace augur
