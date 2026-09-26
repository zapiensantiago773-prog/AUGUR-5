#pragma once

#include <vector>

namespace augur
{

// Plate reverb after J. Dattorro, "Effect Design Part 1" (JAES 1997): band-limited input, four input
// diffusers, then a figure-of-eight tank of two halves (modulated all-pass, delay, damping, all-pass,
// delay) with the output taken from seven taps per side. Lengths are the paper's (29761 Hz) scaled
// to the running rate and by SIZE; DECAY is an RT60 in seconds.
class PlateReverb
{
public:
    void prepare (double sampleRate);
    void reset() noexcept;

    // Send-style: output = dry + wet * mix. size 0..1, decay = RT60 seconds.
    void process (float* left, float* right, int numSamples, float size, float decaySeconds, float mix) noexcept;

private:
    struct Line
    {
        std::vector<float> buf;
        int mask = 0, write = 0;
        void allocate (int maxLength);
        void clear() noexcept;
        void push (float x) noexcept
        {
            write = (write + 1) & mask;
            buf[static_cast<size_t> (write)] = x;
        }
        float tap (int delay) const noexcept { return buf[static_cast<size_t> ((write - delay) & mask)]; }
        float tapFrac (float delay) const noexcept;
    };

    // All-pass built on a Line: y = -g*x + d; line <- x + g*y.
    static float allpass (Line& line, int delay, float g, float x) noexcept;
    static float allpassFrac (Line& line, float delay, float g, float x) noexcept;

    int len (double paperSamples, float size) const noexcept;

    double sampleRate = 48000.0, scale = 1.0;
    Line pre, in1, in2, in3, in4;
    Line apL, delL1, ap2L, delL2;
    Line apR, delR1, ap2R, delR2;
    float bandwidthState = 0.0f, dampL = 0.0f, dampR = 0.0f;
    float feedL = 0.0f, feedR = 0.0f;
    double lfoPhase = 0.0;
};

} // namespace augur
