#pragma once

#include "Util/Simd4.h"

#include <array>

namespace augur
{

// The voice filter. Every model is a zero-delay-feedback (TPT) design with its nonlinearities
// linearised around the previous sample's operating point (semi-implicit, Mystran-style), so the
// linear part of each loop is solved exactly: stable at any cutoff, under audio-rate modulation and
// in self-oscillation.
//
//  - Ladder family (Cem3320, Ssm2040, Cascade): four nonlinear integrator stages, each integrating
//    tanh(input - output) as an OTA cascade does, with global feedback. Slope and mode come from
//    mixing the stage outputs (the Xpander approach): LP/BP/HP at 12 or 24 dB/oct.
//  - Multimode: state-variable filter with saturating integrators (SEM-style, 12 dB), or two in
//    series for 24 dB (Butterworth-spaced, resonance on the second section).
//  - Bite: Sallen-Key "Korg35"-style 2-pole with the saturator inside the positive feedback loop:
//    aggressive, screaming resonance. LP or HP (BP uses LP).
//
// After any model, an optional 2-pole high-pass (Q 0.707) cleans up the low end.
class LadderFilter
{
public:
    enum class Model
    {
        Cem3320,   // Rev 3: full bass loss with resonance, sharper resonance
        Ssm2040,   // Rev 1/2: softer stage saturation, partial passband compensation, rounder resonance
        Cascade,   // OTA 4-pole, cleaner stages and a driven input: smooth, glassy resonance
        Multimode, // nonlinear state-variable filter
        Bite       // aggressive 2-pole with saturating feedback
    };

    enum class Mode
    {
        LowPass,
        BandPass,
        HighPass
    };

    void prepare (double sampleRate) noexcept;
    void reset() noexcept;

    // Four filters processed at once (one voice per SIMD lane). All lanes use this filter's model,
    // shape and sample rate; the state is gathered from / scattered back to the scalar filters per segment.
    struct Lanes
    {
        simd::F4 s[4], v[4], u, h[2];
    };
    static void gather (Lanes& lanes, LadderFilter* const* filters) noexcept;
    static void scatter (const Lanes& lanes, LadderFilter* const* filters, int count) noexcept;
    simd::F4 process4 (Lanes& lanes, simd::F4 x, simd::F4 cutoffHz, simd::F4 resonance) const noexcept;

    void copyStateFrom (const LadderFilter& other) noexcept
    {
        s = other.s;
        v = other.v;
        u = other.u;
        h = other.h;
    }
    void setModel (Model m) noexcept;
    void setShape (bool slope12, Mode m) noexcept;
    // Post-filter high-pass corner; at or below 10 Hz it is bypassed.
    void setHighpass (float hz) noexcept;

    // cutoffHz: corner frequency; resonance 0..1 (self-oscillates close to 1 in the ladder and Bite models).
    // Scalar reference path (runs the SIMD kernel on one lane).
    float process (float x, float cutoffHz, float resonance) noexcept;

private:
    simd::F4 ladder4 (Lanes& l, simd::F4 x, simd::F4 g, simd::F4 resonance) const noexcept;
    simd::F4 svf4 (Lanes& l, simd::F4 x, simd::F4 g, simd::F4 resonance) const noexcept;
    simd::F4 bite4 (Lanes& l, simd::F4 x, simd::F4 g, simd::F4 resonance) const noexcept;

    double sampleRate = 96000.0;
    float piOverFs = 0.0f;
    float maxCutoff = 20000.0f;
    Model model = Model::Cem3320;
    Mode mode = Mode::LowPass;
    bool slope12 = false;
    std::array<float, 5> mix { 0.0f, 0.0f, 0.0f, 0.0f, 1.0f }; // ladder output mix: input, y1..y4
    bool hpfOn = false;
    float hpfG = 0.0f, hpfNorm = 1.0f;

    std::array<float, 4> s {};  // integrator states
    std::array<float, 4> v {};  // nonlinearity operating points
    float u = 0.0f;             // last input-stage argument
    std::array<float, 2> h {};  // high-pass states
};

} // namespace augur
