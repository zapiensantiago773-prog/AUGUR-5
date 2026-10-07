#include "TestHelpers.h"
#include "Engine/SynthEngine.h"
#include "Util/FastMath.h"
#include "Util/Random.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <memory>
#include <vector>

using augur::ModDest;
using augur::ModSource;
using augur::SynthEngine;
using augur::SynthParams;

namespace
{
SynthParams openParams()
{
    SynthParams p;
    p.mixOsc1 = 0.0f;
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

std::vector<float> play (const SynthParams& p, int note, double seconds, double sr = 48000.0)
{
    auto e = std::make_unique<SynthEngine>();
    e->prepare (sr);
    e->setParams (p);
    e->noteOn (note, 0.9f);
    std::vector<float> l (static_cast<size_t> (seconds * sr)), r (l.size());
    e->process (l.data(), r.data(), static_cast<int> (l.size()));
    return l;
}
} // namespace

TEST_CASE ("fastmath::log2 is accurate", "[math]")
{
    augur::Random r (3);
    for (int i = 0; i < 100000; ++i)
    {
        const float x = std::exp2 (r.nextBipolar() * 20.0f);
        REQUIRE_THAT (augur::fastmath::log2 (x), Catch::Matchers::WithinAbs (std::log2 (x), 2.0e-6));
    }
}

TEST_CASE ("Sub oscillator plays one or two octaves below OSC A", "[modulation][sub]")
{
    auto p = openParams();
    p.mixSub = 1.0f;
    const double f0 = 440.0 * std::exp2 ((60.0 - 69.0) / 12.0);

    p.subOctave = 0;
    auto out = play (p, 60, 1.0);
    CHECK (augur::test::rms (out, out.size() / 2) > 0.02);
    CHECK_THAT (augur::test::measureFrequency (out, 48000.0, out.size() / 2), Catch::Matchers::WithinRel (f0 / 2.0, 0.003));

    p.subOctave = 1;
    out = play (p, 60, 1.0);
    CHECK_THAT (augur::test::measureFrequency (out, 48000.0, out.size() / 2), Catch::Matchers::WithinRel (f0 / 4.0, 0.003));
}

TEST_CASE ("Ring mod and FM change the sound and stay finite", "[modulation]")
{
    auto p = openParams();
    p.mixOsc1 = 0.8f;
    p.osc2Semi = 7;
    const auto dry = play (p, 60, 0.5);

    auto ringP = p;
    ringP.mixOsc1 = 0.0f;
    ringP.mixRing = 1.0f;
    const auto ring = play (ringP, 60, 0.5);

    auto fmP = p;
    fmP.crossMod = 0.7f;
    const auto fm = play (fmP, 60, 0.5);

    for (const auto* v : { &ring, &fm })
    {
        double diff = 0.0;
        for (size_t i = 0; i < v->size(); ++i)
        {
            REQUIRE (std::isfinite ((*v)[i]));
            diff += std::abs ((*v)[i] - dry[i]);
        }
        CHECK (augur::test::rms (*v, v->size() / 2) > 0.01);
        CHECK (diff / static_cast<double> (v->size()) > 0.005);
    }
}

TEST_CASE ("Mod envelope and LFO 2 drive their matrix destinations", "[modulation]")
{
    // Mod env -> OSC 1 FREQ: with attack 0 and full decay the pitch starts high and falls back.
    auto p = openParams();
    p.mixOsc1 = 0.8f;
    p.menvA = 0.001f;
    p.menvD = 0.5f;
    p.menvS = 0.0f;
    p.matrix[4] = { ModSource::ModEnv, ModDest::Osc1Freq, 0.5f }; // +6 semitones
    const auto out = play (p, 60, 1.0);
    const std::vector<float> start (out.begin() + 200, out.begin() + 1400), end (out.begin() + 30000, out.end());
    CHECK (augur::test::measureFrequency (start, 48000.0) > 1.2 * augur::test::measureFrequency (end, 48000.0));

    // LFO 2 -> amp level: the level moves at the LFO rate.
    auto q = openParams();
    q.mixOsc1 = 0.8f;
    q.lfo2Rate = 4.0f;
    q.lfo2Wave = 0;
    q.matrix[5] = { ModSource::Lfo2, ModDest::AmpLevel, -1.0f };
    const auto trem = play (q, 60, 1.0);
    double lo = 1e9, hi = 0.0;
    for (size_t w = 0; w + 1200 <= trem.size(); w += 1200) // 25 ms windows
    {
        const std::vector<float> win (trem.begin() + static_cast<long> (w), trem.begin() + static_cast<long> (w + 1200));
        const double level = augur::test::rms (win);
        if (w > 4800)
        {
            lo = std::min (lo, level);
            hi = std::max (hi, level);
        }
    }
    CHECK (hi > 3.0 * lo);
}

TEST_CASE ("Every active matrix route changes the sound; inactive ones are exactly silent", "[modulation][matrix]")
{
    // Each of the 12 x 16 source / destination pairs, one at a time, against the same patch with the amount at 0.
    // Every source is made non-zero: a pulse wave on both oscillators (PW is audible), cutoff mid-way, the LFO at
    // full amount, mod wheel and aftertouch up, a note away from middle C (keytrack). A rate is only audible when
    // its LFO goes somewhere, so slots 7 and 8 send LFO and LFO 2 to the cutoff in every render. Levels that sit at
    // 0 only move one way (a negative push on a silent sub stays silent), so a route counts as working when
    // either sign of the amount changes the sound.
    SynthParams base = openParams();
    base.mixOsc1 = 0.5f;
    base.mixOsc2 = 0.4f;
    base.osc1Saw = base.osc2Saw = false;
    base.osc1Pulse = base.osc2Pulse = true;
    base.osc1Pw = base.osc2Pw = 0.3f;
    base.osc2Semi = 7;
    base.cutoffHz = 1500.0f;
    base.resonance = 0.3f;
    base.lfoAmount = 1.0f;
    base.lfoRate = 5.0f;
    base.lfo2Rate = 3.0f;
    base.menvA = 0.05f;
    base.menvS = 0.6f;
    base.matrix[6] = { ModSource::Lfo, ModDest::FilterCutoff, 0.3f };
    base.matrix[7] = { ModSource::Lfo2, ModDest::FilterCutoff, 0.4f };

    const auto render = [] (const SynthParams& p) {
        auto e = std::make_unique<SynthEngine>();
        e->prepare (48000.0);
        e->setParams (p);
        e->setModWheel (1.0f);
        e->setChannelPressure (1.0f);
        e->noteOn (72, 0.9f);
        std::vector<float> l (14400), r (l.size());
        e->process (l.data(), r.data(), static_cast<int> (l.size()));
        return l;
    };
    const auto reference = render (base);
    const double refRms = augur::test::rms (reference);
    REQUIRE (refRms > 0.01);

    int active = 0, inactive = 0;
    for (int s = 0; s < static_cast<int> (ModSource::count); ++s)
        for (int d = 0; d < static_cast<int> (ModDest::count); ++d)
        {
            const auto source = static_cast<ModSource> (s);
            const auto dest = static_cast<ModDest> (d);
            double relDiff = 0.0;
            for (const float amount : { 0.8f, -0.8f })
            {
                auto p = base;
                p.matrix[0] = { source, dest, amount };
                const auto out = render (p);
                double diff = 0.0;
                for (size_t i = 0; i < out.size(); ++i)
                {
                    REQUIRE (std::isfinite (out[i]));
                    diff += static_cast<double> (out[i] - reference[i]) * static_cast<double> (out[i] - reference[i]);
                }
                relDiff = std::max (relDiff, std::sqrt (diff / static_cast<double> (out.size())) / refRms);
            }
            INFO ("source " << s << " -> destination " << d << ": difference " << relDiff);
            if (augur::isMatrixRouteActive (source, dest))
            {
                CHECK (relDiff > 0.01); // audible: more than 1 % of the signal (-40 dB)
                ++active;
            }
            else
            {
                CHECK (relDiff == 0.0);
                ++inactive;
            }
        }
    CHECK (active == 12 * 16 - 10);
    CHECK (inactive == 10);
}

TEST_CASE ("Output with every new modulation feature is bit-identical however the host slices blocks", "[modulation][determinism]")
{
    auto p = openParams();
    p.mixOsc1 = 0.6f;
    p.mixOsc2 = 0.3f;
    p.mixRing = 0.3f;
    p.mixSub = 0.4f;
    p.subOctave = 1;
    p.crossMod = 0.4f;
    p.lfo2Retrig = false;
    p.lfo2Wave = 6;
    p.lfo2Rate = 3.0f;
    p.matrix[4] = { ModSource::Lfo2, ModDest::FilterCutoff, 0.3f };
    p.matrix[5] = { ModSource::ModEnv, ModDest::CrossMod, 0.5f };
    p.matrix[6] = { ModSource::NoteRandom, ModDest::Drive, 0.4f };
    p.matrix[7] = { ModSource::Keytrack, ModDest::Lfo2Rate, 0.5f };
    p.cutoffHz = 3000.0f;
    p.filterModel = 3;

    const auto render = [&] (augur::Random* rng) {
        auto e = std::make_unique<SynthEngine>();
        e->prepare (48000.0);
        e->setParams (p);
        std::vector<float> l (36000), r (36000);
        const int events[] = { 100, 5000, 11111, 20000 };
        const int notes[] = { 48, 55, 63, 70 };
        int pos = 0, next = 0;
        while (pos < 36000)
        {
            if (next < 4 && pos == events[next])
                e->noteOn (notes[next++], 0.8f);
            int len = rng != nullptr ? 1 + static_cast<int> (rng->nextFloat() * 500.0f) : 256;
            len = std::min (len, 36000 - pos);
            if (next < 4)
                len = std::min (len, events[next] - pos);
            e->process (l.data() + pos, r.data() + pos, len);
            pos += len;
        }
        return l;
    };
    augur::Random rng (99);
    REQUIRE (render (nullptr) == render (&rng));
}
