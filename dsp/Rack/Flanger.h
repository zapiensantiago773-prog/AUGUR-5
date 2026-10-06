#pragma once

#include "Rack/FxCommon.h"

#include <array>

namespace augur::rack
{

struct FlangerParams
{
    double rateHz = 0.25;   // 0.01 .. 10 Hz (or a note value when synced)
    double depth = 0.7;     // sweep span 0 .. 1
    double manualMs = 2.0;  // centre delay 0.1 .. 10 ms
    double feedback = 0.5;  // -0.95 .. 0.95 (regeneration; negative = hollow, "jet" inverted)
    bool throughZero = false;
    double spreadDeg = 90.0;
    double mix = 0.5;
};

// Flanger in two flavours:
//  - BBD (Electric Mistress / A/DA lineage): a short modulated delay with regeneration, summed with dry;
//  - through-zero (two-machine tape flanging): the dry path is held at a fixed delay and the swept path
//    crosses it, so the relative delay passes through zero and the comb collapses into a full cancellation.
class Flanger
{
public:
    void prepare (double sampleRate);
    void reset() noexcept;
    void process (double& left, double& right, const FlangerParams& p) noexcept;
    double currentDelayMs (int ch) const noexcept { return lastMs[static_cast<size_t> (ch)]; }

private:
    double sr = 48000.0, phase = 0.0;
    std::array<DelayLine, 2> wetLine, dryLine;
    std::array<double, 2> fb {}, lastMs { 2.0, 2.0 };
    std::array<OnePole, 2> damp;
    Smooth mixS, fbS, depthS, manualS;
};

} // namespace augur::rack
