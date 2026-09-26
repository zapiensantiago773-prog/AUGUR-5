#include "Filter/LadderFilter.h"

#include "Util/FastMath.h"

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
    const float aIn = fastmath::tanhRatio (u * mc.inputDrive);
    float alpha[4], beta[4];
    for (size_t i = 0; i < 4; ++i)
    {
        const float G = g * fastmath::tanhRatio (v[i] * mc.stageDrive);
        const float inv = 1.0f / (1.0f + G);
        alpha[i] = G * inv;
        beta[i] = s[i] * inv;
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

    for (size_t i = 0; i < 4; ++i)
        s[i] = 2.0f * y[i] - s[i];

    return y[3];
}

} // namespace augur
