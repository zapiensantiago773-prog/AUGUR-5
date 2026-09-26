#include "Effects/SpringReverb.h"

#include <algorithm>
#include <cmath>

namespace augur
{

namespace
{
constexpr double twoPi = 6.28318530717959;
// Round-trip delays of the three springs at medium tension (seconds): typical long tanks.
constexpr std::array<double, 3> springSeconds { 0.0371, 0.0433, 0.0509 };
} // namespace

void SpringReverb::prepare (double newSampleRate, std::uint64_t seed)
{
    sampleRate = newSampleRate;
    rng.setSeed (seed);
    // Dispersion band up to ~fs / (2K): about 4 kHz whatever the rate.
    stretch = std::max (2, static_cast<int> (std::lround (newSampleRate / 8000.0)));
    loopLpCoeff = static_cast<float> (1.0 - std::exp (-twoPi * 4500.0 / newSampleRate));
    outLpCoeff = static_cast<float> (1.0 - std::exp (-twoPi * 5200.0 / newSampleRate));
    inHpCoeff = static_cast<float> (1.0 - std::exp (-twoPi * 180.0 / newSampleRate));
    driftCoeff = static_cast<float> (1.0 - std::exp (-1.0 / (0.25 * newSampleRate)));

    for (size_t i = 0; i < springs.size(); ++i)
    {
        auto& s = springs[i];
        const int maxDelay = static_cast<int> (springSeconds[i] * 1.4 * newSampleRate) + 8;
        int size = 1;
        while (size < maxDelay)
            size <<= 1;
        s.loop.assign (static_cast<size_t> (size), 0.0f);
        s.loopMask = size - 1;
        s.baseDelay = static_cast<float> (springSeconds[i] * newSampleRate);
        s.apState.assign (static_cast<size_t> (chainLength * stretch), 0.0f);
    }
    reset();
}

void SpringReverb::reset() noexcept
{
    for (auto& s : springs)
    {
        std::fill (s.loop.begin(), s.loop.end(), 0.0f);
        std::fill (s.apState.begin(), s.apState.end(), 0.0f);
        s.write = 0;
        s.apPos = 0;
        s.lp = s.lp2 = 0.0f;
        s.drift = s.driftTarget = 0.0f;
    }
    inHp = 0.0f;
    outLp = {};
    driftCounter = 0;
}

float SpringReverb::processSpring (Spring& s, float x, float feedback, float delaySamples) noexcept
{
    // Loop return: fractional read of the round-trip delay, then the tank's high-frequency loss.
    const float pos = static_cast<float> (s.write) - delaySamples;
    const float fl = std::floor (pos);
    const int i = static_cast<int> (fl);
    const float t = pos - fl;
    const float a0 = s.loop[static_cast<size_t> (i & s.loopMask)], a1 = s.loop[static_cast<size_t> ((i + 1) & s.loopMask)];
    const float back = a0 + (a1 - a0) * t;
    s.lp += (back - s.lp) * loopLpCoeff;

    // Dispersive chain: w[n] = u[n] - a w[n-K]; y[n] = a w[n] + w[n-K] (one K-sample ring per stage).
    float u = x + feedback * s.lp;
    const int K = stretch;
    float* state = s.apState.data();
    for (int stage = 0; stage < chainLength; ++stage)
    {
        float& slot = state[stage * K + s.apPos];
        const float delayed = slot;
        const float w = u - apCoeff * delayed;
        u = apCoeff * w + delayed;
        slot = w;
    }
    s.apPos = s.apPos + 1 == K ? 0 : s.apPos + 1;

    s.write = (s.write + 1) & s.loopMask;
    s.loop[static_cast<size_t> (s.write)] = u;
    return u;
}

void SpringReverb::process (const float* input, float* outL, float* outR, int numSamples, float decaySeconds, float tension,
                            float level) noexcept
{
    const float lengthScale = 1.3f - 0.6f * std::clamp (tension, 0.0f, 1.0f);
    const float rt60 = std::max (0.3f, decaySeconds);
    std::array<float, numSprings> feedback {}, delays {};
    for (size_t i = 0; i < springs.size(); ++i)
    {
        delays[i] = springs[i].baseDelay * lengthScale;
        feedback[i] = static_cast<float> (std::pow (10.0, -3.0 * delays[i] / (static_cast<double> (rt60) * sampleRate)));
    }

    for (int n = 0; n < numSamples; ++n)
    {
        // Springs are thin in the bass: the driver is high-passed.
        inHp += (input[n] - inHp) * inHpCoeff;
        const float x = (input[n] - inHp) * 0.5f;

        // Slow random drift of each spring's length (0.3 %).
        if (--driftCounter <= 0)
        {
            driftCounter = 2048;
            for (auto& s : springs)
                s.driftTarget = rng.nextBipolar() * 0.003f;
        }

        float y[numSprings];
        for (size_t i = 0; i < springs.size(); ++i)
        {
            auto& s = springs[i];
            s.drift += (s.driftTarget - s.drift) * driftCoeff;
            y[i] = processSpring (s, x, feedback[i], delays[i] * (1.0f + s.drift));
        }

        const float l = y[0] + 0.5f * y[2], r = y[1] + 0.5f * y[2];
        outLp[0] += (l - outLp[0]) * outLpCoeff;
        outLp[1] += (r - outLp[1]) * outLpCoeff;
        outL[n] += outLp[0] * level;
        outR[n] += outLp[1] * level;
    }
}

} // namespace augur
