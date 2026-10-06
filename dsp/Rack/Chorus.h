#pragma once

#include "Rack/FxCommon.h"
#include "Rack/RackRng.h"

#include <array>
#include <cstdint>

namespace augur::rack
{

enum class ChorusMode
{
    juno1 = 0, // Juno-60 I:    0.513 Hz triangle, 1.66 .. 5.35 ms
    juno2,     // Juno-60 II:   0.863 Hz triangle, same span
    juno12,    // Juno-60 I+II: 9.75 Hz sine-like, 3.3 .. 3.7 ms (vibrato-chorus)
    dimension, // Dimension D: trapezoidal LFO, 8 .. 12 ms, inverted cross-feed through a high-pass
    ensemble   // string-machine ensemble: 3 BBDs, two 3-phase LFOs (chorus + vibrato)
};

struct ChorusParams
{
    ChorusMode mode = ChorusMode::juno1;
    double rate = 1.0;  // multiplier of the mode's own rate (0.25 .. 4, 1 = original)
    double depth = 1.0; // multiplier of the mode's own sweep (0 .. 2, 1 = original)
    double tone = 0.5;  // BBD bandwidth (dark .. bright)
    double hiss = 0.2;  // BBD / compander noise (the Juno chorus hiss), 0 .. 1
    double width = 1.0; // stereo spread 0 .. 1.5
    double mix = 0.5;
};

// Bucket-brigade choruses of three classic machines. Each delay line is a BBD model: clock-tracking
// anti-alias / reconstruction low-passes, a soft compander limit and a little hiss.
class Chorus
{
public:
    void prepare (double sampleRate, std::uint64_t seed);
    void reset() noexcept;
    void process (double& left, double& right, const ChorusParams& p) noexcept;

private:
    struct Bbd
    {
        DelayLine line;
        Svf pre, post;
        double run (double in, double delayMs, double sr, double toneHz, double hiss, Rng& rng) noexcept;
    };
    double triangle (double ph) const noexcept { return ph < 0.5 ? 4.0 * ph - 1.0 : 3.0 - 4.0 * ph; }
    double trapezoid (double ph) const noexcept;

    double sr = 48000.0;
    Rng rng;
    std::array<Bbd, 3> bbd;
    double phase = 0.0, vibPhase = 0.0;
    OnePole crossHpL, crossHpR, bassL, bassR;
    Smooth mixS, depthS, widthS, rateS;
    ChorusMode lastMode = ChorusMode::juno1;
};

} // namespace augur::rack
