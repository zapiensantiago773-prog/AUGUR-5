#include "Effects/Phaser.h"

#include "Util/FastMath.h"

#include <algorithm>
#include <cmath>

namespace augur
{

void Phaser::prepare (double newSampleRate) noexcept
{
    sampleRate = newSampleRate;
    piOverFs = static_cast<float> (3.14159265358979 / newSampleRate);
    reset();
}

void Phaser::reset() noexcept
{
    left = Channel {};
    right = Channel {};
}

float Phaser::Channel::process (float x, float a, float feedback) noexcept
{
    // First-order all-pass (TDF-II): y = a*x + s; s = x - a*y. Feedback from the chain output,
    // soft-limited so high settings ring instead of blowing up.
    float s = x + fastmath::tanh (last * feedback);
    for (float& z : state)
    {
        const float y = a * s + z;
        z = s - a * y;
        s = y;
    }
    last = s;
    return s;
}

void Phaser::process (float* l, float* r, int numSamples, float rateHz, float depth, float feedback, float mix) noexcept
{
    const double inc = std::clamp (static_cast<double> (rateHz), 0.01, 20.0) / sampleRate;
    const float width = std::clamp (depth, 0.0f, 1.0f) * 5.0f; // octaves of sweep (up to 120 Hz .. 4 kHz)
    const float fb = std::clamp (feedback, 0.0f, 0.95f);
    const float wet = std::clamp (mix, 0.0f, 1.0f);
    const float lowOct = 7.0f + (5.0f - width) * 0.5f;       // log2 Hz of the sweep's low end

    const auto coeff = [this] (float oct) {
        const float t = fastmath::tan (std::min (fastmath::exp2 (oct) * piOverFs, 1.4f));
        return (t - 1.0f) / (t + 1.0f);
    };

    for (int n = 0; n < numSamples; ++n)
    {
        phase += inc;
        if (phase >= 1.0)
            phase -= 1.0;
        const float ph = static_cast<float> (phase);
        const float triL = 1.0f - 2.0f * std::abs (ph - 0.5f) * 2.0f;
        const float phR = ph + 0.25f >= 1.0f ? ph - 0.75f : ph + 0.25f;
        const float triR = 1.0f - 2.0f * std::abs (phR - 0.5f) * 2.0f;
        const float aL = coeff (lowOct + width * (0.5f + 0.5f * triL));
        const float aR = coeff (lowOct + width * (0.5f + 0.5f * triR));

        const float dl = l[n], dr = r[n];
        const float yl = left.process (dl, aL, fb);
        const float yr = right.process (dr, aR, fb);
        // Equal mix of dry and all-passed gives the deepest notches (classic phaser at wet = 0.5).
        l[n] = dl * (1.0f - wet) + yl * wet;
        r[n] = dr * (1.0f - wet) + yr * wet;
    }
}

} // namespace augur
