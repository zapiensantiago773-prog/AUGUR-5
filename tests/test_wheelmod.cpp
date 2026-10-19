#include "TestHelpers.h"
#include "Engine/SynthEngine.h"
#include "Modulation/Lfo.h"
#include "Modulation/WheelModNoise.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

using augur::Lfo;
using augur::SynthEngine;
using augur::SynthParams;

namespace
{
SynthParams oneSaw()
{
    SynthParams p;
    p.mixOsc1 = 0.8f;
    p.mixOsc2 = 0.0f;
    p.cutoffHz = 20000.0f;
    p.resonance = 0.0f;
    p.envAmount = 0.0f;
    p.aenvA = 0.001f;
    p.aenvS = 1.0f;
    p.analogAge = 0.0f;
    p.voiceDetune = 0.0f;
    p.mixDrive = 0.0f;
    return p;
}

// Frequency of OSC A in consecutive windows.
std::vector<double> pitchTrack (const std::vector<float>& x, std::size_t from, std::size_t window)
{
    std::vector<double> f;
    for (std::size_t w = from; w + window <= x.size(); w += window)
        f.push_back (augur::test::measureFrequency (std::vector<float> (x.begin() + static_cast<long> (w), x.begin() + static_cast<long> (w + window)),
                                                    48000.0));
    return f;
}
} // namespace

TEST_CASE ("LFO: single waves are unchanged, SAW / TRI / SQUARE combine with the Prophet's polarities", "[lfo][wheelmod]")
{
    for (int setting = 0; setting < 4; ++setting)
    {
        Lfo a, b;
        a.prepare (96000.0, 11);
        b.prepare (96000.0, 11);
        for (int i = 0; i < 96000; ++i)
            REQUIRE (a.processShapes (3.0, setting).centred == b.process (3.0, static_cast<Lfo::Wave> (setting)));
    }
    const auto range = [] (int setting) {
        Lfo l;
        l.prepare (48000.0, 5);
        float lo = 1e9f, hi = -1e9f;
        for (int i = 0; i < 96000; ++i)
        {
            const float v = l.processShapes (2.0, setting).prophet;
            lo = std::min (lo, v);
            hi = std::max (hi, v);
        }
        return std::pair<float, float> (lo, hi);
    };
    // Triangle bipolar; saw and square only positive (the square's trills go up); SAW+TRI adds them.
    CHECK_THAT (range (0).first, Catch::Matchers::WithinAbs (-1.0, 0.01));
    CHECK_THAT (range (0).second, Catch::Matchers::WithinAbs (1.0, 0.01));
    CHECK_THAT (range (1).first, Catch::Matchers::WithinAbs (0.0, 0.01));
    CHECK_THAT (range (1).second, Catch::Matchers::WithinAbs (1.0, 0.01));
    CHECK_THAT (range (2).first, Catch::Matchers::WithinAbs (0.0, 0.01));
    CHECK_THAT (range (2).second, Catch::Matchers::WithinAbs (1.0, 0.01));
    CHECK (range (4).second > 1.2f); // SAW + TRI
    CHECK (range (4).first < -0.3f);
}

TEST_CASE ("WHEEL-MOD noise: same level and pink slope at any sample rate", "[wheelmod]")
{
    for (const double sr : { 48000.0, 96000.0, 192000.0 })
    {
        augur::WheelModNoise n;
        n.prepare (sr, 3);
        const auto len = static_cast<std::size_t> (sr * 4.0);
        std::vector<float> x (len);
        for (auto& v : x)
            v = n.next();
        const double level = augur::test::rms (x, len / 4);
        INFO ("rate " << sr << " rms " << level);
        CHECK_THAT (level, Catch::Matchers::WithinAbs (0.4, 0.06));

        // Mean power density over 100-200 Hz against 400-800 Hz: two octaves of -3 dB/octave = 6 dB.
        const std::size_t fft = 1 << 17;
        const auto db = augur::test::spectrumDb (x, len - fft, fft);
        const auto bandPower = [&] (double lo, double hi) {
            double acc = 0.0;
            int count = 0;
            for (auto b = static_cast<std::size_t> (lo * fft / sr); b < static_cast<std::size_t> (hi * fft / sr); ++b, ++count)
                acc += std::pow (10.0, db[b] / 10.0);
            return 10.0 * std::log10 (acc / std::max (1, count));
        };
        const double slope = bandPower (100.0, 200.0) - bandPower (400.0, 800.0);
        INFO ("100-200 Hz over 400-800 Hz: " << slope << " dB");
        CHECK_THAT (slope, Catch::Matchers::WithinAbs (6.0, 2.0));
    }
}

