#pragma once

#include "Oscillator/BlepRing.h"
#include "Util/Random.h"

#include <array>
#include <cstdint>

namespace augur
{

// Tolerances of one CEM3340 as fitted in a Prophet-5 Rev 3 voice (see docs/prophet5_vco_analysis.md).
struct Cem3340Unit
{
    ExpoConverter expo;
    float asymmetry = 0.0f;          // (I_down - I_up) / I: triangle symmetry 45..55 % in the datasheet
    float comparatorDelay = 100e-9f; // seconds; triangle overshoots its thresholds while the comparator switches
    float converterGain = 1.0f;      // gain of the tri->saw converter's inverting branch
    float converterOffset = 0.0f;    // offset of that branch (saw units, 1 = 10 V)
    float syncHold = 4.4e-6f;        // Prophet sync: PNP base recovery keeps the core at 0 V this long
    float pulseFallDelay = 0.5e-6f;  // half of the slower pulse fall time ("finite comparator gain")
    float pwOffset = 0.0f;           // PWM comparator offset (pulse-width units)
    float sawLevel = 1.0f, triLevel = 1.0f, pulseLevel = 1.0f;
};

// Triangle-core VCO modelled on the CEM3340 block diagram: the timing capacitor is charged and discharged
// between 0 V and Vcc/3 by the exponential current; a tri->saw converter folds the falling half up to
// make the 0..10 V sawtooth; the pulse is a comparator between saw/2 and the PWM CV. The core is
// simulated in continuous time between events and every event is placed at its exact sub-sample time
// (BLEP/BLAMP), including audio-rate PWM (the PW CV is interpolated with a Catmull-Rom cubic, one sample
// late), the Prophet's capacitor-discharge sync and the comparator-delay overshoot that makes the top
// octaves go flat.
class Cem3340Vco
{
public:
    static constexpr int latencySamples = BlepRing::latency;

    void prepare (double sampleRate, std::uint64_t seed) noexcept;
    void setUnit (const Cem3340Unit& u) noexcept { unit = u; }
    const Cem3340Unit& getUnit() const noexcept { return unit; }

    // pitch: commanded pitch in MIDI semitones (the applied CV). pw: 0..1 (0 V..5 V).
    // syncD >= 0: the master's saw fell `syncD` samples before the end of this sample.
    VcoOutputs process (float pitch, float pw, float syncD) noexcept;

    // Position of this oscillator's saw reset inside the last sample, or -1 (drives slaves).
    float lastResetD() const noexcept { return resetD; }

    // Steady-state frequency for a commanded pitch (what the autotune routine measures).
    double staticFrequency (double pitch) const noexcept;

private:
    enum class Ev
    {
        None,
        TurnTop,
        TurnBottom,
        HoldEnd,
        Comparator,
        FallDue,
        Sync
    };

    double saw (double v, int direction) const noexcept;
    double sawSlope (int direction, double su, double sd) const noexcept;
    void pulseStep (float d, float step) noexcept;

    const BlepTable* table = nullptr;
    Cem3340Unit unit;
    Random rng;
    BlepRing sawRing, triRing, pulseRing;

    double sampleRate = 96000.0;
    double v = 0.0;           // capacitor voltage, 1 = Vcc/3 (5 V)
    int dir = 1;              // +1 charging, -1 discharging
    double hold = 0.0;        // samples the sync still clamps the core at 0 V
    bool comparatorHigh = true; // saw below the PW threshold
    bool pulseHigh = true;      // pulse output (lags the comparator on falling edges)
    double pendingFall = -1.0;  // samples until a scheduled falling edge, or -1
    std::array<float, 3> wHistory { 0.5f, 0.5f, 0.5f }; // PW CV of the last three samples (cubic interpolation)
    float resetD = -1.0f;
};

} // namespace augur
