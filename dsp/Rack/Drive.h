#pragma once

#include "Rack/FxCommon.h"

#include <array>

namespace augur::rack
{

enum class DriveModel
{
    tube = 0, // 12AX7 common-cathode stage (Koren triode equations)
    diode,    // op-amp + diodes-in-feedback clipper (Tube Screamer clipping stage)
    tape,     // magnetic tape: anhysteretic (biased) <-> Jiles-Atherton hysteresis (under-biased)
    fold,     // parallel-cell wavefolder (Buchla 259 lineage)
    crush     // bit depth / sample-rate reduction
};

struct DriveParams
{
    DriveModel model = DriveModel::tube;
    double driveDb = 12.0; // 0..48 dB into the stage (CRUSH: amount of reduction 0..1 mapped from it)
    double bias = 0.0;     // -1..1: tube operating point / diode asymmetry / tape bias / fold symmetry
    double tone = 0.5;     // 0 dark .. 1 bright (CRUSH: target sample rate)
    double outputDb = 0.0; // -24..+12
    double mix = 1.0;
};

// Stereo saturation stage, 4x oversampled for every model except CRUSH (whose aliasing is the effect).
// Level-compensated so DRIVE changes the character more than the loudness.
class Drive
{
public:
    void prepare (double sampleRate);
    void reset() noexcept;
    void process (double& left, double& right, const DriveParams& p) noexcept;

private:
    double shape (double x, int ch, const DriveParams& p) noexcept;
    double tube (double x, double bias) const noexcept;
    double diode (double x, double bias, double gain, int ch) noexcept;
    double tape (double x, int ch) noexcept;
    double fold (double x, double bias) const noexcept;

    double sr = 48000.0, osRate = 192000.0;
    std::array<HalfbandFir, 2> upA, upB, downA, downB; // host<->2x (A), 2x<->4x (B), per channel
    std::array<DelayLine, 2> dryDelay;                 // aligns the dry path with the oversampling latency
    double dryLatency = 0.0;
    DriveModel lastModel = DriveModel::tube;
    std::array<OnePole, 2> preHp, toneLp, diodeHp;
    std::array<double, 2> tapeM {}, tapeH {}, crushHold {}, crushPhase {};
    Smooth gainIn, gainOut, mixS, toneS, biasS;
    double tubeNorm = 1.0, stageGain = 1.0;
};

} // namespace augur::rack
