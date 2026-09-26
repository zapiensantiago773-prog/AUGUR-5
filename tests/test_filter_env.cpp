#include "TestHelpers.h"
#include "Envelope/RcAdsr.h"
#include "Filter/LadderFilter.h"
#include "Util/Random.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <array>
#include <cmath>

using augur::LadderFilter;
using augur::RcAdsr;

TEST_CASE ("Filter stays stable under random audio-rate cutoff modulation at full resonance", "[filter]")
{
    const auto model = GENERATE (LadderFilter::Model::Cem3320, LadderFilter::Model::Ssm2040);
    LadderFilter f;
    f.prepare (96000.0);
    f.setModel (model);
    augur::Random r (42);

    float peak = 0.0f;
    for (int i = 0; i < 96000; ++i)
    {
        const float cutoff = 20.0f * std::pow (2000.0f, r.nextFloat());
        const float y = f.process (r.nextBipolar() * 2.0f, cutoff, 1.1f);
        REQUIRE (std::isfinite (y));
        peak = std::max (peak, std::abs (y));
    }
    CHECK (peak < 4.0f);
}

TEST_CASE ("Filter self-oscillates near the cutoff at high resonance", "[filter]")
{
    constexpr double sr = 96000.0;
    const auto model = GENERATE (LadderFilter::Model::Cem3320, LadderFilter::Model::Ssm2040);
    LadderFilter f;
    f.prepare (sr);
    f.setModel (model);

    std::vector<float> out (static_cast<size_t> (sr));
    for (size_t i = 0; i < out.size(); ++i)
        out[i] = f.process (i == 0 ? 0.1f : 0.0f, 1000.0f, 1.0f);

    INFO ("self-oscillation rms " << augur::test::rms (out, out.size() / 2));
    CHECK (augur::test::rms (out, out.size() / 2) > 0.1);
    const double freq = augur::test::measureFrequency (out, sr, out.size() / 2);
    CHECK (freq > 800.0);
    CHECK (freq < 1150.0);
}

TEST_CASE ("CEM3320 loses bass as resonance rises, SSM2040 keeps more of it", "[filter]")
{
    const auto dcGain = [] (LadderFilter::Model m, float reso) {
        LadderFilter f;
        f.prepare (96000.0);
        f.setModel (m);
        float y = 0.0f;
        for (int i = 0; i < 20000; ++i)
            y = f.process (0.05f, 2000.0f, reso);
        return y / 0.05f;
    };

    CHECK_THAT (dcGain (LadderFilter::Model::Cem3320, 0.0f), Catch::Matchers::WithinRel (1.0f, 0.02f));
    CHECK (dcGain (LadderFilter::Model::Cem3320, 0.6f) < 0.3f);
    CHECK (dcGain (LadderFilter::Model::Ssm2040, 0.6f) > dcGain (LadderFilter::Model::Cem3320, 0.6f));
}

TEST_CASE ("RC envelope: attack reaches full level at the set time, release falls 60 dB at the set time", "[envelope]")
{
    constexpr double sr = 48000.0;
    RcAdsr env;
    env.prepare (sr);
    env.setParameters (0.1f, 0.5f, 0.5f, 0.3f);
    env.noteOn();

    int n = 0;
    while (env.getStage() == RcAdsr::Stage::Attack && n < 100000)
    {
        env.next();
        ++n;
    }
    CHECK_THAT (n / sr, Catch::Matchers::WithinRel (0.1, 0.01));

    // Attack curve is convex (RC aimed above the top): halfway in time is well above half level.
    RcAdsr env2;
    env2.prepare (sr);
    env2.setParameters (0.1f, 0.5f, 0.5f, 0.3f);
    env2.noteOn();
    float mid = 0.0f;
    for (int i = 0; i < static_cast<int> (0.05 * sr); ++i)
        mid = env2.next();
    CHECK (mid > 0.6f);

    for (int i = 0; i < static_cast<int> (sr * 5); ++i)
        env.next();
    CHECK_THAT (env.getLevel(), Catch::Matchers::WithinAbs (0.5f, 1e-3f));

    env.noteOff();
    const float start = env.getLevel();
    n = 0;
    while (env.getLevel() > start * 0.001f && n < 1000000)
    {
        env.next();
        ++n;
    }
    CHECK_THAT (n / sr, Catch::Matchers::WithinRel (0.3, 0.01));
}

