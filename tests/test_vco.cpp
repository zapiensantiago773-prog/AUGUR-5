#include "TestHelpers.h"
#include "Analog/Autotune.h"
#include "Mixer/OtaMixer.h"
#include "Oscillator/Cem3340.h"
#include "Oscillator/Ssm2030.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <functional>

using namespace augur;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

namespace
{
double hzOf (double pitch) { return 440.0 * std::exp2 ((pitch - 69.0) / 12.0); }
double pitchOf (double hz) { return 69.0 + 12.0 * std::log2 (hz / 440.0); }

Cem3340Unit idealCem()
{
    Cem3340Unit u;
    u.comparatorDelay = 0.0f;
    u.syncHold = 0.0f;
    u.pulseFallDelay = 0.0f;
    return u;
}

Cem3340Unit typicalCem()
{
    Cem3340Unit u; // defaults: 100 ns comparator, 4.4 us sync hold, 0.5 us slow fall
    u.asymmetry = 0.02f;
    u.converterGain = 1.002f;
    u.converterOffset = 0.001f;
    u.expo.bulkCentsAt10k = 3.0f;
    return u;
}

enum class Wave { Saw, Tri, Pulse };

float pick (const VcoOutputs& o, Wave w)
{
    return w == Wave::Saw ? o.saw : (w == Wave::Tri ? o.tri : o.pulse);
}

// Renders one waveform, removing its mean so zero-crossing and spectral tools apply.
template <typename Osc>
std::vector<float> render (Osc& vco, double sr, double seconds, const std::function<float (size_t)>& pitchAt,
                           const std::function<float (size_t)>& pwAt, Wave w)
{
    std::vector<float> out (static_cast<size_t> (seconds * sr));
    for (size_t n = 0; n < out.size(); ++n)
        out[n] = pick (vco.process (pitchAt (n), pwAt (n), -1.0f), w);
    double mean = 0.0;
    for (auto s : out)
        mean += s;
    mean /= static_cast<double> (out.size());
    for (auto& s : out)
        s -= static_cast<float> (mean);
    return out;
}

} // namespace

//==============================================================================
// CEM3340: triangle core

TEST_CASE ("CEM3340 ideal unit plays the exact commanded pitch at every sample rate", "[cem3340][samplerate]")
{
    const double sr = GENERATE (from_range (std::begin (test::sampleRates), std::end (test::sampleRates)));
    const float pitch = GENERATE (21.0f, 69.0f, 108.0f);
    Cem3340Vco vco;
    vco.prepare (sr, 1);
    vco.setUnit (idealCem());
    const auto saw = render (vco, sr, 1.0, [&] (size_t) { return pitch; }, [] (size_t) { return 0.5f; }, Wave::Saw);
    CHECK_THAT (test::measureFrequency (saw, sr, 64), WithinRel (hzOf (pitch), 1e-5));
}

TEST_CASE ("CEM3340 steady-state frequency model matches the simulated core", "[cem3340][autotune]")
{
    constexpr double sr = 96000.0;
    const float pitch = GENERATE (36.0f, 60.0f, 96.0f, 115.0f);
    Cem3340Vco vco;
    vco.prepare (sr, 2);
    vco.setUnit (typicalCem());
    const auto tri = render (vco, sr, 1.0, [&] (size_t) { return pitch; }, [] (size_t) { return 0.5f; }, Wave::Tri);
    CHECK_THAT (test::measureFrequency (tri, sr, 64), WithinRel (vco.staticFrequency (pitch), 2e-5));
}

TEST_CASE ("CEM3340 goes flat only in the top octaves (comparator delay + bulk resistance)", "[cem3340][analog]")
{
    Cem3340Vco vco;
    vco.prepare (96000.0, 3);
    vco.setUnit (typicalCem());
    const auto errorCents = [&] (double pitch) { return 1200.0 * std::log2 (vco.staticFrequency (pitch) / hzOf (pitch)); };
    INFO ("C4 " << errorCents (60) << "  C7 " << errorCents (96) << "  C9 " << errorCents (120) << " cents");
    CHECK (std::abs (errorCents (48)) < 0.5); // untuned offset from asymmetry + delay; autotune removes it
    CHECK (errorCents (96) < -1.5);         // 2 kHz: already measurable
    CHECK (errorCents (120) < -8.0);        // 8.4 kHz: clearly flat
    CHECK (errorCents (120) < errorCents (108));
}

