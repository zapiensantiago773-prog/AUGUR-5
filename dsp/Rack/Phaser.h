#pragma once

#include "Rack/FxCommon.h"

#include <array>

namespace augur::rack
{

enum class PhaserLfo
{
    sine = 0,
    triangle,
    envelope // the input level sweeps the notches (envelope phaser)
};

struct PhaserParams
{
    int stages = 4;          // 4 (Phase 90 / Small Stone), 6 (Bi-Phase), 8, 12
    double rateHz = 0.5;     // 0.02 .. 10 Hz (or a note value when synced)
    double depth = 0.8;      // sweep span 0 .. 1 (up to ~5 octaves)
    double centreHz = 600.0; // MANUAL: centre of the sweep (100 Hz .. 5 kHz)
    double feedback = 0.0;   // -0.95 .. 0.95 (COLOR / regeneration; negative = inverted)
    double spreadDeg = 90.0; // phase offset of the right channel's LFO, 0 .. 180
    PhaserLfo lfo = PhaserLfo::triangle;
    double mix = 0.5;        // 0.5 = the classic equal dry + shifted sum (deepest notches)
};

// Analog phase shifter: a cascade of first-order all-pass stages whose break frequency the LFO sweeps
// exponentially (JFET / OTA control), optional regeneration around the cascade (Small Stone COLOR), dry +
// shifted signal summed. Stereo: two cascades with offset LFO phases.
class Phaser
{
public:
    void prepare (double sampleRate);
    void reset() noexcept;
    void process (double& left, double& right, const PhaserParams& p) noexcept;
    // Current centre of the sweep per channel (GUI).
    double sweepHz (int ch) const noexcept { return lastHz[static_cast<size_t> (ch)]; }

private:
    static constexpr int maxStages = 12;
    struct Chain
    {
        std::array<double, maxStages> s {};
        double fb = 0.0;
    };
    double run (Chain& c, double x, double g, int stages, double feedback) noexcept;

    double sr = 48000.0, phase = 0.0, env = 0.0, envCoeffA = 0.0, envCoeffR = 0.0;
    std::array<Chain, 2> chain;
    std::array<double, 2> lastHz { 600.0, 600.0 };
    Smooth mixS, fbS, depthS, centreS;
};

} // namespace augur::rack
