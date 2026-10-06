#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "RackMeasure.h"

#include "Rack/FxRack.h"

#include <cmath>
#include <functional>
#include <vector>

using namespace augur;
using namespace augur::rack;
namespace m = augur::rackmeasure;

namespace
{
constexpr double sr = 48000.0;

struct Stereo
{
    std::vector<float> l, r;
};

template <typename Fx>
Stereo run (Fx& fxUnit, const std::function<void (Fx&, double&, double&)>& step, const std::function<double (size_t)>& input, double seconds)
{
    Stereo s;
    const auto n = static_cast<size_t> (seconds * sr);
    s.l.resize (n);
    s.r.resize (n);
    for (size_t i = 0; i < n; ++i)
    {
        double l = input (i), r = l;
        step (fxUnit, l, r);
        s.l[i] = static_cast<float> (l);
        s.r[i] = static_cast<float> (r);
    }
    return s;
}

std::function<double (size_t)> sine (double hz, double amp)
{
    return [hz, amp] (size_t n) { return amp * std::sin (2.0 * m::pi * hz * static_cast<double> (n) / sr); };
}

std::function<double (size_t)> impulse()
{
    return [] (size_t n) { return n == 0 ? 1.0 : 0.0; };
}

bool finiteAndBounded (const Stereo& s, float limit)
{
    for (size_t i = 0; i < s.l.size(); ++i)
        if (! std::isfinite (s.l[i]) || ! std::isfinite (s.r[i]) || std::abs (s.l[i]) > limit || std::abs (s.r[i]) > limit)
            return false;
    return true;
}

double rmsDb (const std::vector<float>& x, size_t from = 0) { return 20.0 * std::log10 (std::max (m::rms (x, from), 1.0e-12)); }
} // namespace

TEST_CASE ("Rack: every effect stays finite and bounded at extreme settings", "[fx]")
{
    FxRack rack;
    rack.prepare (sr, 1);
    FxParams p;
    p.on.fill (true);
    p.drive.driveDb = 48.0;
    p.chorus.depth = 2.0;
    p.chorus.rate = 4.0;
    p.phaser.feedback = 0.95;
    p.phaser.stages = 12;
    p.flanger.feedback = 0.95;
    p.flanger.throughZero = true;
    p.echo.intensity = 1.0;
    p.echo.mode = 6;
    p.reverb.decayS = 30.0;
    p.reverb.type = GENERATE (ReverbType::plate, ReverbType::room, ReverbType::hall, ReverbType::shimmer);
    p.reverb.shimmer = 1.0;
    p.comp.ratioIndex = 6;
    p.comp.attackIndex = 0;
    const auto s = run<FxRack> (rack, [&] (FxRack& f, double& l, double& r) { f.process (l, r, p, 120.0); }, sine (220.0, 1.0), 6.0);
    CHECK (finiteAndBounded (s, 8.0f));
}

TEST_CASE ("Drive: dry path aligned with the oversampling (no comb at 50 % mix)", "[fx][drive]")
{
    const double hz = GENERATE (200.0, 500.0, 1500.0, 3000.0, 6000.0, 9000.0); // above: the TONE low-pass itself
    Drive d;
    d.prepare (sr);
    DriveParams p;
    p.model = DriveModel::tape;
    p.bias = 1.0;      // ideal bias: linear at low level
    p.driveDb = 0.0;
    p.tone = 1.0;
    p.mix = 0.5;
    const auto s = run<Drive> (d, [&] (Drive& f, double& l, double& r) { f.process (l, r, p); }, sine (hz, 0.01), 0.5);
    const double gain = rmsDb (s.l, 12000) - 20.0 * std::log10 (0.01 / std::sqrt (2.0));
    WARN (hz << " Hz: " << gain << " dB");
    CHECK (std::abs (gain) < 1.5);
}

TEST_CASE ("Drive: 4x oversampling keeps aliasing low", "[fx][drive]")
{
    const auto model = GENERATE (DriveModel::tube, DriveModel::diode, DriveModel::tape, DriveModel::fold);
    Drive d;
    d.prepare (sr);
    DriveParams p;
    p.model = model;
    p.driveDb = 24.0;
    p.tone = 1.0;
    const double hz = 46.875 * 64; // 3 kHz, on a bin
    const auto s = run<Drive> (d, [&] (Drive& f, double& l, double& r) { f.process (l, r, p); }, sine (hz, 0.5), 1.6);
    const double worst = m::worstAlias (s.l, sr, hz, 20000.0, 8192, 65536);
    WARN ("model " << static_cast<int> (model) << ": worst alias " << worst << " dB");
    CHECK (worst < -60.0);
}