TEST_CASE ("CEM3340 asymmetry bends the sawtooth at mid-ramp by the expected slope ratio", "[cem3340][analog]")
{
    constexpr double sr = 96000.0;
    auto u = idealCem();
    u.asymmetry = 0.1f; // datasheet limit (45/55 %)
    Cem3340Vco vco;
    vco.prepare (sr, 4);
    vco.setUnit (u);

    // 100 Hz: 960 samples per cycle; measure the saw slope in each half, away from the corners.
    std::vector<float> saw (static_cast<size_t> (sr * 0.2));
    for (auto& s : saw)
        s = vco.process (static_cast<float> (pitchOf (100.0)), 0.5f, -1.0f).saw;

    double riseLow = 0.0, riseHigh = 0.0;
    int nLow = 0, nHigh = 0;
    for (size_t n = 2000; n + 1 < saw.size(); ++n)
    {
        const double ds = saw[n + 1] - saw[n];
        if (ds <= 0.0)
            continue; // skip the reset
        if (saw[n] > 0.1f && saw[n] < 0.4f) { riseLow += ds; ++nLow; }
        if (saw[n] > 0.6f && saw[n] < 0.9f) { riseHigh += ds; ++nHigh; }
    }
    const double ratio = (riseLow / nLow) / (riseHigh / nHigh);
    const double expected = (1.0 - 0.05) / (1.0 + 0.05);
    CHECK_THAT (ratio, WithinRel (expected, 0.01));
}

TEST_CASE ("CEM3340 aliasing (measured) at the internal rate", "[cem3340][aliasing]")
{
    const double sr = GENERATE (88200.0, 96000.0);
    const double f0 = GENERATE (1001.7, 3001.7, 7012.3);
    const auto wave = GENERATE (Wave::Saw, Wave::Tri, Wave::Pulse);

    Cem3340Vco vco;
    vco.prepare (sr, 5);
    vco.setUnit (typicalCem());
    const auto p = static_cast<float> (pitchOf (f0));
    const auto x = render (vco, sr, 1.0, [&] (size_t) { return p; }, [] (size_t) { return 0.37f; }, wave);

    // The unit's steady-state frequency (flattened by the comparator) defines the harmonic series.
    const double worst = test::worstAliasDb (x, sr, vco.staticFrequency (p), 20000.0, 4096, 65536);
    WARN ("CEM3340 alias sr=" << sr << " f=" << f0 << " wave=" << static_cast<int> (wave) << ": " << worst << " dB");
    CHECK (worst < -100.0);
}

TEST_CASE ("CEM3340 audio-rate PWM and FM stay band-limited", "[cem3340][aliasing]")
{
    // Modulator locked to a quarter of the carrier's (average) frequency: the whole signal is periodic at
    // fm, so every legitimate component is a harmonic of fm and anything else is aliasing.
    constexpr double sr = 96000.0;
    constexpr double fm = 377.0;
    constexpr double depthOct = 0.25; // FM: +-3 semitones
    const bool fmMode = GENERATE (false, true);
    const double meanFactor = fmMode ? std::cyl_bessel_i (0.0, depthOct * 0.69314718055994531) : 1.0; // mean of 2^(d sin)
    const double f0 = 4.0 * fm / meanFactor;

    Cem3340Vco vco;
    vco.prepare (sr, 6);
    vco.setUnit (idealCem()); // exact frequency, so the periodicity above is exact
    const double w = 2.0 * 3.14159265358979323846 * fm / sr;
    const auto x = render (vco, sr, 1.0,
        [&] (size_t n) { return static_cast<float> (pitchOf (f0) + (fmMode ? 12.0 * depthOct * std::sin (w * static_cast<double> (n)) : 0.0)); },
        [&] (size_t n) { return static_cast<float> (fmMode ? 0.5 : 0.5 + 0.35 * std::sin (w * static_cast<double> (n))); },
        fmMode ? Wave::Saw : Wave::Pulse);

    const double worst = test::worstAliasDb (x, sr, fm, 20000.0, 4096, 65536);
    WARN ((fmMode ? "FM" : "PWM") << " alias: " << worst << " dB");
    CHECK (worst < -100.0);
}

TEST_CASE ("Prophet sync resets the slave on the master's falling saw edge", "[cem3340][sync]")
{
    constexpr double sr = 96000.0;
    Cem3340Vco master, slave;
    master.prepare (sr, 7);
    slave.prepare (sr, 8);
    master.setUnit (typicalCem());
    slave.setUnit (typicalCem());

    const auto mp = static_cast<float> (pitchOf (1001.7));
    const auto sp = static_cast<float> (pitchOf (2711.0));
    std::vector<float> out (static_cast<size_t> (sr));
    for (auto& s : out)
    {
        master.process (mp, 0.5f, -1.0f);
        s = slave.process (sp, 0.5f, master.lastResetD()).saw;
    }
    double mean = 0.0;
    for (auto s : out) mean += s;
    mean /= static_cast<double> (out.size());
    for (auto& s : out) s -= static_cast<float> (mean);

    // Locked: every component is a harmonic of the master, and nothing aliases.
    const double worst = test::worstAliasDb (out, sr, master.staticFrequency (mp), 20000.0, 4096, 65536);
    WARN ("sync alias: " << worst << " dB");
    CHECK (worst < -100.0);
}

//==============================================================================
// SSM2030: ramp core

