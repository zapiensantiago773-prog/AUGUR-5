#include "Effects/TapeEcho.h"

#include "Util/FastMath.h"

#include <algorithm>
#include <cmath>

namespace augur
{

namespace
{
constexpr double twoPi = 6.28318530717959;
constexpr std::array<float, 3> headRatio { 1.0f, 1.95f, 2.9f };

// Heads used by each selector position (bit 0 = head 1) and whether the spring is on.
struct ModeEntry
{
    int heads;
    bool reverb;
};
constexpr std::array<ModeEntry, 12> modes { {
    { 0b001, false }, { 0b010, false }, { 0b100, false }, { 0b110, false },
    { 0b001, true }, { 0b010, true }, { 0b100, true }, { 0b011, true },
    { 0b110, true }, { 0b101, true }, { 0b111, true }, { 0b000, true },
} };

// Tape + record-amp saturation: soft, slightly asymmetric (even harmonics of the bias-less head).
inline float tapeSaturate (float x) noexcept
{
    return fastmath::tanh (x + 0.08f * x * x) - fastmath::tanh (0.0f);
}
} // namespace

double TapeEcho::head1Seconds (float rate) noexcept
{
    // Slow .. fast: 250 ms .. 55 ms on head 1 (heads 2 / 3 follow at 1.95x / 2.9x, up to ~725 ms).
    const double r = std::clamp (static_cast<double> (rate), 0.0, 1.0);
    return 0.250 * std::pow (0.055 / 0.250, r);
}

void TapeEcho::prepare (double newSampleRate)
{
    sampleRate = newSampleRate;
    int size = 1;
    while (size < static_cast<int> (maxSeconds * newSampleRate) + 8)
        size <<= 1;
    tape.assign (static_cast<size_t> (size), 0.0f);
    mask = size - 1;

    speedCoeff = static_cast<float> (1.0 - std::exp (-1.0 / (0.25 * newSampleRate)));   // transport inertia
    driftCoeff = static_cast<float> (1.0 - std::exp (-1.0 / (0.4 * newSampleRate)));
    inLpCoeff = static_cast<float> (1.0 - std::exp (-twoPi * 7000.0 / newSampleRate));
    hpCoeff = static_cast<float> (1.0 - std::exp (-twoPi * 110.0 / newSampleRate));
    lpCoeff = static_cast<float> (1.0 - std::exp (-twoPi * 3800.0 / newSampleRate));   // playback bandwidth
    bassCoeff = static_cast<float> (1.0 - std::exp (-twoPi * 250.0 / newSampleRate));
    trebleCoeff = static_cast<float> (1.0 - std::exp (-twoPi * 2500.0 / newSampleRate));

    springIn.assign (4096, 0.0f);
    springL.assign (4096, 0.0f);
    springR.assign (4096, 0.0f);
    spring.prepare (newSampleRate, 0xEC40u);
    rng.setSeed (0x7A9Eu);
    reset();
}

void TapeEcho::reset() noexcept
{
    std::fill (tape.begin(), tape.end(), 0.0f);
    writePos = 0;
    speed = -1.0f; // snap to the first setting
    wowPhase = flutterPhase = 0.0;
    drift = driftTarget = 0.0f;
    driftCounter = 0;
    inLp = fbHp = fbLp = bassState = trebleState = feedbackSample = 0.0f;
    spring.reset();
}

float TapeEcho::readTape (float delaySamples) const noexcept
{
    // 4-point Hermite: the heads read a moving tape.
    const float pos = static_cast<float> (writePos) - delaySamples;
    const float fl = std::floor (pos);
    const int i = static_cast<int> (fl);
    const float t = pos - fl;
    const float xm1 = tape[static_cast<size_t> ((i - 1) & mask)];
    const float x0 = tape[static_cast<size_t> (i & mask)];
    const float x1 = tape[static_cast<size_t> ((i + 1) & mask)];
    const float x2 = tape[static_cast<size_t> ((i + 2) & mask)];
    const float c1 = 0.5f * (x1 - xm1);
    const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
    const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
    return ((c3 * t + c2) * t + c1) * t + x0;
}

void TapeEcho::process (float* left, float* right, int numSamples, const Settings& st, float mix) noexcept
{
    const auto& mode = modes[static_cast<size_t> (std::clamp (st.mode, 0, 11))];
    const float head1 = static_cast<float> (head1Seconds (st.rate) * sampleRate);
    if (speed < 0.0f)
        speed = head1;
    const float feedback = std::clamp (st.intensity, 0.0f, 1.0f) * 1.15f; // > 1: runaway, held by saturation
    const float drive = 0.5f + 2.5f * std::clamp (st.input, 0.0f, 1.0f);
    const float wowDepth = 0.0045f * std::clamp (st.wow, 0.0f, 1.0f);
    const float bassGain = st.bass * 1.2f, trebleGain = st.treble * 1.2f;
    const double wowInc = 0.55 / sampleRate, flutterInc = 7.3 / sampleRate;
    const int activeHeads = (mode.heads & 1) + ((mode.heads >> 1) & 1) + ((mode.heads >> 2) & 1);
    const float headNorm = activeHeads > 0 ? 1.0f / std::sqrt (static_cast<float> (activeHeads)) : 0.0f;

    for (int start = 0; start < numSamples; start += 4096)
    {
        const int count = std::min (4096, numSamples - start);
        for (int k = 0; k < count; ++k)
        {
            const int n = start + k;
            const float dry = 0.5f * (left[n] + right[n]);

            // Transport: the delay follows REPEAT RATE with inertia (pitch glide), plus wow & flutter.
            speed += (head1 - speed) * speedCoeff;
            wowPhase += wowInc;
            flutterPhase += flutterInc;
            if (wowPhase >= 1.0)
                wowPhase -= 1.0;
            if (flutterPhase >= 1.0)
                flutterPhase -= 1.0;
            if (--driftCounter <= 0)
            {
                driftCounter = 1024;
                driftTarget = rng.nextBipolar();
            }
            drift += (driftTarget - drift) * driftCoeff;
            const float mod = 1.0f + wowDepth * (0.6f * static_cast<float> (std::sin (twoPi * wowPhase))
                                                 + 0.25f * static_cast<float> (std::sin (twoPi * flutterPhase)) + 0.3f * drift);

            // Playback heads.
            float heads = 0.0f, pan = 0.0f;
            for (int h = 0; h < 3; ++h)
            {
                if ((mode.heads >> h & 1) == 0)
                    continue;
                const float d = std::min (speed * headRatio[static_cast<size_t> (h)] * mod, static_cast<float> (mask - 4));
                const float v = readTape (d);
                heads += v;
                pan += v * (h == 0 ? -0.3f : (h == 1 ? 0.3f : 0.0f)); // heads 1 / 2 slightly apart in stereo
            }
            heads *= headNorm;
            pan *= headNorm;

            // Playback preamp: tape bandwidth, then BASS / TREBLE shelves (they shape the repeats too).
            fbLp += (heads - fbLp) * lpCoeff;
            fbHp += (fbLp - fbHp) * hpCoeff;
            float play = fbLp - fbHp;
            bassState += (play - bassState) * bassCoeff;
            trebleState += (play - trebleState) * trebleCoeff;
            play += bassGain * bassState + trebleGain * (play - trebleState);

            // Record head: input (band-limited) + feedback, saturated onto the tape.
            inLp += (dry - inLp) * inLpCoeff;
            const float rec = tapeSaturate ((inLp * drive + feedback * feedbackSample) * 0.8f) * 1.25f;
            writePos = (writePos + 1) & mask;
            tape[static_cast<size_t> (writePos)] = rec / drive;
            feedbackSample = play;

            const float echo = play * st.echoLevel * mix;
            left[n] += echo - pan * st.echoLevel * mix;
            right[n] += echo + pan * st.echoLevel * mix;
            springIn[static_cast<size_t> (k)] = mode.reverb ? 0.6f * (dry + play) : 0.0f;
            springL[static_cast<size_t> (k)] = springR[static_cast<size_t> (k)] = 0.0f;
        }

        // Spring reverb on the input and the echoes.
        if (mode.reverb || st.reverbLevel > 0.0f)
        {
            spring.process (springIn.data(), springL.data(), springR.data(), count, 2.6f, 0.5f, st.reverbLevel * mix);
            for (int k = 0; k < count; ++k)
            {
                left[start + k] += springL[static_cast<size_t> (k)];
                right[start + k] += springR[static_cast<size_t> (k)];
            }
        }
    }
}

} // namespace augur