TEST_CASE ("SIMD 4-lane filter matches the scalar filter", "[filter][simd]")
{
    const auto model = GENERATE (LadderFilter::Model::Cem3320, LadderFilter::Model::Ssm2040);
    std::array<LadderFilter, 4> scalar, lanes;
    for (size_t k = 0; k < 4; ++k)
    {
        scalar[k].prepare (96000.0);
        lanes[k].prepare (96000.0);
        scalar[k].setModel (model);
        lanes[k].setModel (model);
    }
    std::array<LadderFilter*, 4> ptrs { &lanes[0], &lanes[1], &lanes[2], &lanes[3] };
    augur::Random r (7);
    float worst = 0.0f;
    for (int block = 0; block < 300; ++block)
    {
        LadderFilter::Lanes st;
        LadderFilter::gather (st, ptrs.data());
        for (int n = 0; n < 64; ++n)
        {
            float x[4], fc[4], res[4], ref[4];
            for (size_t k = 0; k < 4; ++k)
            {
                x[k] = r.nextBipolar() * 1.5f;
                fc[k] = 30.0f * std::pow (600.0f, r.nextFloat());
                res[k] = r.nextFloat() * 1.05f;
                ref[k] = scalar[k].process (x[k], fc[k], res[k]);
            }
            float got[4];
            augur::simd::store (got, lanes[0].process4 (st, augur::simd::load (x), augur::simd::load (fc), augur::simd::load (res)));
            for (size_t k = 0; k < 4; ++k)
                worst = std::max (worst, std::abs (got[k] - ref[k]));
        }
        LadderFilter::scatter (st, ptrs.data(), 4);
    }
    INFO ("max difference " << worst);
    CHECK (worst < 1.0e-4f);
}

namespace
{
// Steady-state gain of a small sine through the filter (linear region).
double sineGain (LadderFilter& f, double sr, double hz, float cutoff, float reso)
{
    f.reset();
    const int n = static_cast<int> (sr * 0.25);
    double in2 = 0.0, out2 = 0.0;
    for (int i = 0; i < n; ++i)
    {
        const float x = 0.05f * static_cast<float> (std::sin (2.0 * 3.14159265358979 * hz * i / sr));
        const float y = f.process (x, cutoff, reso);
        if (i > n / 2)
        {
            in2 += static_cast<double> (x) * x;
            out2 += static_cast<double> (y) * y;
        }
    }
    return std::sqrt (out2 / in2);
}
} // namespace

TEST_CASE ("Every filter model and shape stays stable at extreme settings", "[filter]")
{
    const auto model = GENERATE (LadderFilter::Model::Cem3320, LadderFilter::Model::Ssm2040, LadderFilter::Model::Cascade,
                                 LadderFilter::Model::Multimode, LadderFilter::Model::Bite);
    const auto mode = GENERATE (LadderFilter::Mode::LowPass, LadderFilter::Mode::BandPass, LadderFilter::Mode::HighPass);
    const bool twelve = GENERATE (false, true);
    LadderFilter f;
    f.prepare (96000.0);
    f.setModel (model);
    f.setShape (twelve, mode);
    f.setHighpass (200.0f);
    augur::Random r (7);

    float peak = 0.0f;
    for (int i = 0; i < 48000; ++i)
    {
        const float cutoff = 10.0f * std::pow (4000.0f, r.nextFloat());
        const float y = f.process (r.nextBipolar() * 3.0f, cutoff, 1.1f);
        REQUIRE (std::isfinite (y));
        peak = std::max (peak, std::abs (y));
    }
    INFO ("model " << static_cast<int> (model) << " mode " << static_cast<int> (mode) << " 12dB " << twelve);
    CHECK (peak < 30.0f);
}

