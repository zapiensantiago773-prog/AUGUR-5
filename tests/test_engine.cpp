#include "TestHelpers.h"
#include "Engine/SynthEngine.h"
#include "Util/Random.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_range.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <memory>

using augur::SynthEngine;
using augur::SynthParams;

namespace
{
struct Render
{
    std::vector<float> left, right;
};

// Renders a short phrase; `blockSizes` decides how the host would slice it.
Render renderPhrase (SynthEngine& e, int totalSamples, augur::Random* blockRng, int fixedBlock)
{
    Render r;
    r.left.assign (static_cast<size_t> (totalSamples), 0.0f);
    r.right.assign (static_cast<size_t> (totalSamples), 0.0f);

    struct Ev { int at; int note; bool on; };
    const Ev events[] = { { 100, 60, true }, { 3000, 64, true }, { 5000, 67, true }, { 20000, 60, false },
                          { 26000, 64, false }, { 30000, 67, false }, { 31000, 72, true }, { 40000, 72, false } };

    int pos = 0;
    size_t nextEvent = 0;
    while (pos < totalSamples)
    {
        int block = blockRng != nullptr ? 1 + static_cast<int> (blockRng->nextFloat() * 700.0f) : fixedBlock;
        block = std::min (block, totalSamples - pos);

        // Split at events exactly like the plugin does.
        int done = 0;
        while (done < block)
        {
            while (nextEvent < std::size (events) && events[nextEvent].at == pos + done)
            {
                const auto& ev = events[nextEvent++];
                if (ev.on)
                    e.noteOn (ev.note, 0.8f);
                else
                    e.noteOff (ev.note);
            }
            int len = block - done;
            if (nextEvent < std::size (events))
                len = std::min (len, events[nextEvent].at - (pos + done));
            e.process (r.left.data() + pos + done, r.right.data() + pos + done, len);
            done += len;
        }
        pos += block;
    }
    return r;
}

SynthParams richPatch()
{
    SynthParams p;
    p.osc1Pulse = true;
    p.osc2Tri = true;
    p.osc1Sync = true;
    p.mixNoise = 0.1f;
    p.resonance = 0.7f;
    p.pmOn = true;
    p.pmFilterEnv = 0.3f;
    p.pmOsc2 = 0.2f;
    p.pmFilter = true;
    p.matrix[0] = { augur::ModSource::Lfo, augur::ModDest::FilterCutoff, 0.4f };
    p.matrix[1] = { augur::ModSource::Noise, augur::ModDest::Osc2Pw, 0.2f };
    p.chorusOn = p.delayOn = p.reverbOn = true;
    p.analogAge = 0.8f;
    return p;
}
} // namespace

TEST_CASE ("Output is bit-identical no matter how the host slices blocks", "[engine][determinism]")
{
    const double sr = GENERATE (44100.0, 96000.0);
    auto a = std::make_unique<SynthEngine>();
    auto b = std::make_unique<SynthEngine>();
    a->prepare (sr);
    b->prepare (sr);
    a->setParams (richPatch());
    b->setParams (richPatch());

    augur::Random blockRng (99);
    const auto ra = renderPhrase (*a, 48000, nullptr, 512);
    const auto rb = renderPhrase (*b, 48000, &blockRng, 0);

    float maxDiff = 0.0f;
    for (size_t i = 0; i < ra.left.size(); ++i)
        maxDiff = std::max ({ maxDiff, std::abs (ra.left[i] - rb.left[i]), std::abs (ra.right[i] - rb.right[i]) });
    CHECK (maxDiff == 0.0f);
}

