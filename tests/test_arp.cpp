#include "Engine/SynthEngine.h"
#include "Modulation/Arpeggiator.h"
#include "Util/Random.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <memory>
#include <vector>

using augur::Arpeggiator;

namespace
{
struct Timed
{
    double beat;
    bool on;
    int note;
};

// Steps the arpeggiator on a fine beat grid and records what it emits.
std::vector<Timed> run (Arpeggiator& arp, const Arpeggiator::Settings& s, double from, double to)
{
    std::vector<Timed> out;
    Arpeggiator::Event ev[3];
    const auto ticks = static_cast<long> (std::lround ((to - from) * 3840.0));
    for (long k = 0; k < ticks; ++k)
    {
        const double b = from + static_cast<double> (k) / 3840.0;
        const int n = arp.fire (b + 1.0e-9, s, ev);
        for (int i = 0; i < n; ++i)
            out.push_back ({ b, ev[i].on, ev[i].note });
    }
    return out;
}

std::vector<int> onNotes (const std::vector<Timed>& events)
{
    std::vector<int> notes;
    for (const auto& e : events)
        if (e.on)
            notes.push_back (e.note);
    return notes;
}

void press (Arpeggiator& arp, const Arpeggiator::Settings& s, std::initializer_list<int> notes)
{
    bool start = false;
    for (int n : notes)
        start = arp.keyDown (n, 0.8f, s) || start;
    if (start)
        arp.start (0.0, false, s);
}
} // namespace

TEST_CASE ("Arp modes produce the expected note order", "[arp]")
{
    Arpeggiator arp;
    Arpeggiator::Settings s;

    SECTION ("Up over two octaves")
    {
        s.mode = Arpeggiator::Mode::Up;
        s.octaves = 2;
        press (arp, s, { 67, 60, 64 });
        const auto notes = onNotes (run (arp, s, 0.0, 2.0));
        REQUIRE (notes == std::vector<int> { 60, 64, 67, 72, 76, 79, 60, 64 });
    }
    SECTION ("Down")
    {
        s.mode = Arpeggiator::Mode::Down;
        press (arp, s, { 60, 64, 67 });
        REQUIRE (onNotes (run (arp, s, 0.0, 1.0)) == std::vector<int> { 67, 64, 60, 67 });
    }
    SECTION ("Up-Down does not repeat the turning notes")
    {
        s.mode = Arpeggiator::Mode::UpDown;
        press (arp, s, { 60, 64, 67, 72 });
        REQUIRE (onNotes (run (arp, s, 0.0, 2.0)) == std::vector<int> { 60, 64, 67, 72, 67, 64, 60, 64 });
    }
    SECTION ("Order follows the playing order")
    {
        s.mode = Arpeggiator::Mode::Order;
        press (arp, s, { 67, 60, 64 });
        REQUIRE (onNotes (run (arp, s, 0.0, 1.0)) == std::vector<int> { 67, 60, 64, 67 });
    }
    SECTION ("Random only plays held notes")
    {
        s.mode = Arpeggiator::Mode::Random;
        press (arp, s, { 60, 64, 67 });
        for (int n : onNotes (run (arp, s, 0.0, 8.0)))
            REQUIRE ((n == 60 || n == 64 || n == 67));
    }
}

TEST_CASE ("Arp steps, gate and swing land on the beat grid", "[arp]")
{
    Arpeggiator arp;
    Arpeggiator::Settings s;
    s.stepBeats = 0.25;
    s.gate = 0.5f;
    s.swing = 0.2f;
    press (arp, s, { 60 });
    const auto events = run (arp, s, 0.0, 1.0);

    std::vector<double> ons, offs;
    for (const auto& e : events)
        (e.on ? ons : offs).push_back (e.beat);
    REQUIRE (ons.size() == 4);
    const double grid = 1.0 / 3840.0;
    // Even steps on the grid, odd steps late by swing * step.
    CHECK_THAT (ons[0], Catch::Matchers::WithinAbs (0.0, grid));
    CHECK_THAT (ons[1], Catch::Matchers::WithinAbs (0.25 + 0.05, grid));
    CHECK_THAT (ons[2], Catch::Matchers::WithinAbs (0.5, grid));
    CHECK_THAT (ons[3], Catch::Matchers::WithinAbs (0.75 + 0.05, grid));
    // Gate: half a step after each note-on.
    CHECK_THAT (offs[0], Catch::Matchers::WithinAbs (0.125, grid));
    CHECK_THAT (offs[1], Catch::Matchers::WithinAbs (0.425, grid));
}

