#include "TestHelpers.h"
#include "Effects/Fuzz.h"
#include "Util/HalfbandFir.h"
#include "Effects/TapeDelay.h"
#include "Effects/SpringReverb.h"
#include "Engine/SynthEngine.h"
#include "Util/Random.h"

#include <memory>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <cstdio>
#include <vector>

using augur::Fuzz;

TEST_CASE ("Fuzz aliasing at full sustain (measured, oversampled bus)", "[fx][fuzz][aliasing]")
{
    constexpr double sr = 96000.0;
    const double f0 = GENERATE (220.0 * 1.0013, 1234.5, 3100.7);
    Fuzz fuzz;
    fuzz.prepare (sr);
    constexpr std::size_t n = 65536, skip = 9600;
    std::vector<float> l (n + skip), r (n + skip);
    for (std::size_t i = 0; i < l.size(); ++i)
        l[i] = r[i] = 0.3f * static_cast<float> (std::sin (2.0 * 3.14159265358979 * f0 * static_cast<double> (i) / sr));
    fuzz.process (l.data(), r.data(), static_cast<int> (l.size()), true, 1.0f, 0.5f, 0.7f, 1.0f);

    // Only what lands below 20 kHz survives the half-band decimator to the host rate.
    const double worst = augur::test::worstAliasDb (l, sr, f0, 20000.0, skip, n);
    std::printf ("fuzz f0 %.1f Hz: worst alias %.1f dB\n", f0, worst);
    CHECK (worst < -105.0);
}

TEST_CASE ("Half-band up/down stages are transparent in the audio band and reject images", "[fx][halfband]")
{
    constexpr double sr = 96000.0;
    for (double f : { 100.0, 5000.0, 19000.0 })
    {
        augur::HalfbandUpsampler<31> up;
        augur::HalfbandDownsampler<31> down;
        double inE = 0.0, outE = 0.0;
        std::vector<float> high;
        for (int i = 0; i < 48000; ++i)
        {
            const float x = static_cast<float> (std::sin (2.0 * 3.14159265358979 * f * i / sr));
            float a, b;
            up.process (x, a, b);
            high.push_back (a);
            high.push_back (b);
            const float y = down.process (a, b);
            if (i > 1000)
            {
                inE += static_cast<double> (x) * x;
                outE += static_cast<double> (y) * y;
            }
        }
        INFO ("f " << f);
        CHECK (std::abs (10.0 * std::log10 (outE / inE)) < 0.01); // passband flat within 0.01 dB

        // The zero-stuffing image sits at 96 kHz - f in the 192 kHz stream.
        constexpr std::size_t n = 65536;
        const auto db = augur::test::spectrumDb (high, 4000, n);
        const double binHz = 2.0 * sr / n;
        const auto bin = static_cast<std::size_t> (std::lround ((sr - f) / binHz));
        double image = -300.0;
        for (std::size_t b = bin - 20; b <= bin + 20; ++b)
            image = std::max (image, db[b]);
        CHECK (image < -95.0);
    }
}

TEST_CASE ("Ping-pong delay alternates sides", "[fx][delay]")
{
    constexpr double sr = 48000.0;
    augur::TapeDelay d;
    d.prepare (sr);
    const int n = 48000;
    std::vector<float> l (static_cast<size_t> (n), 0.0f), r (static_cast<size_t> (n), 0.0f);
    d.process (l.data(), r.data(), n, 0.1f, 0.6f, 1.0f, true); // let the tape-style time glide settle
    std::fill (l.begin(), l.end(), 0.0f);
    std::fill (r.begin(), r.end(), 0.0f);
    l[0] = r[0] = 1.0f;
    d.process (l.data(), r.data(), n, 0.1f, 0.6f, 1.0f, true);
    const auto energy = [&] (const std::vector<float>& x, int from, int to) {
        double e = 0.0;
        for (int i = from; i < to; ++i)
            e += static_cast<double> (x[static_cast<size_t> (i)]) * x[static_cast<size_t> (i)];
        return e;
    };
    // First repeat (100 ms): left only; second: right only.
    CHECK (energy (l, 2000, 7000) > 100.0 * energy (r, 2000, 7000) + 1e-9);
    const int second = 2 * 4800;
    CHECK (energy (r, second - 2500, second + 2500) > 10.0 * energy (l, second - 2500, second + 2500));
}