TEST_CASE ("Engine renders sound at every sample rate and returns to silence", "[engine][samplerate]")
{
    const double sr = GENERATE (from_range (std::begin (augur::test::sampleRates), std::end (augur::test::sampleRates)));
    auto e = std::make_unique<SynthEngine>();
    e->prepare (sr);
    SynthParams p = richPatch();
    p.delayOn = p.reverbOn = false;
    p.analogAge = 0.0f; // no noise floor, so silence is exact
    p.aenvR = 0.05f;
    e->setParams (p);

    const auto r = renderPhrase (*e, static_cast<int> (sr * 1.5), nullptr, 256);
    float peak = 0.0f;
    for (size_t i = 0; i < r.left.size(); ++i)
    {
        REQUIRE (std::isfinite (r.left[i]));
        REQUIRE (std::isfinite (r.right[i]));
        peak = std::max (peak, std::abs (r.left[i]));
    }
    CHECK (peak > 0.02f);
    CHECK (peak < 2.0f);

    const auto tailStart = r.left.size() - static_cast<size_t> (0.05 * sr);
    CHECK (augur::test::rms (r.left, tailStart) < 1e-4);
}

TEST_CASE ("Poly mode never uses more voices than VOICES", "[engine][voices]")
{
    auto e = std::make_unique<SynthEngine>();
    e->prepare (48000.0);
    SynthParams p;
    p.voiceCount = 5;
    p.aenvR = 2.0f;
    e->setParams (p);

    std::vector<float> l (64), r (64);
    e->process (l.data(), r.data(), 64);
    for (int n = 0; n < 8; ++n)
        e->noteOn (48 + n, 1.0f);
    e->process (l.data(), r.data(), 64);

    int active = 0;
    for (int v = 0; v < SynthEngine::maxVoices; ++v)
        active += e->getVoiceLevel (v) > 0.0f ? 1 : 0;
    CHECK (active == 5);
}

TEST_CASE ("Sustain pedal holds released notes until it is lifted", "[engine][midi]")
{
    auto e = std::make_unique<SynthEngine>();
    e->prepare (48000.0);
    SynthParams p;
    p.aenvR = 0.01f;
    p.analogAge = 0.0f;
    e->setParams (p);

    std::vector<float> l (4800), r (4800);
    e->setSustain (true);
    e->noteOn (60, 1.0f);
    e->process (l.data(), r.data(), 4800);
    e->noteOff (60);
    e->process (l.data(), r.data(), 4800);
    CHECK (augur::test::rms (l, 2400) > 0.01);

    e->setSustain (false);
    for (int i = 0; i < 5; ++i)
        e->process (l.data(), r.data(), 4800);
    CHECK (augur::test::rms (l) < 1e-4);
}

namespace
{
SynthParams bareOscillator()
{
    SynthParams p;
    p.mixOsc1 = 1.0f;
    p.mixOsc2 = 0.0f;
    p.mixDrive = 0.0f;
    p.cutoffHz = 20000.0f;
    p.resonance = 0.0f;
    p.envAmount = 0.0f;
    p.keytrack = 0;
    p.aenvA = 0.001f;
    p.aenvS = 1.0f;
    p.analogAge = 0.0f;
    p.voiceDetune = 0.0f;
    p.lfoAmount = 0.0f;
    return p;
}

double playAndMeasure (SynthEngine& e, int note, double seconds, double sr)
{
    e.noteOn (note, 1.0f);
    std::vector<float> l (static_cast<size_t> (seconds * sr)), r (l.size());
    e.process (l.data(), r.data(), static_cast<int> (l.size()));
    e.noteOff (note);
    return augur::test::measureFrequency (l, sr, static_cast<size_t> (0.1 * sr));
}
} // namespace

TEST_CASE ("Played pitch after autotune is within the Prophet's tuning resolution", "[engine][autotune]")
{
    const int model = GENERATE (0, 1);
    constexpr double sr = 48000.0;
    auto e = std::make_unique<SynthEngine>();
    e->prepare (sr);
    auto p = bareOscillator();
    p.oscModel = model;
    e->setParams (p);

    for (const int note : { 36, 60, 84 })
    {
        const double f = playAndMeasure (*e, note, 1.0, sr);
        const double cents = 1200.0 * std::log2 (f / (440.0 * std::exp2 ((note - 69) / 12.0)));
        INFO ("model " << model << " note " << note << ": " << cents << " cents");
        CHECK (std::abs (cents) < 2.0);
    }
}

