#include "TestHelpers.h"
#include "Util/Smoother.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>

using augur::LinearSmoother;

TEST_CASE ("LinearSmoother reaches the target in exactly the ramp duration at every sample rate", "[smoother]")
{
    const double sr = GENERATE (from_range (std::begin (augur::test::sampleRates), std::end (augur::test::sampleRates)));

    LinearSmoother s;
    s.reset (sr, 0.01, 0.0f);
    const int expected = static_cast<int> (std::lround (sr * 0.01));
    REQUIRE (s.getRampSamples() == expected);

    s.setTarget (1.0f);
    float previous = 0.0f;
    for (int i = 0; i < expected - 1; ++i)
    {
        const float v = s.next();
        REQUIRE (v > previous); // strictly monotonic
        REQUIRE (v < 1.0f);
        previous = v;
    }
    REQUIRE (s.next() == 1.0f);
    REQUIRE_FALSE (s.isSmoothing());
    REQUIRE (s.next() == 1.0f);
}

TEST_CASE ("LinearSmoother retargets mid-ramp without jumping", "[smoother]")
{
    LinearSmoother s;
    s.reset (48000.0, 0.01, 0.0f);
    s.setTarget (1.0f);
    for (int i = 0; i < 100; ++i)
        s.next();

    const float before = s.getCurrent();
    s.setTarget (0.0f);
    const float after = s.next();
    REQUIRE (after < before);
    REQUIRE (before - after < 2.0f / static_cast<float> (s.getRampSamples()));
}
