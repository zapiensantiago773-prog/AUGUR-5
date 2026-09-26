#include "Filter/LadderFilter.h"

#include "Util/Simd4.h"

#include <algorithm>
#include <cmath>

namespace augur
{

namespace
{
struct ModelConstants
{
    float maxFeedback;     // loop gain at resonance = 1 (4 = linear self-oscillation threshold)
    float stageDrive;      // pre-gain inside each stage nonlinearity (lower = cleaner stages)
    float inputDrive;      // pre-gain of the input/feedback summing nonlinearity
    float passbandComp;    // 0 = no compensation (full bass loss), 1 = fully compensated
};

// Provisional constants, to be calibrated against recordings (see docs/decisions.md).
constexpr ModelConstants cem3320 { 5.0f, 1.0f, 0.55f, 0.0f };
constexpr ModelConstants ssm2040 { 4.8f, 0.75f, 0.7f, 0.45f };
constexpr ModelConstants cascade { 4.6f, 0.6f, 0.9f, 0.3f };

const ModelConstants& constantsFor (LadderFilter::Model m) noexcept
{
    switch (m)
    {
        case LadderFilter::Model::Ssm2040: return ssm2040;
        case LadderFilter::Model::Cascade: return cascade;
        default: return cem3320;
    }
}

// Multimode integrator saturation and Bite feedback saturation.
constexpr float svfDrive = 0.5f;
constexpr float biteSaturation = 1.4f;
} // namespace

void LadderFilter::prepare (double newSampleRate) noexcept
{
    sampleRate = newSampleRate;
    piOverFs = static_cast<float> (3.14159265358979 / newSampleRate);
    // Top of the range: the instrument's 20 kHz, and never so close to Nyquist that the discretised
    // resonance smears (at 1x / 48 kHz that is 16.8 kHz). Pinning the cutoff near Nyquist let the
    // resonance ring ultrasonically and intermodulate with the note into audible non-harmonic tones.
    maxCutoff = static_cast<float> (std::min (20000.0, 0.35 * newSampleRate));
    hpfOn = false; // set again by the voice's next control update
    reset();
}

void LadderFilter::reset() noexcept
{
    s.fill (0.0f);
    v.fill (0.0f);
    u = 0.0f;
    h.fill (0.0f);
}

void LadderFilter::setModel (Model m) noexcept
{
    if (m == model)
        return;
    // The models give the state variables different meanings: start the new one from rest.
    model = m;
    s.fill (0.0f);
    v.fill (0.0f);
    u = 0.0f;
}

void LadderFilter::setShape (bool twelve, Mode m) noexcept
{
    slope12 = twelve;
    mode = m;
    // Ladder outputs mixed from the stage input and the four stage outputs (Xpander-style). The
    // band-pass mixes are scaled for unity gain at the corner without resonance.
    switch (m)
    {
        case Mode::LowPass:  mix = twelve ? std::array<float, 5> { 0, 0, 1, 0, 0 } : std::array<float, 5> { 0, 0, 0, 0, 1 }; break;
        case Mode::BandPass: mix = twelve ? std::array<float, 5> { 0, 2, -2, 0, 0 } : std::array<float, 5> { 0, 0, 4, -8, 4 }; break;
        case Mode::HighPass: mix = twelve ? std::array<float, 5> { 1, -2, 1, 0, 0 } : std::array<float, 5> { 1, -4, 6, -4, 1 }; break;
    }
}

void LadderFilter::setHighpass (float hz) noexcept
{
    hpfOn = hz > 10.0f;
    if (! hpfOn)
        return;
    hpfG = std::tan (std::min (hz, maxCutoff) * piOverFs);
    constexpr float twoR = 1.41421356f; // Q = 0.707
    hpfNorm = 1.0f / (1.0f + twoR * hpfG + hpfG * hpfG);
}

float LadderFilter::process (float x, float cutoffHz, float resonance) noexcept
{
    LadderFilter* self[4] = { this, this, this, this };
    Lanes lanes;
    gather (lanes, self);
    const float y = simd::get (process4 (lanes, simd::splat (x), simd::splat (cutoffHz), simd::splat (resonance)), 0);
    scatter (lanes, self, 1);
    return y;
}

void LadderFilter::gather (Lanes& lanes, LadderFilter* const* f) noexcept
{
    for (size_t k = 0; k < 4; ++k)
    {
        lanes.s[k] = simd::set (f[0]->s[k], f[1]->s[k], f[2]->s[k], f[3]->s[k]);
        lanes.v[k] = simd::set (f[0]->v[k], f[1]->v[k], f[2]->v[k], f[3]->v[k]);
    }
    lanes.u = simd::set (f[0]->u, f[1]->u, f[2]->u, f[3]->u);
    for (size_t k = 0; k < 2; ++k)
        lanes.h[k] = simd::set (f[0]->h[k], f[1]->h[k], f[2]->h[k], f[3]->h[k]);
}

void LadderFilter::scatter (const Lanes& lanes, LadderFilter* const* f, int count) noexcept
{
    for (int lane = 0; lane < count; ++lane)
    {
        for (size_t k = 0; k < 4; ++k)
        {
            f[lane]->s[k] = simd::get (lanes.s[k], lane);
            f[lane]->v[k] = simd::get (lanes.v[k], lane);
        }
        f[lane]->u = simd::get (lanes.u, lane);
        for (size_t k = 0; k < 2; ++k)
            f[lane]->h[k] = simd::get (lanes.h[k], lane);
    }
}

simd::F4 LadderFilter::process4 (Lanes& l, simd::F4 x, simd::F4 cutoffHz, simd::F4 resonance) const noexcept
{
    using namespace simd;
    const F4 fc = min (max (cutoffHz, splat (5.0f)), splat (maxCutoff));
    const F4 g = simd::tan (fc * splat (piOverFs));
    const F4 r = min (max (resonance, splat (0.0f)), splat (1.1f));

    F4 y;
    switch (model)
    {
        case Model::Multimode: y = svf4 (l, x, g, r); break;
        case Model::Bite:      y = bite4 (l, x, g, r); break;
        default:               y = ladder4 (l, x, g, r); break;
    }

    if (! hpfOn)
        return y;

    // Linear 2-pole high-pass (TPT state-variable, Q 0.707).
    const F4 G = splat (hpfG);
    const F4 hp = (y - splat (1.41421356f + hpfG) * l.h[0] - l.h[1]) * splat (hpfNorm);
    const F4 bp = G * hp + l.h[0];
    l.h[0] = bp + G * hp;
    const F4 lp = G * bp + l.h[1];
    l.h[1] = lp + G * bp;
    return hp;
}

simd::F4 LadderFilter::ladder4 (Lanes& l, simd::F4 x, simd::F4 g, simd::F4 r) const noexcept
{
    using namespace simd;
    const auto& mc = constantsFor (model);
    const F4 k = r * splat (mc.maxFeedback);
    // Passband compensation: add back part of the input so bass does not vanish at high resonance.
    const F4 xin = x * (splat (1.0f) + splat (mc.passbandComp * 0.5f) * k);

    // Secant gains of every nonlinearity at the previous sample's operating point.
    const F4 aIn = tanhRatio (l.u * splat (mc.inputDrive));
    F4 alpha[4], beta[4];
    for (size_t i = 0; i < 4; ++i)
    {
        const F4 G = g * tanhRatio (l.v[i] * splat (mc.stageDrive));
        const F4 inv = splat (1.0f) / (splat (1.0f) + G);
        alpha[i] = G * inv;
        beta[i] = l.s[i] * inv;
    }
    // y4 = A * u0 + B, with u0 = aIn * (xin - k * y4)
    F4 A = splat (1.0f), B = splat (0.0f);
    for (size_t i = 0; i < 4; ++i)
    {
        A = A * alpha[i];
        B = alpha[i] * B + beta[i];
    }
    const F4 y4 = (A * aIn * xin + B) / (splat (1.0f) + A * aIn * k);

    l.u = xin - k * y4;
    const F4 in = aIn * l.u;
    F4 stageIn = in;
    F4 y[4];
    for (size_t i = 0; i < 4; ++i)
    {
        y[i] = alpha[i] * stageIn + beta[i];
        l.v[i] = stageIn - y[i];
        l.s[i] = splat (2.0f) * y[i] - l.s[i];
        stageIn = y[i];
    }

    if (mode == Mode::LowPass && ! slope12)
        return y[3];
    return splat (mix[0]) * in + splat (mix[1]) * y[0] + splat (mix[2]) * y[1] + splat (mix[3]) * y[2] + splat (mix[4]) * y[3];
}

simd::F4 LadderFilter::svf4 (Lanes& l, simd::F4 x, simd::F4 g, simd::F4 r) const noexcept
{
    using namespace simd;
    const F4 one = splat (1.0f), two = splat (2.0f);

    // One nonlinear state-variable section: both integrators saturate (secant gains from the previous
    // sample), the damping loop is solved exactly. Returns LP/BP/HP per `mode`.
    const auto section = [&] (F4 in, F4 R, F4& s1, F4& s2, F4& v1, F4& v2) {
        const F4 g1 = g * tanhRatio (v1 * splat (svfDrive));
        const F4 g2 = g * tanhRatio (v2 * splat (svfDrive));
        const F4 bp = (g1 * (in - s2) + s1) / (one + two * R * g1 + g1 * g2);
        const F4 hp = in - (two * R + g2) * bp - s2;
        const F4 lp = g2 * bp + s2;
        s1 = two * bp - s1;
        s2 = two * lp - s2;
        v1 = hp;
        v2 = bp;
        switch (mode)
        {
            case Mode::HighPass: return hp;
            case Mode::BandPass: return bp * two * R * (splat (4.0f) - splat (3.0f) * R); // unity at R = 1, +12 dB at full resonance
            default: return lp;
        }
    };

    if (slope12)
    {
        const F4 R = max (one - r, splat (0.04f));
        return section (x, R, l.s[0], l.s[1], l.v[0], l.v[1]);
    }
    // 24 dB: Butterworth-spaced pair (damping 0.924 / 0.383); resonance narrows the second section.
    const F4 R1 = splat (0.924f) - splat (0.3f) * r;
    const F4 R2 = max (splat (0.383f) * (one - r), splat (0.03f));
    const F4 a = section (x, R1, l.s[0], l.s[1], l.v[0], l.v[1]);
    return section (a, R2, l.s[2], l.s[3], l.v[2], l.v[3]);
}

simd::F4 LadderFilter::bite4 (Lanes& l, simd::F4 x, simd::F4 g, simd::F4 r) const noexcept
{
    using namespace simd;
    const F4 one = splat (1.0f);
    const F4 G = g / (one + g);
    const F4 invOnePlusG = one / (one + g);
    const F4 K = max (r * splat (2.0f), splat (0.01f)); // self-oscillates at K = 2
    const F4 alpha0 = one / (one - K * G + K * G * G);
    const F4 sat = splat (biteSaturation), invSat = splat (1.0f / biteSaturation);

    // One-pole TPT stages; s[] are their states. The saturator sits inside the positive feedback loop.
    if (mode == Mode::HighPass)
    {
        const F4 v1 = (x - l.s[0]) * G;
        const F4 lp1 = v1 + l.s[0];
        l.s[0] = lp1 + v1;
        const F4 y1 = x - lp1;
        const F4 s35 = (splat (0.0f) - G * invOnePlusG) * l.s[1] + invOnePlusG * l.s[2];
        const F4 sum = simd::tanh (alpha0 * (y1 + s35) * sat) * invSat;
        const F4 y = K * sum;
        const F4 v2 = (y - l.s[1]) * G;
        const F4 lp2 = v2 + l.s[1];
        l.s[1] = lp2 + v2;
        const F4 hp2 = y - lp2;
        const F4 v3 = (hp2 - l.s[2]) * G;
        const F4 lp3 = v3 + l.s[2];
        l.s[2] = lp3 + v3;
        // The loop's second high-pass pole only acts through the feedback: one more at the output
        // makes it 12 dB/oct at any resonance.
        const F4 v4 = (sum - l.s[3]) * G;
        const F4 lp4 = v4 + l.s[3];
        l.s[3] = lp4 + v4;
        return sum - lp4;
    }

    const F4 v1 = (x - l.s[0]) * G;
    const F4 y1 = v1 + l.s[0];
    l.s[0] = y1 + v1;
    const F4 s35 = (K - K * G) * invOnePlusG * l.s[1] - invOnePlusG * l.s[2];
    const F4 sum = simd::tanh (alpha0 * (y1 + s35) * sat) * invSat;
    const F4 v2 = (sum - l.s[1]) * G;
    const F4 y2 = v2 + l.s[1];
    l.s[1] = y2 + v2;
    const F4 y = K * y2;
    const F4 v3 = (y - l.s[2]) * G;
    const F4 lp3 = v3 + l.s[2];
    l.s[2] = lp3 + v3;
    return y2;
}

} // namespace augur