TEST_CASE ("Engine with every effect on is bit-identical however the host slices blocks", "[fx][determinism]")
{
    augur::SynthParams p;
    p.fuzzOn = true;
    p.fx.on.fill (true);                     // the whole rack: drive, chorus, phaser, flanger, delay, echo, reverb, comp
    p.fx.order = { 3, 1, 7, 0, 5, 2, 6, 4 }; // in a shuffled order
    p.fx.chorus.mode = augur::rack::ChorusMode::juno12;
    p.fx.delay.sync = p.fx.delay.pingPong = true;
    p.fx.phaserSync = true;
    const auto render = [&] (augur::Random* rng) {
        auto e = std::make_unique<augur::SynthEngine>();
        e->prepare (48000.0);
        e->setParams (p);
        e->setTransport (124.0, false, 0.0);
        e->noteOn (57, 0.8f);
        e->noteOn (64, 0.8f);
        std::vector<float> l (30000), r (30000);
        int pos = 0;
        while (pos < 30000)
        {
            int len = rng != nullptr ? 1 + static_cast<int> (rng->nextFloat() * 600.0f) : 512;
            len = std::min (len, 30000 - pos);
            e->process (l.data() + pos, r.data() + pos, len);
            pos += len;
        }
        return l;
    };
    augur::Random rng (8);
    const auto a = render (nullptr);
    REQUIRE (a == render (&rng));
    for (float v : a)
        REQUIRE (std::isfinite (v));
}

TEST_CASE ("Fuzz MIX blends time-aligned signals (no comb filter)", "[fx][fuzz]")
{
    // At MIX 0 with the pedal engaged, the output is the dry signal through the resampling chain:
    // a pure delay, flat in magnitude (a misaligned dry path would notch it).
    constexpr double sr = 96000.0;
    for (double f : { 2400.0, 7000.0, 15000.0 })
    {
        Fuzz fuzz;
        fuzz.prepare (sr);
        std::vector<float> l (24000), r (24000);
        for (std::size_t i = 0; i < l.size(); ++i)
            l[i] = r[i] = 0.2f * static_cast<float> (std::sin (2.0 * 3.14159265358979 * f * static_cast<double> (i) / sr));
        std::vector<float> in (l);
        fuzz.process (l.data(), r.data(), static_cast<int> (l.size()), true, 1.0f, 0.5f, 0.7f, 0.0f);
        INFO ("f " << f);
        CHECK_THAT (augur::test::rms (l, 4000) / augur::test::rms (in, 4000), Catch::Matchers::WithinAbs (1.0, 0.002));
    }
}

TEST_CASE ("Spring reverb decays with the requested RT60", "[fx][spring]")
{
    constexpr double sr = 48000.0;
    const float rt60 = GENERATE (1.5f, 3.0f);
    augur::SpringReverb spring;
    spring.prepare (sr);
    const std::size_t n = static_cast<std::size_t> (sr * rt60 * 2.0);
    std::vector<float> in (n, 0.0f), l (n, 0.0f), r (n, 0.0f);
    in[0] = 1.0f;
    spring.process (in.data(), l.data(), r.data(), static_cast<int> (n), rt60, 0.5f, 1.0f);
    std::vector<double> edc (n);
    double acc = 0.0;
    for (std::size_t i = n; i-- > 0;)
    {
        acc += static_cast<double> (l[i]) * l[i] + static_cast<double> (r[i]) * r[i];
        edc[i] = acc;
    }
    const auto timeAt = [&] (double db) {
        for (std::size_t i = 0; i < n; ++i)
            if (10.0 * std::log10 (edc[i] / edc[0]) <= db)
                return static_cast<double> (i) / sr;
        return static_cast<double> (n) / sr;
    };
    const double measured = 3.0 * (timeAt (-25.0) - timeAt (-5.0));
    INFO ("spring RT60 set " << rt60 << " measured " << measured);
    CHECK (measured > 0.6 * rt60);
    CHECK (measured < 1.4 * rt60);
}

TEST_CASE ("Engine with tape echo and spring is bit-identical however the host slices blocks", "[fx][determinism]")
{
    augur::SynthParams p;
    p.fx.on[static_cast<size_t> (augur::rack::FxId::echo)] = true;
    p.fx.echo.mode = 6;
    p.fx.echoSpring = 0.6;
    p.fx.on[static_cast<size_t> (augur::rack::FxId::reverb)] = true;
    p.fx.reverbSpring = true;
    const auto render = [&] (augur::Random* rng) {
        auto e = std::make_unique<augur::SynthEngine>();
        e->prepare (48000.0);
        e->setParams (p);
        e->noteOn (60, 0.8f);
        std::vector<float> l (30000), r (30000);
        int pos = 0;
        while (pos < 30000)
        {
            int len = rng != nullptr ? 1 + static_cast<int> (rng->nextFloat() * 600.0f) : 512;
            len = std::min (len, 30000 - pos);
            e->process (l.data() + pos, r.data() + pos, len);
            pos += len;
        }
        return l;
    };
    augur::Random rng (12);
    REQUIRE (render (nullptr) == render (&rng));
}
