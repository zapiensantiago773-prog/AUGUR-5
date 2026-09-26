#pragma once

#include <vector>

namespace augur
{

// Tabulated band-limited step (BLEP) and ramp (BLAMP) residuals.
// Kernel: sinc at 0.48 cycles/sample with a Kaiser window (beta 11.2, ~110 dB), 8 zero crossings per side.
// At the voice's internal rate (>= 88.2 kHz) everything that could fold below 20 kHz lies in the stopband.
// Linear-phase kernel -> the oscillator output is delayed by `zeroCrossings` samples.
class BlepTable
{
public:
    static constexpr int zeroCrossings = 8;
    static constexpr int taps = 2 * zeroCrossings;
    static constexpr int phases = 512;
    static constexpr int segment = phases + 1; // each tap owns [t, t+1] incl. both ends: no interpolation across t = 0

    // Built on first use. Call once outside the audio thread (Vco::prepare does).
    static const BlepTable& get();

    // Adds step * BLEP(t) + slope * BLAMP(t) for an event that happened `d` samples (0..1) before the
    // current sample. `ring[(start + j) & mask]` receives tap j, where start = currentIndex - zeroCrossings.
    void accumulate (float* ring, unsigned mask, unsigned start, float d, float step, float slope) const noexcept
    {
        float pos = d * static_cast<float> (phases);
        pos = pos < 0.0f ? 0.0f : (pos > static_cast<float> (phases) ? static_cast<float> (phases) : pos);
        const int i0 = static_cast<int> (pos);
        const float fr = pos - static_cast<float> (i0);

        const float* b = blep.data() + i0;
        const float* r = blamp.data() + i0;
        for (int j = 0; j < taps; ++j)
        {
            const int o = j * segment;
            float v = 0.0f;
            if (step != 0.0f)
                v += step * (b[o] + fr * (b[o + 1] - b[o]));
            if (slope != 0.0f)
                v += slope * (r[o] + fr * (r[o + 1] - r[o]));
            ring[(start + static_cast<unsigned> (j)) & mask] += v;
        }
    }

private:
    BlepTable();

    std::vector<float> blep, blamp;
};

} // namespace augur
