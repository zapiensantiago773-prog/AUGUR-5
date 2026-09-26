#pragma once

#include "Oscillator/BlepTable.h"
#include "Util/Random.h"

#include <array>
#include <cstdint>

namespace augur
{

// Circuit-level character of one VCO chip instance (CEM3340 / SSM2030 style).
struct VcoCharacter
{
    float curvature = 0.0f;       // quadratic bow of the ramp (c = phi + k*phi*(1-phi)); 0 = linear
    float deadTimeSeconds = 0.0f; // capacitor discharge time: flat notch at the ramp bottom, scales with pitch
    float hfTrim = 1.0f;          // HF tracking trim; 1 exactly cancels the pitch loss from the dead time
    float triPeak = 0.5f;         // apex of the triangle waveshaper (0.5 = symmetric)
    float sawLevel = 1.0f;
    float triLevel = 1.0f;
    float pulseLevel = 1.0f;
    float pwOffset = 0.0f;        // comparator threshold error
    float dcOffset = 0.0f;
    float cycleJitter = 0.0f;     // relative period jitter (std-dev) drawn once per cycle
};

// Ramp-core VCO. The capacitor ramp is the master; triangle (waveshaper) and pulse (comparator)
// are derived from it, exactly like the chip. Every discontinuity (reset, pulse edge, triangle apex,
// end of dead time, hard sync) is placed at its exact sub-sample time with BLEP/BLAMP residuals.
class Vco
{
public:
    static constexpr int latencySamples = BlepTable::zeroCrossings;

    void prepare (double sampleRate, std::uint64_t seed) noexcept;
    void setCharacter (const VcoCharacter& c) noexcept { character = c; }
    const VcoCharacter& getCharacter() const noexcept { return character; }

    // Soft phase reset (optional behaviour; the default is free-running).
    void setPhase (double newPhase) noexcept;

    // Returns one band-limited sample (delayed by latencySamples).
    // syncD >= 0: the master reset `syncD` samples before this sample's end -> hard sync.
    float process (float freqHz, float pw, float gSaw, float gTri, float gPulse, float syncD) noexcept;

    // Sub-sample position of this oscillator's own reset in the last processed sample, or -1.
    float lastResetD() const noexcept { return resetD; }

private:
    struct WaveState
    {
        float saw, tri, pulse, dsaw, dtri;
    };

    WaveState evaluate (double ph, bool inDead, double inc, float w, float p) const noexcept;
    double phiFromC (double c) const noexcept;
    void addEvent (float d, float step, float slope) noexcept;

    static constexpr unsigned ringSize = 32;
    static constexpr unsigned ringMask = ringSize - 1;

    const BlepTable* table = nullptr;
    VcoCharacter character;
    Random rng;

    double sampleRate = 48000.0;
    double invSampleRate = 1.0 / 48000.0;
    double phase = 0.0;
    double dead = 0.0;   // remaining dead time in samples
    double jitter = 1.0; // current cycle's period factor
    float resetD = -1.0f;

    std::array<float, ringSize> ring {};
    unsigned writeIndex = 0;
};

} // namespace augur