TEST_CASE ("WHEEL-MOD moves only the switched destinations, by INITIAL + MOD wheel", "[wheelmod]")
{
    // LFO square at 1 Hz into FREQ A: 0..+1 (Prophet polarity); depth 0.5 -> 0.25 x 12 = +3 semitones on top.
    const double f0 = 440.0 * std::exp2 ((60.0 - 69.0) / 12.0);
    const auto extremes = [] (SynthParams p, float wheel) {
        auto e = std::make_unique<SynthEngine>();
        e->prepare (48000.0);
        e->setParams (p);
        e->setModWheel (wheel);
        e->noteOn (60, 0.9f);
        std::vector<float> l (3 * 48000), r (l.size());
        e->process (l.data(), r.data(), static_cast<int> (l.size()));
        const auto f = pitchTrack (l, 24000, 1920); // 40 ms windows after 0.5 s
        return std::pair<double, double> (*std::min_element (f.begin(), f.end()), *std::max_element (f.begin(), f.end()));
    };
    auto p = oneSaw();
    p.lfoWave = 2; // SQUARE
    p.lfoRate = 1.0f;
    p.wmAmount = 0.5f;

    const auto off = extremes (p, 0.0f); // no destination switched on
    CHECK_THAT (off.first, Catch::Matchers::WithinRel (f0, 0.003));
    CHECK_THAT (off.second, Catch::Matchers::WithinRel (f0, 0.003));

    p.wmFreqA = true;
    const auto initial = extremes (p, 0.0f);
    CHECK_THAT (initial.first, Catch::Matchers::WithinRel (f0, 0.005));
    CHECK_THAT (initial.second, Catch::Matchers::WithinRel (f0 * std::exp2 (3.0 / 12.0), 0.01));

    p.wmAmount = 0.0f; // the same depth from the MOD wheel alone
    const auto wheel = extremes (p, 0.5f);
    CHECK_THAT (wheel.second, Catch::Matchers::WithinRel (f0 * std::exp2 (3.0 / 12.0), 0.01));
    const auto silent = extremes (p, 0.0f);
    CHECK_THAT (silent.second, Catch::Matchers::WithinRel (f0, 0.003));
}

TEST_CASE ("GLIDE slews like the Prophet's OTA: a constant rate, so a wider interval takes longer", "[glide]")
{
    // GLIDE 0.3 = 0.3 s per octave. An RC glide reaches any interval in about the same time.
    const auto reachTime = [] (int from, int to) {
        auto p = oneSaw();
        p.glide = 0.3f;
        p.unison = true;
        p.voiceCount = 1;
        auto e = std::make_unique<SynthEngine>();
        e->prepare (48000.0);
        e->setParams (p);
        e->noteOn (from, 0.9f);
        std::vector<float> l (static_cast<std::size_t> (0.5 * 48000)), r (l.size());
        e->process (l.data(), r.data(), static_cast<int> (l.size()));
        e->noteOn (to, 0.9f);
        std::vector<float> g (static_cast<std::size_t> (1.0 * 48000)), gr (g.size());
        e->process (g.data(), gr.data(), static_cast<int> (g.size()));
        const double target = 440.0 * std::exp2 ((to - 69.0) / 12.0);
        const auto f = pitchTrack (g, 0, 480); // 10 ms windows
        for (std::size_t k = 0; k < f.size(); ++k)
            if (std::abs (f[k] / target - 1.0) < 0.01)
                return 0.01 * static_cast<double> (k);
        return 1.0;
    };
    const double octave = reachTime (72, 84), tone = reachTime (72, 74);
    INFO ("octave " << octave << " s, whole tone " << tone << " s");
    CHECK (octave > 0.22);
    CHECK (octave < 0.36);
    CHECK (tone / octave < 0.4); // an RC glide would give ~0.6
}