TEST_CASE ("Arp on a running transport waits for the next grid line", "[arp]")
{
    Arpeggiator arp;
    Arpeggiator::Settings s;
    s.stepBeats = 0.5;
    arp.keyDown (60, 0.8f, s);
    arp.start (10.1, true, s);
    const auto events = run (arp, s, 10.1, 11.0);
    REQUIRE (! events.empty());
    CHECK_THAT (events.front().beat, Catch::Matchers::WithinAbs (10.5, 1.0 / 3840.0));
}

TEST_CASE ("Arp latch keeps the chord until a new one is played", "[arp]")
{
    Arpeggiator arp;
    Arpeggiator::Settings s;
    s.latch = true;
    press (arp, s, { 60, 64 });
    REQUIRE_FALSE (arp.keyUp (60, s));
    REQUIRE_FALSE (arp.keyUp (64, s));
    REQUIRE (onNotes (run (arp, s, 0.0, 1.0)) == std::vector<int> { 60, 64, 60, 64 });

    arp.keyDown (70, 0.8f, s); // all keys were up: a new chord replaces the latched one
    REQUIRE (onNotes (run (arp, s, 1.0, 1.5)) == std::vector<int> { 70, 70 });

    REQUIRE (arp.unlatch() == false); // 70 still held
    arp.keyUp (70, s);
    REQUIRE (arp.unlatch());
}

TEST_CASE ("Arp without latch stops when the last key is released", "[arp]")
{
    Arpeggiator arp;
    Arpeggiator::Settings s;
    press (arp, s, { 60, 64 });
    REQUIRE_FALSE (arp.keyUp (60, s));
    REQUIRE (arp.keyUp (64, s));
}

TEST_CASE ("Arpeggiated engine output is bit-identical no matter how the host slices blocks", "[arp][engine][determinism]")
{
    constexpr double sr = 48000.0;
    constexpr int total = 48000;

    const auto render = [&] (augur::Random* blockRng, int fixedBlock) {
        auto e = std::make_unique<augur::SynthEngine>();
        e->prepare (sr);
        augur::SynthParams p;
        p.arpOn = true;
        p.arpMode = 2;
        p.arpOctaves = 2;
        p.arpRate = 6; // 1/16T: steps between samples
        p.arpSwing = 0.13f;
        p.arpGate = 0.7f;
        e->setParams (p);
        e->setTransport (127.0, false, 0.0);

        std::vector<float> l (total), r (total);
        int pos = 0;
        bool pressed = false, released = false;
        while (pos < total)
        {
            int block = blockRng != nullptr ? 1 + static_cast<int> (blockRng->nextFloat() * 700.0f) : fixedBlock;
            block = std::min (block, total - pos);
            int done = 0;
            while (done < block)
            {
                const int at = pos + done;
                if (! pressed && at == 777)
                {
                    e->noteOn (60, 0.9f);
                    e->noteOn (63, 0.7f);
                    e->noteOn (67, 0.8f);
                    pressed = true;
                }
                if (! released && at == 40000)
                {
                    e->noteOff (60);
                    e->noteOff (63);
                    e->noteOff (67);
                    released = true;
                }
                int len = block - done;
                if (! pressed)
                    len = std::min (len, 777 - at);
                else if (! released)
                    len = std::min (len, 40000 - at);
                e->process (l.data() + at, r.data() + at, len);
                done += len;
            }
            pos += block;
        }
        return l;
    };

    augur::Random rng (1234);
    const auto a = render (nullptr, 512);
    const auto b = render (&rng, 0);
    REQUIRE (a == b);

    // It plays: energy while the arpeggio runs.
    double energy = 0.0;
    for (int i = 2000; i < 40000; ++i)
        energy += static_cast<double> (a[static_cast<size_t> (i)]) * a[static_cast<size_t> (i)];
    REQUIRE (energy > 1.0);
}
