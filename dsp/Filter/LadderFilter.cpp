#include "Filter/LadderFilter.h"

#include "Util/FastMath.h"
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
} // namespace

void LadderFilter::prepare (double newSampleRate) noexcept
{
    sampleRate = newSampleRate;
    piOverFs = static_cast<float> (3.14159265358979 / newSampleRate);
    maxCutoff = static_cast<float> (0.45 * newSampleRate);
    reset();
}

void LadderFilter::reset() noexcept
{
    s.fill (0.0f);
    v.fill (0.0f);
    u = 0.0f;
}

float LadderFilter::process (float x, float cutoffHz, float resonance) noexcept
{
    const auto& mc = model == Model::Cem3320 ? cem3320 : ssm2040;
    const float fc = std::clamp (cutoffHz, 5.0f, maxCutoff);
    const float g = fastmath::tan (fc * piOverFs);
    const float r = std::clamp (resonance, 0.0f, 1.1f);
    const float k = r * mc.maxFeedback;

    // Passband compensation: add back part of the input so bass does not vanish at high resonance.
    const float xin = x * (1.0f + mc.passbandComp * k * 0.5f);

    // Secant gains of every nonlinearity at the previous sample's operating point (semi-implicit:
    // the linear part of the loop is solved exactly, so it stays stable at any cutoff/resonance).
    // The four stages are independent here, so they are computed together (SIMD).
    const float aIn = fastmath::tanhRatio (u * mc.inputDrive);
    float alpha[4], beta[4];
    {
        using namespace simd;
        const F4 G = splat (g) * tanhRatio (load (v.data()) * splat (mc.stageDrive));
        const F4 inv = splat (1.0f) / (splat (1.0f) + G);
        store (alpha, G * inv);
        store (beta, load (s.data()) * inv);
    }

    // y4 = A * u0 + B, with u0 = aIn * (xin - k * y4)
    float A = 1.0f, B = 0.0f;
    for (size_t i = 0; i < 4; ++i)
    {
        A *= alpha[i];
        B = alpha[i] * B + beta[i];
    }
    const float y4 = (A * aIn * xin + B) / (1.0f + A * aIn * k);

    std::array<float, 4> y {};
    u = xin - k * y4;
    float stageIn = aIn * u;
    for (size_t i = 0; i < 4; ++i)
    {
        y[i] = alpha[i] * stageIn + beta[i];
        v[i] = stageIn - y[i];
        stageIn = y[i];
    }

    simd::store (s.data(), simd::splat (2.0f) * simd::load (y.data()) - simd::load (s.data()));

    return y[3];
}

void LadderFilter::gather (Lanes& lanes, LadderFilter* const* f) noexcept
{
    for (size_t k = 0; k < 4; ++k)
    {
        lanes.s[k] = simd::set (f[0]->s[k], f[1]->s[k], f[2]->s[k], f[3]->s[k]);
        lanes.v[k] = simd::set (f[0]->v[k], f[1]->v[k], f[2]->v[k], f[3]->v[k]);
    }
    lanes.u = simd::set (f[0]->u, f[1]->u, f[2]->u, f[3]->u);
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
    }
}

simd::F4 LadderFilter::process4 (Lanes& l, simd::F4 x, simd::F4 cutoffHz, simd::F4 resonance) const noexcept
{
    using namespace simd;
    const auto& mc = model == Model::Cem3320 ? cem3320 : ssm2040;
    const F4 fc = min (max (cutoffHz, splat (5.0f)), splat (maxCutoff));
    const F4 g = simd::tan (fc * splat (piOverFs));
    const F4 k = min (max (resonance, splat (0.0f)), splat (1.1f)) * splat (mc.maxFeedback);
    const F4 xin = x * (splat (1.0f) + splat (mc.passbandComp * 0.5f) * k);

    // Same semi-implicit solve as process(), one voice per lane.
    const F4 aIn = tanhRatio (l.u * splat (mc.inputDrive));
    F4 alpha[4], beta[4];
    for (size_t i = 0; i < 4; ++i)
    {
        const F4 G = g * tanhRatio (l.v[i] * splat (mc.stageDrive));
        const F4 inv = splat (1.0f) / (splat (1.0f) + G);
        alpha[i] = G * inv;
        beta[i] = l.s[i] * inv;
    }
    F4 A = splat (1.0f), B = splat (0.0f);
    for (size_t i = 0; i < 4; ++i)
    {
        A = A * alpha[i];
        B = alpha[i] * B + beta[i];
    }
    const F4 y4 = (A * aIn * xin + B) / (splat (1.0f) + A * aIn * k);

    l.u = xin - k * y4;
    F4 stageIn = aIn * l.u;
    for (size_t i = 0; i < 4; ++i)
    {
        const F4 y = alpha[i] * stageIn + beta[i];
        l.v[i] = stageIn - y;
        l.s[i] = splat (2.0f) * y - l.s[i];
        stageIn = y;
    }
    return stageIn;
}

} // namespace augur
