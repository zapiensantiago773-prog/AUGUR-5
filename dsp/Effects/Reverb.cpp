#include "Effects/Reverb.h"

#include <algorithm>
#include <cmath>

namespace augur
{

namespace
{
constexpr std::array<float, Reverb::numLines> baseMs { 29.7f, 37.1f, 41.1f, 43.7f, 53.0f, 59.9f, 67.1f, 73.3f };
constexpr float maxSizeScale = 2.2f;
constexpr float preDelayMs = 18.0f;

int nextPow2 (int n)
{
    int s = 1;
    while (s < n)
        s <<= 1;
    return s;
}

void hadamard8 (float* x) noexcept
{
    for (int len = 1; len < 8; len <<= 1)
        for (int i = 0; i < 8; i += len << 1)
            for (int j = i; j < i + len; ++j)
            {
                const float a = x[j], b = x[j + len];
                x[j] = a + b;
                x[j + len] = a - b;
            }
    constexpr float norm = 0.35355339f; // 1/sqrt(8)
    for (int i = 0; i < 8; ++i)
        x[i] *= norm;
}
} // namespace

void Reverb::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;
    for (size_t i = 0; i < lines.size(); ++i)
    {
        const int maxLen = static_cast<int> (baseMs[i] * 0.001f * maxSizeScale * static_cast<float> (newSampleRate)) + 64;
        const int size = nextPow2 (maxLen);
        lines[i].buf.assign (static_cast<size_t> (size), 0.0f);
        lines[i].mask = size - 1;
    }
    const int preSize = nextPow2 (static_cast<int> (preDelayMs * 0.001 * newSampleRate) + 4);
    preDelay.assign (static_cast<size_t> (preSize), 0.0f);
    preMask = preSize - 1;
    dampCoeff = static_cast<float> (1.0 - std::exp (-6.28318530717959 * 6500.0 / newSampleRate));
    reset();
}

void Reverb::reset() noexcept
{
    for (auto& l : lines)
    {
        std::fill (l.buf.begin(), l.buf.end(), 0.0f);
        l.lp = 0.0f;
    }
    std::fill (preDelay.begin(), preDelay.end(), 0.0f);
    writePos = 0;
}

void Reverb::process (float* left, float* right, int numSamples, float size, float decaySeconds, float mix) noexcept
{
    const float scale = 0.35f + std::clamp (size, 0.0f, 1.0f) * (maxSizeScale - 0.4f);
    const float rt60 = std::max (0.1f, decaySeconds);
    const int preLen = static_cast<int> (preDelayMs * 0.001f * static_cast<float> (sampleRate));

    std::array<float, numLines> lengths {}, gains {};
    for (size_t i = 0; i < lines.size(); ++i)
    {
        lengths[i] = baseMs[i] * 0.001f * scale * static_cast<float> (sampleRate);
        gains[i] = std::pow (10.0f, -3.0f * lengths[i] / (rt60 * static_cast<float> (sampleRate)));
    }

    const double modInc = 0.37 / sampleRate;
    const float modDepth = 0.0006f * static_cast<float> (sampleRate) * 0.5f;

    for (int n = 0; n < numSamples; ++n)
    {
        preDelay[static_cast<size_t> (writePos & preMask)] = 0.5f * (left[n] + right[n]);
        const float in = preDelay[static_cast<size_t> ((writePos - preLen) & preMask)];

        modPhase += modInc;
        if (modPhase >= 1.0)
            modPhase -= 1.0;
        const float mod = std::sin (static_cast<float> (modPhase) * 6.2831853f) * modDepth;

        float x[numLines];
        for (size_t i = 0; i < lines.size(); ++i)
        {
            auto& l = lines[i];
            float len = lengths[i];
            if (i == 1) len += mod;
            if (i == 6) len -= mod;
            const float pos = static_cast<float> (writePos) - len;
            const float fl = std::floor (pos);
            const int idx = static_cast<int> (fl);
            const float t = pos - fl;
            const float a = l.buf[static_cast<size_t> (idx & l.mask)];
            const float b = l.buf[static_cast<size_t> ((idx + 1) & l.mask)];
            float y = a + t * (b - a);
            l.lp += (y - l.lp) * dampCoeff;
            x[i] = l.lp * gains[i];
        }

        const float outL = x[0] + x[2] + x[4] + x[6];
        const float outR = x[1] + x[3] + x[5] + x[7];

        hadamard8 (x);
        for (size_t i = 0; i < lines.size(); ++i)
        {
            const float sign = (i & 1) ? -1.0f : 1.0f;
            lines[i].buf[static_cast<size_t> (writePos & lines[i].mask)] = x[i] + in * sign * 0.35f;
        }

        left[n] += outL * 0.5f * mix;
        right[n] += outR * 0.5f * mix;
        ++writePos;
    }
    writePos &= 0x3fffffff;
}

} // namespace augur
