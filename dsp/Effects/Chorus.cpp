#include "Effects/Chorus.h"

#include "Util/FastMath.h"

#include <cmath>

namespace augur
{

void Chorus::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;
    int size = 1;
    while (size < static_cast<int> (0.05 * newSampleRate) + 8)
        size <<= 1;
    buffer.assign (static_cast<size_t> (size), 0.0f);
    mask = size - 1;
    lpCoeff = static_cast<float> (1.0 - std::exp (-2.0 * 3.14159265358979 * 9000.0 / newSampleRate));
    reset();
}

void Chorus::reset() noexcept
{
    std::fill (buffer.begin(), buffer.end(), 0.0f);
    writePos = 0;
    lpL = lpR = 0.0f;
}

float Chorus::read (float delaySamples) const noexcept
{
    // 4-point Hermite interpolation.
    const float pos = static_cast<float> (writePos) - delaySamples;
    const float fl = std::floor (pos);
    const int i = static_cast<int> (fl);
    const float t = pos - fl;
    const float xm1 = buffer[static_cast<size_t> ((i - 1) & mask)];
    const float x0 = buffer[static_cast<size_t> (i & mask)];
    const float x1 = buffer[static_cast<size_t> ((i + 1) & mask)];
    const float x2 = buffer[static_cast<size_t> ((i + 2) & mask)];
    const float c1 = 0.5f * (x1 - xm1);
    const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
    const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
    return ((c3 * t + c2) * t + c1) * t + x0;
}

void Chorus::process (float* left, float* right, int numSamples, float rateHz, float depth, float mix, int mode) noexcept
{
    double rate = rateHz, centre = 0.0065, swing = 0.0005 + 0.0045 * depth; // seconds
    switch (mode)
    {
        case 1: rate = 0.513; centre = 0.003505; swing = 0.001845; break;
        case 2: rate = 0.863; centre = 0.003505; swing = 0.001845; break;
        case 3: rate = 9.75;  centre = 0.0035;   swing = 0.0002;   break;
        default: break;
    }
    const double inc = rate / sampleRate;
    const float baseDelay = static_cast<float> (centre * sampleRate);
    const float modDepth = static_cast<float> (swing * sampleRate);

    for (int n = 0; n < numSamples; ++n)
    {
        const float dryL = left[n], dryR = right[n];
        buffer[static_cast<size_t> (writePos & mask)] = 0.5f * (dryL + dryR);

        phase += inc;
        if (phase >= 1.0)
            phase -= 1.0;
        const float tri = 1.0f - 4.0f * std::abs (static_cast<float> (phase) - 0.5f);

        float wetL = read (baseDelay + modDepth * tri);
        float wetR = read (baseDelay - modDepth * tri);
        lpL += (wetL - lpL) * lpCoeff;
        lpR += (wetR - lpR) * lpCoeff;
        wetL = fastmath::tanh (lpL * 1.2f) * (1.0f / 1.2f);
        wetR = fastmath::tanh (lpR * 1.2f) * (1.0f / 1.2f);

        left[n] = dryL * (1.0f - 0.5f * mix) + wetL * mix;
        right[n] = dryR * (1.0f - 0.5f * mix) + wetR * mix;
        ++writePos;
    }
    writePos &= mask;
}

} // namespace augur
