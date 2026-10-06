#pragma once

#include "Effects/SpringReverb.h"
#include "Effects/TapeDelay.h"
#include "Rack/BusComp.h"
#include "Rack/Chorus.h"
#include "Rack/Drive.h"
#include "Rack/Flanger.h"
#include "Rack/Phaser.h"
#include "Rack/Reverb.h"
#include "Rack/TapeEcho.h"

#include <array>
#include <cstdint>

namespace augur::rack
{

// The TONAL LAB effects rack (shared with PYTHIA 32) plus AUGUR's own stereo delay and spring tank.
// Runs after the voices' decimator (AUGUR's FUZZ pedal sits before it, on the oversampled bus).
enum class FxId
{
    drive = 0,
    chorus,
    phaser,
    flanger,
    delay,
    echo,
    reverb,
    comp,
    count
};
inline constexpr int fxCount = static_cast<int> (FxId::count);
inline constexpr const char* fxNames[fxCount] { "DRIVE", "CHORUS", "PHASER", "FLANGER", "DELAY", "TAPE ECHO", "REVERB", "BUS COMP" };

// AUGUR's stereo delay: tape-like feedback path, tempo sync, ping-pong (send: dry + wet x mix).
struct DelayParams
{
    double timeSeconds = 0.375;
    double feedback = 0.35;
    double mix = 0.3;
    bool sync = false;
    int division = 6; // index into augur::delaySyncBeats
    bool pingPong = false;
};

struct FxParams
{
    std::array<int, fxCount> order { 0, 1, 2, 3, 4, 5, 6, 7 }; // processing order (effect ids)
    std::array<bool, fxCount> on {};
    DriveParams drive;
    ChorusParams chorus;
    PhaserParams phaser;
    bool phaserSync = false;
    int phaserDivision = 9;
    FlangerParams flanger;
    bool flangerSync = false;
    int flangerDivision = 12;
    DelayParams delay;
    TapeEchoParams echo;
    bool echoSync = false;
    int echoDivision = 6;
    bool echoHeadsOff = false; // selector position 12 of the classic unit: spring only
    double echoSpring = 0.0;   // spring tank inside the echo unit (selector positions 5..12)
    ReverbParams reverb;
    bool reverbSpring = false; // REVERB type SPRING: AUGUR's three-spring tank instead of the rack reverb
    BusCompParams comp;
};

// Each unit fades in / out over 10 ms when switched, costs nothing while off, and starts from silence when
// switched back on. The order of the units is free (FxParams::order).
class FxRack
{
public:
    void prepare (double sampleRate, std::uint64_t seed);
    void reset() noexcept;
    void process (double& left, double& right, const FxParams& p, double bpm) noexcept;

    // GUI read-outs (relaxed, audio -> GUI).
    float compGainReduction() const noexcept { return comp.gainReductionDb(); }
    double phaserHz() const noexcept { return phaser.sweepHz (0); }
    double flangerMs() const noexcept { return flanger.currentDelayMs (0); }
    double echoHeadMs (int h) const noexcept { return echo.headDelayMs (h); }

private:
    void runOne (FxId id, double& l, double& r, const FxParams& p, double bpm) noexcept;
    void resetOne (FxId id) noexcept;
    double springWet (SpringReverb& tank, double l, double r, double decay, double tension, double& outR) noexcept;

    Drive drive;
    Chorus chorus;
    Phaser phaser;
    Flanger flanger;
    TapeDelay delay;
    TapeEcho echo;
    SpringReverb echoSpring;
    Reverb reverb;
    SpringReverb springTank;
    BusComp comp;
    Smooth springMixS;
    std::array<double, fxCount> fade {};
    double fadeStep = 0.001;
};

} // namespace augur::rack
