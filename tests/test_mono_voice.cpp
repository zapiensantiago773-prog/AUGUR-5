#include "TestHelpers.h"
#include "Voice/MonoTestVoice.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <numbers>

using augur::MonoTestVoice;
using Catch::Matchers::WithinRel;

namespace
{
std::vector<float> render (MonoTestVoice& v, double seconds, double sr)
{
    std::vector<float> out (static_cast<std::size_t> (seconds * sr));
    v.render (out.data(), static_cast<int> (out.size()));
    return out;
}
} // namespace

TEST_CASE ("Pitch is exact at every sample rate across the keyboard", "[voice][samplerate]")
{
    const double sr = GENERATE (from_range (std::begin (augur::test::sampleRates), std::end (augur::test::sampleRates)));
    const int note = GENERATE (21, 60, 69, 108);

    MonoTestVoice v;
    v.prepare (sr);
    v.noteOn (note, 1.0f);
    const auto out = render (v, 1.0, sr);

    const double expected = MonoTestVoice::noteToHz (note);
    const auto skip = static_cast<std::size_t> (0.01 * sr); // past the gate ramp
    CHECK_THAT (augur::test::measureFrequency (out, sr, skip), WithinRel (expected, 1e-5));
}

TEST_CASE ("Note on and note off never click", "[voice][clicks]")
{
    const double sr = GENERATE (from_range (std::begin (augur::test::sampleRates), std::end (augur::test::sampleRates)));

    MonoTestVoice v;
    v.prepare (sr);
    v.noteOn (69, 1.0f);
    auto out = render (v, 0.1037, sr); // odd length so note-off lands mid-cycle
    v.noteOff (69);
    const auto tail = render (v, 0.05, sr);
    out.insert (out.end(), tail.begin(), tail.end());

    // A full-scale sine moves at most 2*pi*f/sr per sample; the 5 ms gate adds at most 1/rampSamples.
    const double rampSamples = MonoTestVoice::gateRampSeconds * sr;
    const auto bound = static_cast<float> (2.0 * std::numbers::pi * 440.0 / sr + 1.0 / rampSamples) * 1.01f;
    CHECK (augur::test::maxStep (out) <= bound);

    // Silent once the release ramp has finished.
    const auto rampEnd = static_cast<std::size_t> (rampSamples) + 2;
    for (std::size_t n = rampEnd; n < tail.size(); ++n)
        REQUIRE (tail[n] == 0.0f);
    CHECK_FALSE (v.isActive());
}

TEST_CASE ("Last-note priority returns to the previously held note", "[voice][midi]")
{
    constexpr double sr = 48000.0;
    MonoTestVoice v;
    v.prepare (sr);

    v.noteOn (60, 1.0f);
    v.noteOn (64, 1.0f);
    CHECK (v.getCurrentNote() == 64);

    v.noteOff (64);
    CHECK (v.getCurrentNote() == 60);
    const auto out = render (v, 0.5, sr);
    CHECK_THAT (augur::test::measureFrequency (out, sr), WithinRel (MonoTestVoice::noteToHz (60), 1e-4));

    v.noteOff (99); // releasing an unheld note is harmless
    CHECK (v.isActive());

    v.allNotesOff();
    render (v, 0.01, sr);
    CHECK_FALSE (v.isActive());
}