TEST_CASE ("Drive: DRIVE changes colour more than loudness", "[fx][drive]")
{
    const auto model = GENERATE (DriveModel::tube, DriveModel::diode, DriveModel::tape);
    double lo = 1e9, hi = -1e9;
    for (double db : { 0.0, 12.0, 24.0, 36.0, 48.0 })
    {
        Drive d;
        d.prepare (sr);
        DriveParams p;
        p.model = model;
        p.driveDb = db;
        p.tone = 1.0;
        const auto s = run<Drive> (d, [&] (Drive& f, double& l, double& r) { f.process (l, r, p); }, sine (220.0, 0.3), 0.5);
        const double level = rmsDb (s.l, 6000);
        lo = std::min (lo, level);
        hi = std::max (hi, level);
    }
    WARN ("model " << static_cast<int> (model) << ": level spread " << hi - lo << " dB over 0..48 dB of drive");
    CHECK (hi - lo < 6.0);
}

TEST_CASE ("Phaser: N stages give N/2 notches", "[fx][phaser]")
{
    const int stages = GENERATE (4, 6, 8, 12);
    Phaser ph;
    ph.prepare (sr);
    PhaserParams p;
    p.stages = stages;
    p.depth = 0.0; // static
    p.centreHz = 600.0; // where the MANUAL smoother starts: a settled, static filter
    p.feedback = 0.0;
    p.mix = 0.5;
    constexpr size_t preroll = 24000; // let the control smoothers settle first
    const auto s = run<Phaser> (ph, [&] (Phaser& f, double& l, double& r) { f.process (l, r, p); },
                                [] (size_t n) { return n == preroll ? 1.0 : 0.0; }, (65536.0 + preroll) / sr + 0.01);
    // Magnitude response from the impulse response: count dips deeper than -20 dB in 20 Hz .. 20 kHz.
    std::vector<std::complex<double>> a (65536);
    for (size_t i = 0; i < a.size(); ++i)
        a[i] = s.l[i + preroll];
    m::fft (a);
    int notches = 0;
    bool inNotch = false;
    for (size_t b = 30; b < 27000; ++b)
    {
        const double db = 20.0 * std::log10 (std::abs (a[b]) + 1.0e-12);
        if (! inNotch && db < -20.0)
        {
            ++notches;
            inNotch = true;
        }
        else if (inNotch && db > -10.0)
            inNotch = false;
    }
    CHECK (notches == stages / 2);
}

TEST_CASE ("Flanger: through-zero reaches a full cancellation", "[fx][flanger]")
{
    Flanger f;
    f.prepare (sr);
    FlangerParams p;
    p.throughZero = true;
    p.depth = 1.0;
    p.manualMs = 3.0;
    p.feedback = 0.0;
    p.rateHz = 0.25;
    p.mix = 1.0;
    const auto s = run<Flanger> (f, [&] (Flanger& u, double& l, double& r) { u.process (l, r, p); }, sine (300.0, 0.5), 4.2);
    double minDb = 0.0;
    for (size_t start = 4800; start + 480 < s.l.size(); start += 240)
    {
        double e = 0.0;
        for (size_t i = start; i < start + 480; ++i)
            e += s.l[i] * s.l[i];
        minDb = std::min (minDb, 10.0 * std::log10 (e / 480.0 + 1.0e-20) - 20.0 * std::log10 (0.5 / std::sqrt (2.0)));
    }
    WARN ("deepest cancellation " << minDb << " dB");
    CHECK (minDb < -30.0);
}

TEST_CASE ("Tape echo: heads at 1 : 1.90 : 2.75 of the tape speed", "[fx][echo]")
{
    TapeEcho e;
    e.prepare (sr, 1);
    TapeEchoParams p;
    p.mode = 6; // all three heads
    p.headMs = 200.0;
    p.intensity = 0.0;
    p.wow = p.flutter = p.age = 0.0;
    p.mix = 1.0;
    p.width = 0.0;
    // Let the motor settle at the speed first, then fire the impulse.
    for (int i = 0; i < static_cast<int> (2.0 * sr); ++i)
    {
        double l = 0.0, r = 0.0;
        e.process (l, r, p);
    }
    std::vector<float> y (static_cast<size_t> (0.8 * sr));
    for (size_t i = 0; i < y.size(); ++i)
    {
        double l = i == 0 ? 1.0 : 0.0, r = l;
        e.process (l, r, p);
        y[i] = static_cast<float> (l);
    }
    for (double expectMs : { 200.0, 380.0, 550.0 })
    {
        const auto c = static_cast<size_t> (expectMs * 0.001 * sr);
        size_t best = c - 200;
        for (size_t i = c - 200; i < c + 200; ++i)
            if (std::abs (y[i]) > std::abs (y[best]))
                best = i;
        CHECK (std::abs (best / sr * 1000.0 - expectMs) < 1.5);
    }
}

