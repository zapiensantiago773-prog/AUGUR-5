#pragma once

#include "Util/Simd4.h"

#include <array>

namespace augur
{

// 4-pole (24 dB/oct) low-pass built from four nonlinear integrator stages with global feedback,
// discretised with the trapezoidal rule (ZDF / TPT). Each stage integrates tanh(input - output), as
// an OTA-style cascade does. Each tanh is linearised around its previous operating point (secant
// gain) and the resulting linear zero-delay loop is solved exactly (semi-implicit, Mystran-style):
// stable at any cutoff, including audio-rate modulation and self-oscillation.
class LadderFilter
{
public:
    enum class Model
    {
        Cem3320, // Rev 3: full bass loss with resonance, sharper resonance
        Ssm2040  // Rev 1/2: softer stage saturation, partial passband compensation, rounder resonance
    };

    void prepare (double sampleRate) noexcept;
    void reset() noexcept;
    // Four filters processed at once (one voice per SIMD lane). All lanes use this filter's model and
    // sample rate; the state is gathered from / scattered back to the scalar filters per segment.
    struct Lanes
    {
        simd::F4 s[4], v[4], u;
    };
    static void gather (Lanes& lanes, LadderFilter* const* filters) noexcept;
    static void scatter (const Lanes& lanes, LadderFilter* const* filters, int count) noexcept;
    simd::F4 process4 (Lanes& lanes, simd::F4 x, simd::F4 cutoffHz, simd::F4 resonance) const noexcept;

    void copyStateFrom (const LadderFilter& other) noexcept
    {
        s = other.s;
        v = other.v;
        u = other.u;
    }
    void setModel (Model m) noexcept { model = m; }

    // cutoffHz: stage corner frequency; resonance 0..1 (self-oscillates close to 1); resonanceScale
    // is a per-voice trim.
    float process (float x, float cutoffHz, float resonance) noexcept;

private:
    double sampleRate = 96000.0;
    float piOverFs = 0.0f;
    float maxCutoff = 20000.0f;
    Model model = Model::Cem3320;

    std::array<float, 4> s {};  // integrator states
    std::array<float, 4> v {};  // last stage differences (nonlinearity operating points)
    float u = 0.0f;             // last input-stage argument
};

} // namespace augur
