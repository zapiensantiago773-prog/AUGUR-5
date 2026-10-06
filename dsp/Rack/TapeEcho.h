#pragma once

#include "Rack/FxCommon.h"
#include "Rack/RackRng.h"

#include <array>
#include <cstdint>

namespace augur::rack
{

struct TapeEchoParams
{
    int mode = 3;             // head combinations: 0 H1, 1 H2, 2 H3, 3 H1+H2, 4 H2+H3, 5 H1+H3, 6 H1+H2+H3
    double headMs = 177.0;    // head 1 delay: the tape speed (RE range 69..177 ms; extended 25..1000 ms)
    double intensity = 0.45;  // regeneration 0..1 (self-oscillates near the top)
    double wow = 0.25;        // 0..1
    double flutter = 0.2;     // 0..1
    double saturation = 0.35; // record level into the tape 0..1
    double bassDb = 0.0;      // -12..12 (100 Hz shelf)
    double trebleDb = 0.0;    // -12..12 (4.5 kHz shelf)
    double age = 0.25;        // worn tape / heads: HF loss, hiss, more instability 0..1
    double width = 0.6;       // heads panned across the stereo field 0..1
    double mix = 0.3;
};

// Multi-head tape echo: one tape loop (a delay line written by the record head), three playback heads at the
// RE-201's fixed spacing (delays in the ratio 1 : 1.90 : 2.75, from the measured head times 69/131/189 ms and
// 177/337/489 ms). The tape speed sets all three at once and glides like a motor; playback bandwidth follows
// the speed; wow (slow, irregular) and flutter (capstan, ~8 Hz) modulate the read positions.
class TapeEcho
{
public:
    static constexpr double headRatio[3] { 1.0, 1.90, 2.75 };

    void prepare (double sampleRate, std::uint64_t seed);
    void reset() noexcept;
    void process (double& left, double& right, const TapeEchoParams& p) noexcept;
    double headDelayMs (int head) const noexcept { return speedMs * headRatio[head]; }
    bool headActive (int mode, int head) const noexcept;

private:
    double sr = 48000.0, speedMs = 177.0, speedCoeff = 0.0;
    Rng rng;
    DelayLine tape;
    std::array<OnePole, 3> headLoss;
    Shelf bassL, bassR, trebleL, trebleR;
    OnePole fbHp;
    double fbSample = 0.0, wowPhase = 0.0, flutPhase = 0.0, wowRandom = 0.0, wowTarget = 0.0;
    int wowCounter = 0;
    Smooth mixS, intS, satS, widthS;
};

} // namespace augur::rack
