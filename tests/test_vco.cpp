#include "TestHelpers.h"
#include "Oscillator/Vco.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using augur::Vco;
using augur::VcoCharacter;
using Catch::Matchers::WithinRel;

namespace
{
VcoCharacter cem3340Nominal()
{
    VcoCharacter c;
    c.curvature = 0.035f;
    c.deadTimeSeconds = 1.6e-6f;
    c.hfTrim = 1.0f;
    return c;
}

std::vector<float> renderVco (Vco& vco, double sr, double seconds, float freq, float saw, float tri, float pulse, float pw = 0.5f)
{
    std::vector<float> out (static_cast<size_t> (seconds * sr));
    for (auto& s : out)
        s = vco.process (freq, pw, saw, tri, pulse, -1.0f);
    return out;
}
} // namespace

TEST_CASE ("VCO pitch is exact at every sample rate when the HF trim is calibrated", "[vco][samplerate]")
{
    const double sr = GENERATE (from_range (std::begin (augur::test::sampleRates), std::end (augur::test::sampleRates)));
    const float freq = GENERATE (27.5f, 440.0f, 4186.0f);

    Vco vco;
    vco.prepare (sr, 1);
    vco.setCharacter (cem3340Nominal());
    const auto out = renderVco (vco, sr, 1.0, freq, 1.0f, 0.0f, 0.0f);
    CHECK_THAT (augur::test::measureFrequency (out, sr, 64), WithinRel (static_cast<double> (freq), 1e-5));
}

TEST_CASE ("Untrimmed dead time flattens high notes like the real exponential converter", "[vco][analog]")
{
    constexpr double sr = 96000.0;
    auto c = cem3340Nominal();
    c.hfTrim = 0.0f;
    Vco vco;
    vco.prepare (sr, 1);
    vco.setCharacter (c);

    const float f = 4186.0f;
    const auto out = renderVco (vco, sr, 1.0, f, 1.0f, 0.0f, 0.0f);
    const double expected = 1.0 / (1.0 / f + static_cast<double> (c.deadTimeSeconds));
    CHECK_THAT (augur::test::measureFrequency (out, sr, 64), WithinRel (expected, 1e-4));
    CHECK (expected < f);
}

TEST_CASE ("VCO output is bounded and roughly DC-free for every waveform", "[vco]")
{
    constexpr double sr = 96000.0;
    Vco vco;
    vco.prepare (sr, 7);
    vco.setCharacter (cem3340Nominal());

    for (int w = 0; w < 3; ++w)
    {
        const auto out = renderVco (vco, sr, 0.5, 1234.5f, w == 0 ? 1.0f : 0.0f, w == 1 ? 1.0f : 0.0f, w == 2 ? 1.0f : 0.0f);
        double mean = 0.0;
        float peak = 0.0f;
        for (size_t i = 1000; i < out.size(); ++i)
        {
            mean += out[i];
            peak = std::max (peak, std::abs (out[i]));
        }
        mean /= static_cast<double> (out.size() - 1000);
        INFO ("waveform " << w);
        CHECK (peak < 1.25f);
        CHECK (std::abs (mean) < 0.06);
    }
}

TEST_CASE ("VCO aliasing (measured) at the internal rate", "[vco][aliasing]")
{
    // Internal voice rate is 2x the host rate at 44.1/48 kHz. Only components that land in 20 Hz..20 kHz count;
    // everything above is removed by the half-band decimator.
    const double sr = GENERATE (88200.0, 96000.0);
    const float freq = GENERATE (1001.7f, 3001.7f, 7012.3f);
    const int wave = GENERATE (0, 2); // saw, pulse

    Vco vco;
    vco.prepare (sr, 3);
    vco.setCharacter (cem3340Nominal());
    const auto out = renderVco (vco, sr, 1.0, freq, wave == 0 ? 1.0f : 0.0f, 0.0f, wave == 2 ? 1.0f : 0.0f, 0.37f);

    const double worst = augur::test::worstAliasDb (out, sr, freq, 20000.0, 4096, 65536);
    INFO ("sr " << sr << "  f " << freq << "  wave " << (wave == 0 ? "saw" : "pulse") << "  worst alias " << worst << " dB");
    WARN ("alias sr=" << sr << " f=" << freq << " " << (wave == 0 ? "saw" : "pulse") << ": " << worst << " dB");
    CHECK (worst < -100.0);
}

TEST_CASE ("Hard sync locks the slave to the master's period", "[vco][sync]")
{
    constexpr double sr = 96000.0;
    Vco master, slave;
    master.prepare (sr, 1);
    slave.prepare (sr, 2);
    master.setCharacter (cem3340Nominal());
    slave.setCharacter (cem3340Nominal());

    std::vector<float> out (static_cast<size_t> (sr));
    for (auto& s : out)
    {
        master.process (220.0f, 0.5f, 1.0f, 0.0f, 0.0f, -1.0f);
        s = slave.process (557.0f, 0.5f, 1.0f, 0.0f, 0.0f, master.lastResetD());
    }

    // The synced waveform repeats every master period: compare consecutive periods.
    const auto period = static_cast<size_t> (std::lround (sr / 220.0 * 11.0)); // 11 periods = integer-ish samples
    double diff = 0.0, energy = 0.0;
    for (size_t i = 10000; i < 10000 + period; ++i)
    {
        diff += std::abs (out[i] - out[i + period]);
        energy += std::abs (out[i]);
    }
    CHECK (diff / energy < 0.02);
}