TEST_CASE ("OSC B LO FREQ drops 7.5 octaves (service manual 2-4)", "[engine]")
{
    constexpr double sr = 48000.0;
    auto e = std::make_unique<SynthEngine>();
    e->prepare (sr);
    auto p = bareOscillator();
    p.mixOsc1 = 0.0f;
    p.mixOsc2 = 1.0f;
    p.osc2LoFreq = true;
    p.osc2Kbd = false;
    e->setParams (p);

    // At 1.4 Hz the output DC blocker reshapes the ramp, so time the saw's reset edges instead.
    e->noteOn (60, 1.0f);
    std::vector<float> l (static_cast<size_t> (8.0 * sr)), r (l.size());
    e->process (l.data(), r.data(), static_cast<int> (l.size()));
    std::vector<size_t> resets;
    for (size_t n = 1; n < l.size(); ++n)
        if (l[n] - l[n - 1] < -0.05f && (resets.empty() || n - resets.back() > 1000))
            resets.push_back (n);
    REQUIRE (resets.size() >= 4);
    const double period = static_cast<double> (resets.back() - resets[1]) / static_cast<double> (resets.size() - 2) / sr;
    CHECK_THAT (1.0 / period, Catch::Matchers::WithinRel (261.6255653 / std::exp2 (7.5), 0.01));
}

TEST_CASE ("Even the first note carries no DC thump into the VCA", "[engine][dc]")
{
    constexpr double sr = 48000.0;
    auto e = std::make_unique<SynthEngine>();
    e->prepare (sr);
    SynthParams p;
    p.cutoffHz = 25.0f; // closed: only a DC step could get through
    p.envAmount = 0.0f;
    p.resonance = 0.0f;
    p.mixOsc2 = 0.0f;
    p.osc1Pulse = true;
    p.aenvA = 0.001f;
    p.aenvR = 0.05f;
    p.levelDb = 0.0f;
    p.analogAge = 0.0f;
    p.voiceCount = 1;
    e->setParams (p);

    e->warmUp(); // what the plugin does in prepareToPlay / after a preset or session change

    std::vector<float> l (static_cast<size_t> (2.0 * sr)), r (l.size());
    e->noteOn (69, 1.0f); // the very first note
    e->process (l.data(), r.data(), static_cast<int> (0.3 * sr));
    float peak = 0.0f;
    for (size_t n = 0; n < static_cast<size_t> (0.3 * sr); ++n)
        peak = std::max (peak, std::abs (l[n]));
    INFO ("peak " << 20.0 * std::log10 (peak + 1e-12) << " dBFS");
    CHECK (peak < 0.001f); // < -60 dBFS at full level
}

TEST_CASE ("Notes in the very first block use the current VOICES setting", "[engine][voices]")
{
    auto e = std::make_unique<SynthEngine>();
    e->prepare (48000.0);
    SynthParams p;
    p.voiceCount = 8;
    p.aenvS = 1.0f;
    e->setParams (p);
    for (int n = 0; n < 8; ++n)
        e->noteOn (48 + n, 1.0f); // before any audio has been processed
    std::vector<float> l (2048), r (2048);
    e->process (l.data(), r.data(), 2048);

    int active = 0;
    for (int v = 0; v < SynthEngine::maxVoices; ++v)
        active += e->getVoiceLevel (v) > 0.0f ? 1 : 0;
    CHECK (active == 8);

    // Lowering VOICES releases only the voices that no longer exist.
    p.voiceCount = 6;
    e->setParams (p);
    e->noteOn (80, 1.0f);
    for (int b = 0; b < 30; ++b)
        e->process (l.data(), r.data(), 2048);
    int sustaining = 0;
    for (int v = 0; v < 6; ++v)
        sustaining += e->getVoiceLevel (v) > 0.5f ? 1 : 0;
    CHECK (sustaining == 6);
}
