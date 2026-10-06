#pragma once

#include "Rack/FxCommon.h"
#include "Rack/RackRng.h"

#include <array>
#include <cstdint>

namespace augur::rack
{

enum class ReverbType
{
    plate = 0, // Dattorro's figure-eight plate tank (AES 1997)
    room,      // early reflections + small 8-line FDN
    hall,      // 8-line FDN (Jot): Householder matrix, per-line absorption for two-band RT60, modulated lines
    shimmer    // hall with a pitch shifter inside the feedback loop
};

struct ReverbParams
{
    ReverbType type = ReverbType::plate;
    double size = 1.0;       // 0.3 .. 2 (scales every delay)
    double decayS = 2.5;     // RT60 at low / mid frequencies, 0.2 .. 30 s
    double predelayMs = 10.0;
    double damping = 0.5;    // 0 bright .. 1 dark (high-frequency RT60 relative to the low one)
    double lowCutHz = 80.0;  // high-pass on the reverb return, 20 .. 1000
    double modulation = 0.4; // 0 .. 1 (delay modulation: chorused, non-metallic tail)
    double width = 1.0;      // 0 .. 1.5
    double shimmer = 0.5;    // SHIMMER: amount of pitch-shifted regeneration 0 .. 1
    double pitchSemis = 12.0;
    bool freeze = false;
    double mix = 0.25;
};

class Reverb
{
public:
    void prepare (double sampleRate, std::uint64_t seed);
    void reset() noexcept;
    void process (double& left, double& right, const ReverbParams& p) noexcept;

private:
    struct Allpass
    {
        DelayLine line;
        double process (double x, double delaySamples, double g) noexcept
        {
            const double d = line.read (std::max (1.0, delaySamples - 1.0)); // v[n - D] before writing v[n]
            const double v = x + g * d;
            line.write (v);
            return d - g * v;
        }
        // Integer-delay variant (no interpolation).
        double processInt (double x, int delaySamples, double g) noexcept
        {
            const double d = line.readInt (delaySamples - 1);
            const double v = x + g * d;
            line.write (v);
            return d - g * v;
        }
    };

    void plate (double inL, double inR, double& outL, double& outR, const ReverbParams& p, double decay, double dampCoef) noexcept;
    void fdn (double inL, double inR, double& outL, double& outR, const ReverbParams& p, bool room) noexcept;
    double pitchShift (double x, double semis) noexcept;

    double sr = 48000.0, scale = 1.0;
    Rng rng;
    DelayLine preL, preR;
    // plate
    std::array<Allpass, 4> inDiff;
    Allpass tankApL, tankApR, tankAp2L, tankAp2R;
    DelayLine tankDel1L, tankDel2L, tankDel1R, tankDel2R;
    double bwState = 0.0, dampL = 0.0, dampR = 0.0, tankFbL = 0.0, tankFbR = 0.0, lfoPhase = 0.0;
    // fdn
    static constexpr int lines = 8;
    std::array<DelayLine, lines> fdnLine;
    std::array<double, lines> fdnState {}, fdnLp {}, fdnPhase {};
    std::array<Allpass, 4> fdnDiff;
    DelayLine early;
    // shimmer
    DelayLine shiftLine;
    double shiftPhase = 0.0, shimmerFb = 0.0;
    OnePole shimmerLp, shimmerHp;
    // output
    OnePole lowCutL, lowCutR;
    Smooth mixS, widthS, sizeS;
    ReverbType lastType = ReverbType::plate;
};

} // namespace augur::rack