TEST_CASE ("Reverb: measured RT60 follows DECAY", "[fx][reverb]")
{
    const auto type = GENERATE (ReverbType::plate, ReverbType::hall);
    Reverb rv;
    rv.prepare (sr, 1);
    ReverbParams p;
    p.type = type;
    p.decayS = 2.0;
    p.damping = 0.0;
    p.lowCutHz = 20.0;
    p.modulation = 0.0;
    p.predelayMs = 0.0;
    p.mix = 1.0;
    // White-noise burst, then energy decay (Schroeder integration).
    Rng noise (5);
    std::vector<double> y (static_cast<size_t> (5.0 * sr));
    for (size_t i = 0; i < y.size(); ++i)
    {
        double l = i < static_cast<size_t> (0.2 * sr) ? noise.gauss() * 0.1 : 0.0, r = l;
        rv.process (l, r, p);
        y[i] = l;
    }
    std::vector<double> edc (y.size());
    double acc = 0.0;
    for (size_t i = y.size(); i-- > 0;)
        edc[i] = acc += y[i] * y[i];
    const auto dbAt = [&] (double t) { return 10.0 * std::log10 (edc[static_cast<size_t> (t * sr)] / edc[static_cast<size_t> (0.25 * sr)]); };
    // Slope between -5 and -25 dB -> RT60.
    double t5 = -1.0, t25 = -1.0;
    for (double t = 0.25; t < 4.9; t += 0.001)
    {
        const double d = dbAt (t);
        if (t5 < 0.0 && d < -5.0)
            t5 = t;
        if (t25 < 0.0 && d < -25.0)
            t25 = t;
    }
    const double rt60 = 3.0 * (t25 - t5);
    WARN ("type " << static_cast<int> (type) << ": RT60 " << rt60 << " s (set 2.0)");
    CHECK (std::abs (rt60 - 2.0) < 0.5);
}

TEST_CASE ("Bus compressor holds the static curve (feedback detection)", "[fx][comp]")
{
    const int ratioIndex = GENERATE (1, 3, 5); // 2:1, 4:1, 10:1
    BusComp c;
    c.prepare (sr);
    BusCompParams p;
    p.thresholdDb = -20.0;
    p.ratioIndex = ratioIndex;
    p.attackIndex = 3;
    p.releaseIndex = 1;
    p.scHpfHz = 10.0;
    const auto s = run<BusComp> (c, [&] (BusComp& u, double& l, double& r) { u.process (l, r, p); }, sine (1000.0, 1.0), 2.0);
    // Peak-detected: input peak 0 dBFS -> expected peak -20 + 20 / ratio.
    double pk = 0.0;
    for (size_t i = s.l.size() - 4800; i < s.l.size(); ++i)
        pk = std::max (pk, static_cast<double> (std::abs (s.l[i])));
    const double ratio = BusComp::ratios[ratioIndex];
    const double expect = -20.0 + 20.0 / ratio;
    WARN ("ratio " << ratio << ": peak " << 20.0 * std::log10 (pk) << " dB (expected " << expect << ")");
    CHECK (std::abs (20.0 * std::log10 (pk) - expect) < 1.5);
}

TEST_CASE ("Rack: AUGUR's delay and spring units stay finite and bounded in any order", "[fx][rack]")
{
    FxRack rack;
    rack.prepare (sr, 3);
    FxParams p;
    p.on.fill (true);
    p.order = { 7, 6, 5, 4, 3, 2, 1, 0 }; // reversed chain
    p.delay.feedback = 1.0;
    p.delay.pingPong = true;
    p.echoSpring = 1.0;
    p.echo.intensity = 1.0;
    p.reverbSpring = GENERATE (false, true);
    p.reverb.decayS = 6.0;
    const auto s = run<FxRack> (rack, [&] (FxRack& f, double& l, double& r) { f.process (l, r, p, 124.0); }, sine (330.0, 0.8), 6.0);
    CHECK (finiteAndBounded (s, 8.0f));
}

TEST_CASE ("Rack: a unit switched off costs nothing and passes the signal untouched", "[fx][rack]")
{
    FxRack rack;
    rack.prepare (sr, 1);
    FxParams p; // everything off
    const auto in = sine (440.0, 0.5);
    const auto s = run<FxRack> (rack, [&] (FxRack& f, double& l, double& r) { f.process (l, r, p, 120.0); }, in, 0.2);
    for (size_t i = 0; i < s.l.size(); ++i)
        REQUIRE (s.l[i] == static_cast<float> (in (i)));
}
