#pragma once

#include <algorithm>
#include <array>
#include <cmath>

namespace augur
{

// The Prophet-5 TUNE routine (service manual 2-13), applied to a modelled VCO:
// - the period of the VCO is measured by counting 2.5 MHz CPU clock cycles;
// - successive approximation finds the 14-bit CV (651 uV = 1/128 semitone steps) whose counted period
//   matches the reference note's count;
// - biases are measured at C3..C9 and extrapolated for C0..C2 (to save tuning time, as on the hardware);
// - while playing, each note's bias is interpolated between the octave points.
// What remains is the unit's real residual error: quantisation, count resolution (coarse at the top
// octaves) and the converter's non-linearity between octave points.
struct AutotuneTable
{
    static constexpr int points = 10;          // C0 .. C9
    static constexpr double dacStepsPerOctave = 1536.0; // 128 steps per semitone
    static constexpr double clockHz = 2.5e6;

    std::array<float, points> biasSemitones {};

    // Bias for a pitch (MIDI semitones), interpolated between the octave points.
    float biasFor (float pitch) const noexcept
    {
        const float pos = (pitch - 12.0f) * (1.0f / 12.0f); // octaves above C0 (MIDI 12)
        const int k = std::clamp (static_cast<int> (std::floor (pos)), 0, points - 2);
        const float frac = pos - static_cast<float> (k);
        return biasSemitones[static_cast<size_t> (k)] + frac * (biasSemitones[static_cast<size_t> (k + 1)] - biasSemitones[static_cast<size_t> (k)]);
    }

    // `staticFrequency (pitch)` is the VCO's steady-state frequency for a commanded pitch.
    template <typename Vco>
    void tune (const Vco& vco) noexcept
    {
        for (int k = 3; k < points; ++k)
        {
            const double note = 12.0 * (k + 1);
            const double target = 440.0 * std::exp2 ((note - 69.0) / 12.0);
            const double targetCount = std::floor (clockHz / target);

            // Smallest DAC code whose counted period is not longer than the reference count.
            int lo = -4096, hi = 4095;
            while (lo < hi)
            {
                const int mid = lo + (hi - lo) / 2;
                const double f = vco.staticFrequency (note + 12.0 * mid / dacStepsPerOctave);
                if (std::floor (clockHz / f) <= targetCount)
                    hi = mid;
                else
                    lo = mid + 1;
            }
            biasSemitones[static_cast<size_t> (k)] = static_cast<float> (12.0 * lo / dacStepsPerOctave);
        }
        const float b3 = biasSemitones[3], b4 = biasSemitones[4];
        for (int k = 0; k < 3; ++k)
            biasSemitones[static_cast<size_t> (k)] = b3 + (b3 - b4) * static_cast<float> (3 - k);
    }
};

// 14-bit oscillator DAC: pitch CVs land on a 1/128-semitone grid.
inline float quantiseOscCv (float semitones) noexcept
{
    return std::round (semitones * 128.0f) * (1.0f / 128.0f);
}

} // namespace augur
