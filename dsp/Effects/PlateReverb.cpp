#include "Effects/PlateReverb.h"

#include <algorithm>
#include <cmath>

namespace augur
{

namespace
{
constexpr double paperRate = 29761.0;
constexpr float maxSize = 1.5f; // SIZE 1 -> lengths x 1.5, SIZE 0 -> x 0.5

// Tank lengths (paper samples).
constexpr double lenApL = 672.0, lenDelL1 = 4453.0, lenAp2L = 1800.0, lenDelL2 = 3720.0;
constexpr double lenApR = 908.0, lenDelR1 = 4217.0, lenAp2R = 2656.0, lenDelR2 = 3163.0;
constexpr double excursion = 16.0;
} // namespace

void PlateReverb::Line::allocate (int maxLength)
{
    int size = 1;
    while (size < maxLength + 4)
        size <<= 1;
    buf.assign (static_cast<size_t> (size), 0.0f);
    mask = size - 1;
    write = 0;
}

void PlateReverb::Line::clear() noexcept
{
    std::fill (buf.begin(), buf.end(), 0.0f);
    write = 0;
}

float PlateReverb::Line::tapFrac (float delay) const noexcept
{
    const int i = static_cast<int> (delay);
    const float t = delay - static_cast<float> (i);
    const float a = tap (i), b = tap (i + 1);
    return a + (b - a) * t;
}

float PlateReverb::allpass (Line& line, int delay, float g, float x) noexcept
{
    const float d = line.tap (delay - 1); // the value pushed `delay` samples ago once x is pushed
    const float v = x + g * d;
    line.push (v);
    return d - g * v;
}

float PlateReverb::allpassFrac (Line& line, float delay, float g, float x) noexcept
{
    const float d = line.tapFrac (delay - 1.0f);
    const float v = x + g * d;
    line.push (v);
    return d - g * v;
}

int PlateReverb::len (double paperSamples, float size) const noexcept
{
    return std::max (2, static_cast<int> (paperSamples * scale * (0.5 + static_cast<double> (size))));
}

void PlateReverb::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;
    scale = newSampleRate / paperRate;
    const auto maxLen = [this] (double paper) { return static_cast<int> ((paper + excursion) * scale * maxSize) + 4; };
    pre.allocate (static_cast<int> (0.1 * newSampleRate));
    in1.allocate (maxLen (142.0));
    in2.allocate (maxLen (107.0));
    in3.allocate (maxLen (379.0));
    in4.allocate (maxLen (277.0));
    apL.allocate (maxLen (lenApL));
    delL1.allocate (maxLen (lenDelL1));
    ap2L.allocate (maxLen (lenAp2L));
    delL2.allocate (maxLen (lenDelL2));
    apR.allocate (maxLen (lenApR));
    delR1.allocate (maxLen (lenDelR1));
    ap2R.allocate (maxLen (lenAp2R));
    delR2.allocate (maxLen (lenDelR2));
    reset();
}

void PlateReverb::reset() noexcept
{
    for (Line* l : { &pre, &in1, &in2, &in3, &in4, &apL, &delL1, &ap2L, &delL2, &apR, &delR1, &ap2R, &delR2 })
        l->clear();
    bandwidthState = dampL = dampR = feedL = feedR = 0.0f;
    lfoPhase = 0.0;
}

