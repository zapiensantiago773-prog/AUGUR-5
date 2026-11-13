// Faults found by the whole-instrument audit of D-061, kept as regression tests: the tape echo's heads and the reverb's
// early reflections leaning to a side, FOLD's level, clicks when a selector moves under a sounding note (rack modes,
// filter MODEL / slope) and steps when PAN, SPREAD or the HPF are moved.
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Engine/SynthEngine.h"
#include "Rack/FxRack.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <memory>
#include <vector>

using namespace augur;
using namespace augur::rack;

namespace
{
constexpr double sr = 48000.0;

// Bursts of pink noise, centred: nothing periodic for a delay line to resonate with.
struct Pink
{
    unsigned s = 12345;
    double b0 = 0.0, b1 = 0.0, b2 = 0.0;
    double next (double t) noexcept
    {
        s = s * 1664525u + 1013904223u;
        const double w = static_cast<double> (s >> 8) / 8388608.0 - 1.0;
        b0 = 0.99765 * b0 + w * 0.0990460;
        b1 = 0.96300 * b1 + w * 0.2965164;
        b2 = 0.57000 * b2 + w * 1.0526913;
        const double burst = 0.5 + 0.5 * std::sin (2.0 * pi * 1.3 * t);
        return 0.03 * (b0 + b1 + b2 + w * 0.1848) * burst * burst;
    }
};

struct Levels
{
    double gainDb = 0.0, balanceDb = 0.0;
};

Levels throughRack (const std::function<void (FxParams&)>& setup, FxId id)
{
    FxRack rack;
    rack.prepare (sr, 0x5eedu);
    FxParams p;
    setup (p);
    p.on[static_cast<size_t> (id)] = true;
    Pink src;
    double in = 0.0, l2 = 0.0, r2 = 0.0;
    const long n = static_cast<long> (12 * sr), skip = static_cast<long> (sr);
    for (long i = 0; i < n; ++i)
    {
        const double x = src.next (static_cast<double> (i) / sr);
        double l = x, r = x;
        rack.process (l, r, p, 120.0);
        if (i >= skip)
        {
            in += x * x;
            l2 += l * l;
            r2 += r * r;
        }
    }
    return { 10.0 * std::log10 ((l2 + r2) / (2.0 * in)), 10.0 * std::log10 (l2 / r2) };
}

// Largest second difference just after `at` over the largest in steady playing before and after.
double clickRatio (const std::vector<double>& l, const std::vector<double>& r, double at)
{
    const auto d2 = [&] (double a, double b) {
        double m = 0.0;
        for (auto i = static_cast<size_t> (a * sr); i < static_cast<size_t> (b * sr); ++i)
            m = std::max ({ m, std::abs (l[i] - 2.0 * l[i - 1] + l[i - 2]), std::abs (r[i] - 2.0 * r[i - 1] + r[i - 2]) });
        return m;
    };
    return d2 (at - 0.001, at + 0.2) / std::max (d2 (at - 1.5, at - 0.05), d2 (at + 1.5, at + 2.9));
}

double rackSwitch (FxId id, const std::function<void (FxParams&, bool)>& set)
{
    FxRack rack;
    rack.prepare (sr, 0x5eedu);
    FxParams p;
    p.on[static_cast<size_t> (id)] = true;
    const auto n = static_cast<size_t> (6 * sr);
    std::vector<double> yl (n), yr (n);
    for (size_t i = 0; i < n; ++i)
    {
        const double t = static_cast<double> (i) / sr;
        const double x = 0.15 * std::sin (2.0 * pi * 110.0 * t) + 0.05 * std::sin (2.0 * pi * 1100.0 * t);
        set (p, t >= 3.0);
        yl[i] = yr[i] = x;
        rack.process (yl[i], yr[i], p, 120.0);
    }
    return clickRatio (yl, yr, 3.0);
}

double engineSwitch (const std::function<void (SynthParams&, bool)>& set)
{
    SynthParams p;
    p.cutoffHz = 400.0f;
    p.envAmount = 0.0f;
    p.resonance = 0.0f;
    p.analogAge = 0.0f;
    p.aenvS = 1.0f;
    p.keytrack = 0;
    auto e = std::make_unique<SynthEngine>();
    e->prepare (sr, SynthEngine::defaultUnitSeed, 1);
    set (p, false);
    e->setParams (p);
    e->warmUp();
    const auto n = static_cast<size_t> (6 * sr);
    std::vector<float> l (n), r (n);
    for (int note : { 48, 55, 64 })
        e->noteOn (note, 0.8f);
    for (size_t pos = 0; pos < n; pos += 32)
    {
        set (p, static_cast<double> (pos) / sr >= 3.0);
        e->setParams (p);
        e->process (l.data() + pos, r.data() + pos, static_cast<int> (std::min<size_t> (32, n - pos)));
    }
    return clickRatio (std::vector<double> (l.begin(), l.end()), std::vector<double> (r.begin(), r.end()), 3.0);
}
} // namespace

TEST_CASE ("Tape echo: the playing heads sit centred in every mode", "[fx][echo][audit]")
{
    // Fixed places (H1 left, H2 centre, H3 right) put a single head's echo 2.7 dB to one side and a pair 1.2 dB off.
    const int mode = GENERATE (0, 1, 2, 3, 4, 5, 6);
    const auto wet = throughRack ([mode] (FxParams& p) { p.echo.mode = mode; p.echo.mix = 1.0; }, FxId::echo);
    WARN ("echo mode " << mode << ": wet balance " << wet.balanceDb << " dB");
    CHECK (std::abs (wet.balanceDb) < 0.2);
}

