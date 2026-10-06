#pragma once

#include "Rack/FxCommon.h"

#include <array>
#include <atomic>

namespace augur::rack
{

struct BusCompParams
{
    double thresholdDb = -12.0; // -40 .. 0
    int ratioIndex = 1;         // 1.5, 2, 3, 4, 5, 10, LIMIT (G-console 2/4/10 plus the 500-series in-betweens)
    int attackIndex = 3;        // 0.1, 0.3, 1, 3, 10, 30 ms
    int releaseIndex = 4;       // 0.1, 0.3, 0.6, 1.2 s, AUTO
    double makeupDb = 0.0;      // 0 .. 20
    double scHpfHz = 20.0;      // side-chain high-pass 20 .. 300 Hz (keeps the bass from pumping everything)
    double mix = 1.0;           // parallel compression
};

// Stereo bus compressor of the G-console lineage: a VCA whose control voltage comes from a detector fed by
// the gain-reduced signal (feedback detection, which makes the "glue"), soft knee, stepped attack / release
// and the program-dependent AUTO release (a fast and a slow time constant in series). Linked stereo.
class BusComp
{
public:
    static constexpr double ratios[7] { 1.5, 2.0, 3.0, 4.0, 5.0, 10.0, 50.0 };
    static constexpr double attacksMs[6] { 0.1, 0.3, 1.0, 3.0, 10.0, 30.0 };
    static constexpr double releasesS[5] { 0.1, 0.3, 0.6, 1.2, 0.0 }; // 0 = AUTO

    void prepare (double sampleRate);
    void reset() noexcept;
    void process (double& left, double& right, const BusCompParams& p) noexcept;
    float gainReductionDb() const noexcept { return grMeter.load (std::memory_order_relaxed); }

private:
    double sr = 48000.0;
    std::array<OnePole, 2> scHp;
    double envDb = 0.0, autoFast = 0.0, autoSlow = 0.0, lastGain = 1.0;
    std::atomic<float> grMeter { 0.0f };
    Smooth makeupS, mixS;
};

} // namespace augur::rack