TEST_CASE ("SSM2030 pitch and steady-state model", "[ssm2030]")
{
    const double sr = GENERATE (48000.0, 96000.0);
    const float pitch = GENERATE (40.0f, 81.0f, 110.0f);
    Ssm2030Vco vco;
    vco.prepare (sr, 9);
    Ssm2030Unit u;
    u.curvature = 0.08f;
    u.deadTimeSeconds = 3.0e-6f;
    vco.setUnit (u);
    const auto saw = render (vco, sr, 1.0, [&] (size_t) { return pitch; }, [] (size_t) { return 0.5f; }, Wave::Saw);
    CHECK_THAT (test::measureFrequency (saw, sr, 64), WithinRel (vco.staticFrequency (pitch), 2e-5));
}

TEST_CASE ("SSM2030 aliasing (measured)", "[ssm2030][aliasing]")
{
    constexpr double sr = 96000.0;
    const double f0 = GENERATE (1001.7, 7012.3);
    const auto wave = GENERATE (Wave::Saw, Wave::Tri, Wave::Pulse);
    Ssm2030Vco vco;
    vco.prepare (sr, 10);
    Ssm2030Unit u;
    u.curvature = 0.08f;
    u.deadTimeSeconds = 3.0e-6f;
    vco.setUnit (u);
    const auto p = static_cast<float> (pitchOf (f0));
    const auto x = render (vco, sr, 1.0, [&] (size_t) { return p; }, [] (size_t) { return 0.37f; }, wave);
    const double worst = test::worstAliasDb (x, sr, vco.staticFrequency (p), 20000.0, 4096, 65536);
    WARN ("SSM2030 alias f=" << f0 << " wave=" << static_cast<int> (wave) << ": " << worst << " dB");
    CHECK (worst < -100.0);
}

//==============================================================================
// Autotune and the 14-bit DAC

TEST_CASE ("Autotune corrects a badly trimmed unit to the Prophet's resolution", "[autotune]")
{
    Cem3340Vco vco;
    vco.prepare (96000.0, 11);
    auto u = typicalCem();
    u.expo.tuneCents = 37.0f;     // way off before TUNE
    u.expo.scaleError = 0.003f;   // badly trimmed scale
    vco.setUnit (u);

    AutotuneTable table;
    table.tune (vco);

    const auto error = [&] (double note) {
        const double played = vco.staticFrequency (quantiseOscCv (static_cast<float> (note) + table.biasFor (static_cast<float> (note))));
        return 1200.0 * std::log2 (played / hzOf (note));
    };

    for (int oct = 3; oct <= 9; ++oct)
    {
        // Measured points: half a DAC step (0.39 ct) + one count of the 2.5 MHz period counter.
        const double f = hzOf (12.0 * (oct + 1));
        const double bound = 0.4 + 1200.0 * std::log2 (1.0 + f / AutotuneTable::clockHz);
        INFO ("C" << oct << ": " << error (12.0 * (oct + 1)) << " cents (bound " << bound << ")");
        CHECK (std::abs (error (12.0 * (oct + 1))) < bound);
    }
    INFO ("F#5 " << error (78) << "  C1 " << error (24) << "  C9 " << error (120));
    CHECK (std::abs (error (78)) < 2.0);  // between octave points
    CHECK (std::abs (error (24)) < 3.0);  // extrapolated octave
    CHECK (std::abs (error (120)) < 4.0); // top octave: coarse period count at 2.5 MHz
}

TEST_CASE ("Oscillator DAC lands on 1/128-semitone steps", "[autotune]")
{
    CHECK (quantiseOscCv (60.003f) == 60.0f);
    CHECK_THAT (quantiseOscCv (60.006f), WithinAbs (60.0078125f, 1e-6f));
}

//==============================================================================
// OTA mixer

TEST_CASE ("OTA mixer: saw + pulse is the difference, softly compressed", "[mixer]")
{
    // Lone saw at full scale peaks at 1 (normalisation), half scale is compressed less than linearly.
    CHECK_THAT (ota::mix (1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f), WithinAbs (1.0f, 1e-4f));
    const float half = ota::mix (0.5f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f);
    CHECK (half > 0.5f);  // tanh curvature: small signals have relatively more gain
    CHECK (half < 0.53f); // ...but only mildly (~5 % at the peak)

    // Saw high + pulse high cancel, pulse alone is negative.
    CHECK (std::abs (ota::mix (1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f)) < 0.05f);
    CHECK (ota::mix (0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f) < -0.9f);
}

TEST_CASE ("OTA mixer DC matches the numerical cycle average", "[mixer]")
{
    const float gS = GENERATE (0.0f, 1.0f);
    const float gT = GENERATE (0.0f, 1.0f);
    const float gP = GENERATE (0.0f, 1.0f);
    const float w = GENERATE (0.1f, 0.5f, 0.83f);

    double sum = 0.0;
    constexpr int n = 200000;
    for (int i = 0; i < n; ++i)
    {
        const double ph = (i + 0.5) / n;
        const double tri = ph < 0.5 ? 2.0 * ph : 2.0 - 2.0 * ph;
        sum += ota::mix (static_cast<float> (ph), static_cast<float> (tri), ph < w ? 1.0f : 0.0f, gS, gT, gP);
    }
    CHECK_THAT (ota::dc (gS, gT, gP, w), WithinAbs (static_cast<float> (sum / n), 1e-4f));
}
