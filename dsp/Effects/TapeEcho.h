#pragma once

#include "Effects/SpringReverb.h"
#include "Util/Random.h"

#include <array>
#include <vector>

namespace augur
{

// Multi-head tape echo with spring reverb, in the manner of the classic 1970s studio tape-loop units:
//  - one record head and three playback heads on a tape loop (spacing 1 : 1.95 : 2.9);
//  - REPEAT RATE = tape speed: all three delays scale together and a speed change glides the pitch;
//  - INTENSITY = playback fed back to the record head, up to self-oscillation ("runaway"), held by the
//    tape's saturation;
//  - tape bandwidth (dark repeats), BASS / TREBLE on the playback preamp (so they shape the repeats too),
//    wow & flutter;
//  - a 12-position MODE selector: 1-4 echo only, 5-11 echo + spring reverb, 12 spring reverb only.
class TapeEcho
{
public:
    struct Settings
    {
        int mode = 3;          // 0..11 = selector positions 1..12
        float rate = 0.5f;     // 0 = slow (long), 1 = fast (short)
        float intensity = 0.45f;
        float bass = 0.0f, treble = 0.0f; // -1..1
        float wow = 0.4f;      // wow & flutter amount 0..1
        float input = 0.5f;    // record level (tape drive) 0..1
        float echoLevel = 0.5f, reverbLevel = 0.35f;
    };

    static constexpr double maxSeconds = 2.4;

    void prepare (double sampleRate);
    void reset() noexcept;

    // Send-style: adds the echo and reverb (scaled by mix) to the dry stereo signal in place.
    void process (float* left, float* right, int numSamples, const Settings& s, float mix) noexcept;

    // Head 1 delay in seconds at a REPEAT RATE setting (heads 2 and 3 are 1.95x and 2.9x).
    static double head1Seconds (float rate) noexcept;

private:
    float readTape (float delaySamples) const noexcept;

    double sampleRate = 48000.0;
    std::vector<float> tape;
    int mask = 0, writePos = 0;
    float speed = 1.0f, speedCoeff = 0.0f;        // smoothed tape transport
    double wowPhase = 0.0, flutterPhase = 0.0;
    float drift = 0.0f, driftTarget = 0.0f, driftCoeff = 0.0f;
    int driftCounter = 0;
    float inLp = 0.0f, inLpCoeff = 0.5f;           // record bandwidth
    float fbHp = 0.0f, fbLp = 0.0f, hpCoeff = 0.01f, lpCoeff = 0.3f;
    float bassState = 0.0f, trebleState = 0.0f, bassCoeff = 0.01f, trebleCoeff = 0.2f;
    float feedbackSample = 0.0f;
    std::vector<float> springIn, springL, springR;
    SpringReverb spring;
    Random rng;
};

} // namespace augur