void PlateReverb::process (float* left, float* right, int numSamples, float size, float decaySeconds, float mix) noexcept
{
    size = std::clamp (size, 0.0f, 1.0f);
    const int preDelay = std::max (1, static_cast<int> (0.012 * sampleRate * (0.5 + size)));
    const int d1 = len (142.0, size), d2 = len (107.0, size), d3 = len (379.0, size), d4 = len (277.0, size);
    const int dL1 = len (lenDelL1, size), dAp2L = len (lenAp2L, size), dL2 = len (lenDelL2, size);
    const int dR1 = len (lenDelR1, size), dAp2R = len (lenAp2R, size), dR2 = len (lenDelR2, size);
    const float apLBase = static_cast<float> (lenApL * scale * (0.5 + size));
    const float apRBase = static_cast<float> (lenApR * scale * (0.5 + size));
    const float exc = static_cast<float> (excursion * scale);

    // Decay gain from the RT60: each tank half applies it twice (after the damping and at its end), so
    // decay^2 per half = 10^(-3 * T_half / RT60).
    const double halfSeconds = (lenApL + lenDelL1 + lenAp2L + lenDelL2) * scale * (0.5 + size) / sampleRate;
    const float decay = static_cast<float> (std::min (0.99, std::pow (10.0, -1.5 * halfSeconds / std::max (0.1, static_cast<double> (decaySeconds)))));
    const float decayDiffusion2 = std::clamp (decay + 0.15f, 0.25f, 0.5f);

    const float bandwidth = static_cast<float> (1.0 - std::exp (-2.0 * 3.14159265358979 * 9000.0 / sampleRate));
    const float damping = static_cast<float> (1.0 - std::exp (-2.0 * 3.14159265358979 * 5500.0 / sampleRate));
    const double lfoInc = 1.0 / sampleRate; // 1 Hz tank modulation

    // Output tap positions (paper samples, scaled).
    const auto t = [&] (double paper) { return std::max (1, static_cast<int> (paper * scale * (0.5 + size))); };
    const int tL[7] = { t (266), t (2974), t (1913), t (1996), t (1990), t (187), t (1066) };
    const int tR[7] = { t (353), t (3627), t (1228), t (2673), t (2111), t (335), t (121) };

    for (int n = 0; n < numSamples; ++n)
    {
        const float x = 0.5f * (left[n] + right[n]);
        pre.push (x);
        float s = pre.tap (preDelay);
        bandwidthState += (s - bandwidthState) * bandwidth;
        s = allpass (in1, d1, 0.75f, bandwidthState);
        s = allpass (in2, d2, 0.75f, s);
        s = allpass (in3, d3, 0.625f, s);
        s = allpass (in4, d4, 0.625f, s);

        lfoPhase += lfoInc;
        if (lfoPhase >= 1.0)
            lfoPhase -= 1.0;
        const float mod = static_cast<float> (std::sin (6.28318530717959 * lfoPhase));

        // Left half (fed by the right half's end), then the right half (fed by the left's end).
        float a = s + feedR;
        a = allpassFrac (apL, apLBase + exc * mod, -0.7f, a);
        delL1.push (a);
        a = delL1.tap (dL1);
        dampL += (a - dampL) * damping;
        a = allpass (ap2L, dAp2L, decayDiffusion2, dampL * decay);
        delL2.push (a);
        const float endL = delL2.tap (dL2) * decay;

        float b = s + feedL;
        b = allpassFrac (apR, apRBase - exc * mod, -0.7f, b);
        delR1.push (b);
        b = delR1.tap (dR1);
        dampR += (b - dampR) * damping;
        b = allpass (ap2R, dAp2R, decayDiffusion2, dampR * decay);
        delR2.push (b);
        const float endR = delR2.tap (dR2) * decay;

        feedL = endL;
        feedR = endR;

        const float yl = delR1.tap (tL[0]) + delR1.tap (tL[1]) - ap2R.tap (tL[2]) + delR2.tap (tL[3]) - delL1.tap (tL[4])
                         - ap2L.tap (tL[5]) - delL2.tap (tL[6]);
        const float yr = delL1.tap (tR[0]) + delL1.tap (tR[1]) - ap2L.tap (tR[2]) + delL2.tap (tR[3]) - delR1.tap (tR[4])
                         - ap2R.tap (tR[5]) - delR2.tap (tR[6]);

        left[n] += 0.3f * yl * mix;
        right[n] += 0.3f * yr * mix;
    }
}

} // namespace augur
