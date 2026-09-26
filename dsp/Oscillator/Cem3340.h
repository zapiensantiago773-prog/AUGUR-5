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
    VcoOutputs process (float pitch, float pw, float syncD) noexcept
    {
        // Fast path (inline): most samples contain no event at all. The charging rate is linear across
        // the sample (continuous exponential FM); thresholds are 1 and 0 (the overshoot only extends them).
        const double f = static_cast<double> (unit.expo.fastFrequency (pitch));
        const double b1 = f > 0.0 ? (f * twoOverSampleRate < 0.8 ? f * twoOverSampleRate : 0.8) : 0.0;
        const double b0 = basePrevious < 0.0 ? b1 : basePrevious;
        const float p3 = pw + unit.pwOffset;
        const float wNew = p3 < 0.0f ? 0.0f : (p3 > 1.0f ? 1.0f : p3);
        if (hold <= 0.0 && pendingFall < 0.0 && syncD < 0.0f)
        {
            const double travel = 0.5 * (b0 + b1);
            const double vEnd = dir > 0 ? v + (1.0 - 0.5 * unit.asymmetry) * travel : v - (1.0 + 0.5 * unit.asymmetry) * travel;
            if (dir > 0 ? vEnd < 1.0 : vEnd > 0.0)
            {
                const bool highStart = saw (v, dir) < wHistory[1];
                const bool highEnd = saw (vEnd, dir) < wHistory[2];
                if (highStart == comparatorHigh && highEnd == comparatorHigh)
                {
                    basePrevious = b1;
                    wHistory = { wHistory[1], wHistory[2], wNew };
                    resetD = -1.0f;
                    v = vEnd;
                    return pushOutputs();
                }
            }
        }
        return processEvents (b0, b1, wNew, syncD);
    }

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

    double saw (double vv, int direction) const noexcept
    {
        // Tri->saw converter: pass the rising half (0..5 V -> 0..5 V), invert and lift the falling half
        // (5..0 V -> 5..10 V). Saw units: 1 = 10 V.
        return direction > 0 ? 0.5 * vv
                             : 0.5 + 0.5 * static_cast<double> (unit.converterGain) * (1.0 - vv) + static_cast<double> (unit.converterOffset);
    }
    double sawSlope (int direction, double su, double sd) const noexcept;
    VcoOutputs pushOutputs() noexcept
    {
        VcoOutputs out;
        out.saw = sawRing.push (static_cast<float> (hold > 0.0 ? 0.0 : saw (v, dir))) * unit.sawLevel;
        out.tri = triRing.push (static_cast<float> (hold > 0.0 ? 0.0 : v)) * unit.triLevel;
        out.pulse = pulseRing.push (pulseHigh ? 1.0f : 0.0f) * unit.pulseLevel;
        return out;
    }
    VcoOutputs processEvents (double b0, double b1, float wNew, float syncD) noexcept;

    const BlepTable* table = nullptr;
    Cem3340Unit unit;
    Random rng;
    BlepRing sawRing, triRing, pulseRing;

    double sampleRate = 96000.0;
    double twoOverSampleRate = 2.0 / 96000.0;
    double v = 0.0;           // capacitor voltage, 1 = Vcc/3 (5 V)
    int dir = 1;              // +1 charging, -1 discharging
    double hold = 0.0;        // samples the sync still clamps the core at 0 V
    bool comparatorHigh = true; // saw below the PW threshold
    bool pulseHigh = true;      // pulse output (lags the comparator on falling edges)
    double pendingFall = -1.0;  // samples until a scheduled falling edge, or -1
    double basePrevious = -1.0; // charging rate at the end of the previous sample
    std::array<float, 3> wHistory { 0.5f, 0.5f, 0.5f }; // PW CV of the last three samples (cubic interpolation)
    float resetD = -1.0f;
};

} // namespace augur
