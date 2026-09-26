#pragma once

#include "Oscillator/BlepRing.h"
#include "Util/Random.h"

#include <cstdint>

namespace augur
{

// Tolerances of one SSM2030 (Prophet-5 Rev 1/2).
struct Ssm2030Unit
{
    ExpoConverter expo;
    float curvature = 0.0f;       // quadratic bow of the ramp (c = phi + k*phi*(1-phi)); 0 = linear
    float deadTimeSeconds = 0.0f; // reset (capacitor discharge) time: flat notch at the ramp bottom
    float triPeak = 0.5f;         // apex of the external saw->triangle shaper (0.5 = symmetric)
    float pwOffset = 0.0f;        // comparator threshold error
    float cycleJitter = 0.0f;     // relative period jitter (std-dev) drawn once per cycle
    float sawLevel = 1.0f, triLevel = 1.0f, pulseLevel = 1.0f;
};

// Ramp-core VCO (SSM2030 architecture): the timing capacitor charges linearly and a comparator
// discharges it; the triangle is shaped externally from the ramp and the pulse compares the ramp with
// the PW CV. Every discontinuity is placed at its exact sub-sample time (BLEP/BLAMP).
class Ssm2030Vco
{
public:
    static constexpr int latencySamples = BlepRing::latency;

    void prepare (double sampleRate, std::uint64_t seed) noexcept;
    void setUnit (const Ssm2030Unit& u) noexcept { unit = u; }
    const Ssm2030Unit& getUnit() const noexcept { return unit; }

    // pitch: commanded MIDI pitch. syncD >= 0: the master reset `syncD` samples before this sample's end.
    VcoOutputs process (float pitch, float pw, float syncD) noexcept;

    float lastResetD() const noexcept { return resetD; }
    double staticFrequency (double pitch) const noexcept;

private:
    struct WaveState
    {
        float saw, tri, pulse, dsaw, dtri;
    };

    WaveState evaluate (double ph, bool inDead, double inc, float w, float p) const noexcept;
    double phiFromC (double c) const noexcept;

    const BlepTable* table = nullptr;
    Ssm2030Unit unit;
    Random rng;
    BlepRing sawRing, triRing, pulseRing;

    double sampleRate = 96000.0;
    double invSampleRate = 1.0 / 96000.0;
    double phase = 0.0;
    double dead = 0.0;   // remaining dead time in samples
    double jitter = 1.0; // current cycle's period factor
    double incPrevious = -1.0; // phase increment at the end of the previous sample
    float resetD = -1.0f;
};

} // namespace augur