TEST_CASE ("Filter modes have the right frequency response", "[filter]")
{
    constexpr double sr = 96000.0;
    constexpr float fc = 1000.0f;
    const auto model = GENERATE (LadderFilter::Model::Cem3320, LadderFilter::Model::Ssm2040, LadderFilter::Model::Cascade,
                                 LadderFilter::Model::Multimode, LadderFilter::Model::Bite);
    const bool twelve = GENERATE (false, true);
    LadderFilter f;
    f.prepare (sr);
    f.setModel (model);
    INFO ("model " << static_cast<int> (model) << " 12dB " << twelve);

    f.setShape (twelve, LadderFilter::Mode::LowPass);
    const double lpLow = sineGain (f, sr, fc / 8.0, fc, 0.0f), lpHigh = sineGain (f, sr, fc * 8.0, fc, 0.0f);
    CHECK (lpLow > 0.7);
    CHECK (lpHigh < 0.05);

    f.setShape (twelve, LadderFilter::Mode::HighPass);
    const double hpLow = sineGain (f, sr, fc / 8.0, fc, 0.0f), hpHigh = sineGain (f, sr, fc * 8.0, fc, 0.0f);
    CHECK (hpHigh > 0.7);
    CHECK (hpLow < 0.05);

    if (model != LadderFilter::Model::Bite) // Bite has no band-pass (it uses LP)
    {
        f.setShape (twelve, LadderFilter::Mode::BandPass);
        const double bpMid = sineGain (f, sr, fc, fc, 0.0f);
        CHECK (bpMid > 0.3);
        CHECK (bpMid > 4.0 * sineGain (f, sr, fc / 16.0, fc, 0.0f));
        CHECK (bpMid > 4.0 * sineGain (f, sr, fc * 16.0, fc, 0.0f));
    }

    // 24 dB rolls off faster than 12 dB (two octaves above the corner).
    if (model != LadderFilter::Model::Bite)
    {
        f.setShape (twelve, LadderFilter::Mode::LowPass);
        const double g4 = sineGain (f, sr, fc * 4.0, fc, 0.0f), g8 = sineGain (f, sr, fc * 8.0, fc, 0.0f);
        const double slopeDb = 20.0 * std::log10 (g4 / g8);
        CHECK (slopeDb > (twelve ? 9.0 : 19.0));
        CHECK (slopeDb < (twelve ? 14.0 : 27.0));
    }
}

TEST_CASE ("Bite self-oscillates and screams at high resonance", "[filter]")
{
    constexpr double sr = 96000.0;
    LadderFilter f;
    f.prepare (sr);
    f.setModel (LadderFilter::Model::Bite);
    std::vector<float> out (static_cast<size_t> (sr));
    for (size_t i = 0; i < out.size(); ++i)
        out[i] = f.process (i == 0 ? 0.1f : 0.0f, 1000.0f, 1.05f);
    INFO ("rms " << augur::test::rms (out, out.size() / 2));
    CHECK (augur::test::rms (out, out.size() / 2) > 0.1);
    const double freq = augur::test::measureFrequency (out, sr, out.size() / 2);
    CHECK (freq > 700.0);
    CHECK (freq < 1300.0);
}

TEST_CASE ("Post-filter high-pass removes the lows and leaves the rest", "[filter]")
{
    constexpr double sr = 96000.0;
    LadderFilter f;
    f.prepare (sr);
    // Relative to the main filter alone (wide open, it still rolls off a little at 4 kHz).
    const double ref50 = sineGain (f, sr, 50.0, 20000.0f, 0.0f), ref400 = sineGain (f, sr, 400.0, 20000.0f, 0.0f),
                 ref4k = sineGain (f, sr, 4000.0, 20000.0f, 0.0f);
    f.setHighpass (400.0f);
    CHECK (sineGain (f, sr, 50.0, 20000.0f, 0.0f) / ref50 < 0.03); // 3 octaves below: -36 dB (12 dB/oct)
    CHECK_THAT (sineGain (f, sr, 400.0, 20000.0f, 0.0f) / ref400, Catch::Matchers::WithinRel (0.707, 0.08));
    CHECK_THAT (sineGain (f, sr, 4000.0, 20000.0f, 0.0f) / ref4k, Catch::Matchers::WithinRel (1.0, 0.05));
    f.setHighpass (10.0f); // off
    CHECK_THAT (sineGain (f, sr, 50.0, 20000.0f, 0.0f) / ref50, Catch::Matchers::WithinRel (1.0, 0.01));
}