TEST_CASE ("Reverb: every type is centred on a centred sound", "[fx][reverb][audit]")
{
    // The early reflections' left taps carried 1.5 dB more than the right ones: ROOM, HALL and SHIMMER leaned 0.4 dB.
    const int type = GENERATE (0, 1, 2, 3, 4);
    const auto wet = throughRack ([type] (FxParams& p) {
                                      p.reverb.mix = 1.0;
                                      if (type == 4)
                                          p.reverbSpring = true;
                                      else
                                          p.reverb.type = static_cast<ReverbType> (type);
                                  },
                                  FxId::reverb);
    WARN ("reverb type " << type << ": wet balance " << wet.balanceDb << " dB");
    CHECK (std::abs (wet.balanceDb) < 0.3);
}

TEST_CASE ("Drive FOLD keeps the level of a line signal", "[fx][drive][audit]")
{
    // It had no make-up: +14.6 dB at the default 12 dB of DRIVE.
    const double driveDb = GENERATE (0.0, 12.0, 24.0, 36.0);
    const auto out = throughRack ([driveDb] (FxParams& p) { p.drive.model = DriveModel::fold; p.drive.driveDb = driveDb; }, FxId::drive);
    WARN ("FOLD at " << driveDb << " dB: " << out.gainDb << " dB");
    // Deep in the folds the output settles at the nominal line level (0.1 rms) whatever comes in, like the other
    // models' ceilings; this test signal is quieter than that, so it comes up to it (+8 dB), far from +14.6 .. +40.
    CHECK (std::abs (out.gainDb) < (driveDb <= 12.0 ? 3.0 : 9.0));
}

TEST_CASE ("Rack: a selector moved under a sounding note does not click", "[fx][rack][audit]")
{
    struct Case
    {
        const char* name;
        FxId id;
        std::function<void (FxParams&, bool)> set;
    };
    const Case cases[] = {
        { "drive tube -> diode", FxId::drive, [] (FxParams& p, bool a) { p.drive.model = a ? DriveModel::diode : DriveModel::tube; } },
        { "drive tube -> fold", FxId::drive, [] (FxParams& p, bool a) { p.drive.model = a ? DriveModel::fold : DriveModel::tube; } },
        { "chorus juno1 -> dimension", FxId::chorus, [] (FxParams& p, bool a) { p.chorus.mode = a ? ChorusMode::dimension : ChorusMode::juno1; } },
        { "chorus juno1 -> ensemble", FxId::chorus, [] (FxParams& p, bool a) { p.chorus.mode = a ? ChorusMode::ensemble : ChorusMode::juno1; } },
        { "phaser 4 -> 12 stages", FxId::phaser, [] (FxParams& p, bool a) { p.phaser.stages = a ? 12 : 4; } },
        { "flanger through-zero on", FxId::flanger, [] (FxParams& p, bool a) { p.flanger.throughZero = a; } },
        { "delay ping-pong on", FxId::delay, [] (FxParams& p, bool a) { p.delay.pingPong = a; } },
        { "echo H1+H2 -> all heads", FxId::echo, [] (FxParams& p, bool a) { p.echo.mode = a ? 6 : 3; } },
        { "echo heads off", FxId::echo, [] (FxParams& p, bool a) { p.echoHeadsOff = a; p.echoSpring = 0.35; } },
        { "reverb plate -> room", FxId::reverb, [] (FxParams& p, bool a) { p.reverb.type = a ? ReverbType::room : ReverbType::plate; } },
        { "reverb plate -> hall", FxId::reverb, [] (FxParams& p, bool a) { p.reverb.type = a ? ReverbType::hall : ReverbType::plate; } },
        { "reverb plate -> spring", FxId::reverb, [] (FxParams& p, bool a) { p.reverbSpring = a; } },
        { "hall decay 0.2 -> 30 s", FxId::reverb, [] (FxParams& p, bool a) { p.reverb.type = ReverbType::hall; p.reverb.decayS = a ? 30.0 : 0.2; } },
    };
    for (const auto& c : cases)
    {
        const double ratio = rackSwitch (c.id, c.set);
        INFO (c.name << ": " << ratio);
        CHECK (ratio < 2.5); // 6 .. 100 before
    }
}

TEST_CASE ("Engine: filter switches, HPF, PAN and SPREAD moved under a held chord do not click", "[engine][audit]")
{
    struct Case
    {
        const char* name;
        std::function<void (SynthParams&, bool)> set;
        double limit;
    };
    const Case cases[] = {
        { "filter model 0 -> 1", [] (SynthParams& p, bool a) { p.filterModel = a ? 1 : 0; }, 4.0 },       // 326 before
        { "filter slope 24 -> 12 dB", [] (SynthParams& p, bool a) { p.filterSlope = a ? 1 : 0; }, 2.5 },  // 61
        { "HPF 20 -> 1000 Hz", [] (SynthParams& p, bool a) { p.hpfHz = a ? 1000.0f : 20.0f; }, 2.5 },      // 29
        { "PAN -1 -> 1", [] (SynthParams& p, bool a) { p.voicePan = a ? 1.0f : -1.0f; }, 2.5 },            // 10
        { "SPREAD 0 -> 1", [] (SynthParams& p, bool a) { p.voiceSpread = a ? 1.0f : 0.0f; }, 2.5 },        // 9
    };
    for (const auto& c : cases)
    {
        const double ratio = engineSwitch (c.set);
        INFO (c.name << ": " << ratio);
        CHECK (ratio < c.limit);
    }
}
