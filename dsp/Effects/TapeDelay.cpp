#include "Effects/TapeDelay.h"

#include "Util/FastMath.h"

#include <algorithm>
#include <cmath>

namespace augur
{

void TapeDelay::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;
    int size = 1;
    while (size < static_cast<int> (maxSeconds * newSampleRate) + 8)
        size <<= 1;
    bufL.assign (static_cast<size_t> (size), 0.0f);
    bufR.assign (static_cast<size_t> (size), 0.0f);
    mask = size - 1;
    constexpr double twoPi = 6.28318530717959;
    lpCoeff = static_cast<float> (1.0 - std::exp (-twoPi * 5500.0 / newSampleRate));
    hpCoeff = static_cast<float> (1.0 - std::exp (-twoPi * 90.0 / newSampleRate));
    delayCoeff = static_cast<float> (1.0 - std::exp (-1.0 / (0.12 * newSampleRate)));
    smoothedDelay = static_cast<float> (0.375 * newSampleRate);
    reset();
}

void TapeDelay::reset() noexcept
{
    std::fill (bufL.begin(), bufL.end(), 0.0f);
    std::fill (bufR.begin(), bufR.end(), 0.0f);
    writePos = 0;
    lpL = lpR = hpL = hpR = 0.0f;
}

float TapeDelay::read (const std::vector<float>& buf, float delaySamples) const noexcept
{
    const float pos = static_cast<float> (writePos) - delaySamples;
    const float fl = std::floor (pos);
    const int i = static_cast<int> (fl);
    const float t = pos - fl;
    const float a = buf[static_cast<size_t> (i & mask)];
    const float b = buf[static_cast<size_t> ((i + 1) & mask)];
    return a + t * (b - a);
}

void TapeDelay::process (float* left, float* right, int numSamples, float timeSeconds, float feedback, float mix, bool pingPong) noexcept
{
    const float target = std::clamp (timeSeconds, 0.005f, static_cast<float> (maxSeconds) - 0.01f) * static_cast<float> (sampleRate);
    const float fb = std::clamp (feedback, 0.0f, 1.0f) * 1.05f; // can run slightly hot; saturation keeps it bounded
    const float cross = pingPong ? 1.0f : 0.25f;
    const float spread = pingPong ? 1.0f : 1.0125f; // ping-pong keeps both sides on the beat

    for (int n = 0; n < numSamples; ++n)
    {
        smoothedDelay += (target - smoothedDelay) * delayCoeff;

        const float wetL = read (bufL, smoothedDelay);
        const float wetR = read (bufR, smoothedDelay * spread);

        // Feedback path: band-limit + saturate.
        float fL = (1.0f - cross) * wetL + cross * wetR;
        float fR = (1.0f - cross) * wetR + cross * wetL;
        lpL += (fL - lpL) * lpCoeff;
        lpR += (fR - lpR) * lpCoeff;
        hpL += (lpL - hpL) * hpCoeff;
        hpR += (lpR - hpR) * hpCoeff;
        fL = fastmath::tanh ((lpL - hpL) * fb);
        fR = fastmath::tanh ((lpR - hpR) * fb);

        const size_t w = static_cast<size_t> (writePos & mask);
        if (pingPong)
        {
            bufL[w] = 0.5f * (left[n] + right[n]) + fL;
            bufR[w] = fR;
        }
        else
        {
            bufL[w] = left[n] + fL;
            bufR[w] = right[n] + fR;
        }

        left[n] += wetL * mix;
        right[n] += wetR * mix;
        ++writePos;
    }
    writePos &= mask;
}

} // namespace augur
