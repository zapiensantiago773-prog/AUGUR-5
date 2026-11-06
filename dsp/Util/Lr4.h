#pragma once

#include <algorithm>
#include <cmath>

namespace augur
{

// The stereo parts of the effects (and of UNISON / DUO) act only above this (D-057): below it the bass stays mono
// and in phase, as the instrument itself is mono. The series' value (MANTIS-37 measured the same corner).
inline constexpr double stereoBassHz = 150.0;

// Linkwitz-Riley 4th-order high-pass: two identical Butterworth 2nd-order sections (TPT state-variable form, stable
// and exact at any sample rate). -6 dB at the corner, 24 dB / octave below it: at 55 Hz, -35 dB for a 150 Hz corner.
// A true high-pass, not x - LP(x): that complement still carries the bass (+1.9 dB at 55 Hz for a 120 Hz corner).
class Lr4HighPass
{
public:
    void setCutoff (double hz, double sampleRate) noexcept
    {
        constexpr double pi = 3.14159265358979323846;
        g = std::tan (pi * std::clamp (hz, 1.0, 0.45 * sampleRate) / sampleRate);
        a1 = 1.0 / (1.0 + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }

    double process (double x) noexcept
    {
        for (auto& s : stage)
        {
            const double v3 = x - s.ic2;
            const double v1 = a1 * s.ic1 + a2 * v3;
            const double v2 = s.ic2 + a2 * s.ic1 + a3 * v3;
            s.ic1 = 2.0 * v1 - s.ic1;
            s.ic2 = 2.0 * v2 - s.ic2;
            x = x - k * v1 - v2;
        }
        return x;
    }

    void reset() noexcept
    {
        for (auto& s : stage)
            s = {};
    }

private:
    static constexpr double k = 1.4142135623730951; // 1 / Q, Q = 1 / sqrt 2 (Butterworth)
    struct State
    {
        double ic1 = 0.0, ic2 = 0.0;
    };
    State stage[2];
    double g = 0.0, a1 = 1.0, a2 = 0.0, a3 = 0.0;
};

// High-pass on the side only: the direction `axis` (cos, sin of the pan angle; the centre is 0.7071, 0.7071) passes
// untouched, sample for sample, and only what lies across it loses its bass. So the middle of an effect's wet keeps
// its phase against the dry (an all-pass on the whole wet changed how echoes and early tails add up with a held note:
// measured on every preset, twice the level changes and +5.9 dB on one lead), and a sound panned on purpose keeps
// its bass where it was panned. What it costs: above the corner the side's phase turns (90 degrees at twice it, 30 at
// five times), so a wet's own L / R balance in 150 Hz .. 1 kHz shifts a little (D-057).
class MonoBass
{
public:
    void prepare (double sampleRate) noexcept
    {
        hp.setCutoff (stereoBassHz, sampleRate);
        hp.reset();
    }
    void reset() noexcept { hp.reset(); }

    void process (double& l, double& r, double axisL = 0.70710678118654752, double axisR = 0.70710678118654752) noexcept
    {
        const double along = axisL * l + axisR * r;
        const double across = hp.process (axisL * r - axisR * l);
        l = axisL * along - axisR * across;
        r = axisR * along + axisL * across;
    }

private:
    Lr4HighPass hp;
};

} // namespace augur
