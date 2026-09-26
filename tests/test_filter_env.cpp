#include "TestHelpers.h"
#include "Envelope/RcAdsr.h"
#include "Filter/LadderFilter.h"
#include "Util/Random.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

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
