#pragma once

#include "Oscillator/BlepTable.h"
#include "Util/FastMath.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace augur
{

// One band-limited output stream: the naive waveform plus BLEP/BLAMP corrections, delayed by
// BlepTable::zeroCrossings samples (linear-phase kernel).
class BlepRing
{
public:
    static constexpr int latency = BlepTable::zeroCrossings;

    void reset() noexcept
    {
        ring.fill (0.0f);
        index = 0;
    }

    // Discontinuity `d` samples (0..1) before the current sample: value step and slope step (per sample).
    void add (const BlepTable& table, float d, float step, float slope) noexcept
    {
        if (step == 0.0f && slope == 0.0f)
            return;
        table.accumulate (ring.data(), mask, index - static_cast<unsigned> (latency), d, step, slope);
    }

    // Adds the naive value of the current sample, returns the finished (delayed) sample and advances.
    float push (float naive) noexcept
    {
        ring[index & mask] += naive;
        const unsigned out = (index - static_cast<unsigned> (latency)) & mask;
        const float y = ring[out];
        ring[out] = 0.0f;
        ++index;
        return y;
    }

private:
    static constexpr unsigned size = 32;
    static constexpr unsigned mask = size - 1;
    std::array<float, size> ring {};
    unsigned index = 0;
};

// Outputs of one VCO chip, in units of their own full-scale voltage (all positive-going like the chips):
// saw 0..1 (= 0..10 V), tri 0..1 (= 0..5 V), pulse 0..1 (= 0..12.9 V on the Prophet's 10 K pull-down).
struct VcoOutputs
{
    float saw = 0.0f;
    float tri = 0.0f;
    float pulse = 0.0f;
};

// Exponential converter of one chip instance: turns the commanded pitch (what an ideal 1 V/oct VCO
// would play, in MIDI semitones) into the frequency the core really runs at. Autotune measures and
// corrects this; what it cannot correct (between its measurement points) stays as the unit's character.
struct ExpoConverter
{
    float tuneCents = 0.0f;       // initial frequency error of this unit
    float scaleError = 0.0f;      // relative V/oct error (0.0005 = 0.05 %, CEM3340 trimmed typical)
    float bulkCentsAt10k = 0.0f;  // flattening from Q2 bulk emitter resistance, cents at 10 kHz

    // Exact version (autotune, tests).
    double frequency (double pitch) const noexcept
    {
        const double cents = tuneCents + scaleError * 100.0 * (pitch - 60.0);
        const double f = 440.0 * std::exp2 ((pitch + cents * 0.01 - 69.0) / 12.0);
        return f * std::exp2 (-bulkCentsAt10k * (f / 10000.0) / 1200.0);
    }

    // Per-sample version: one fast exp2, bulk term linearised (it is < 0.5 % everywhere).
    float fastFrequency (float pitch) const noexcept
    {
        const float cents = tuneCents + scaleError * 100.0f * (pitch - 60.0f);
        const float f = fastmath::semitonesToHz (pitch + cents * 0.01f);
        return f * (1.0f - bulkCentsAt10k * f * (0.69314718f / 1200.0f / 10000.0f));
    }
};

} // namespace augur
