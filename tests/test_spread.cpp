// VOICE SPREAD modes (D-060). BY VOICE: each POLY voice has a fixed place and the rotating assigner moves notes side to
// side (measured up to 20 dB note to note at SPREAD 0.6). CENTRED: the voices keep their places relative to each other
// and the whole image is shifted so that its energy is balanced: a note alone sits in the middle, a chord keeps its
// width without leaning, and the stacked modes (UNISON, DUO), already symmetric, are left exactly as they are.
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "Engine/SynthEngine.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <random>
#include <vector>

using namespace augur;

namespace
{
constexpr double sr = 48000.0;

struct Event
{
    double t;
    int note;
    bool on;
};

struct Window
{
    double from, to;
};

struct Render
{
    std::vector<float> l, r;
};

Render play (const SynthParams& p, std::vector<Event> events, double seconds, int block = 32)
{
    // Each event on one whole sample, worked out once: comparing t * sr with the position in one place and rounding
    // t * sr - pos in another landed an event a sample apart between block sizes where the compiler fuses the
    // multiply and the subtraction (Apple clang, arm64).
    std::stable_sort (events.begin(), events.end(), [] (const Event& a, const Event& b) { return a.t < b.t; });
    std::vector<size_t> at (events.size());
    for (size_t i = 0; i < events.size(); ++i)
        at[i] = static_cast<size_t> (std::llround (events[i].t * sr));
    auto e = std::make_unique<SynthEngine>();
    e->prepare (sr, SynthEngine::defaultUnitSeed, 1);
    e->setParams (p);
    const auto total = static_cast<size_t> (seconds * sr);
    Render out { std::vector<float> (total), std::vector<float> (total) };
    std::mt19937 rng (7);
    size_t k = 0;
    for (size_t pos = 0; pos < total;)
    {
        while (k < events.size() && at[k] <= pos)
        {
            if (events[k].on)
                e->noteOn (events[k].note, 0.8f);
            else
                e->noteOff (events[k].note);
            ++k;
        }
        int n = block > 0 ? block : 1 + static_cast<int> (rng() % 300); // block 0: random sizes
        n = static_cast<int> (std::min<size_t> (static_cast<size_t> (n), total - pos));
        if (k < events.size()) // events land on their sample whatever the block size
            n = static_cast<int> (std::min<size_t> (static_cast<size_t> (n), at[k] - pos));
        e->process (out.l.data() + pos, out.r.data() + pos, n);
        pos += static_cast<size_t> (n);
    }
    return out;
}

double balanceDb (const Render& x, Window w) // + = louder on the left
{
    double ll = 0.0, rr = 0.0;
    for (auto i = static_cast<size_t> (w.from * sr); i < static_cast<size_t> (w.to * sr); ++i)
    {
        ll += static_cast<double> (x.l[i]) * x.l[i];
        rr += static_cast<double> (x.r[i]) * x.r[i];
    }
    return 10.0 * std::log10 ((ll + 1.0e-30) / (rr + 1.0e-30));
}

double sideOverMidDb (const Render& x, Window w)
{
    double mm = 0.0, ss = 0.0;
    for (auto i = static_cast<size_t> (w.from * sr); i < static_cast<size_t> (w.to * sr); ++i)
    {
        const double m = 0.5 * (static_cast<double> (x.l[i]) + x.r[i]), s = 0.5 * (static_cast<double> (x.l[i]) - x.r[i]);
        mm += m * m;
        ss += s * s;
    }
    return 10.0 * std::log10 ((ss + 1.0e-30) / (mm + 1.0e-30));
}

// A melody of separate notes (each released before the next): every note lands on the next voice.
void melody (std::vector<Event>& ev, std::vector<Window>& w)
{
    const int notes[10] = { 60, 62, 64, 65, 67, 65, 64, 62, 60, 67 };
    for (int i = 0; i < 10; ++i)
    {
        const double t = 0.1 + i * 0.8;
        ev.push_back ({ t, notes[i], true });
        ev.push_back ({ t + 0.4, notes[i], false });
        w.push_back ({ t + 0.05, t + 0.35 });
    }
}

// Long triads, one after another: each chord lands on a different set of voices.
void chords (std::vector<Event>& ev, std::vector<Window>& w)
{
    const int roots[8] = { 60, 65, 67, 69, 62, 64, 65, 67 };
    for (int c = 0; c < 8; ++c)
    {
        const double t = 0.1 + c * 3.0;
        for (int k : { 0, 4, 7 })
        {
            ev.push_back ({ t, roots[c] + k, true });
            ev.push_back ({ t + 2.9, roots[c] + k, false });
        }
        w.push_back ({ t + 0.3, t + 2.85 });
    }
}

double swing (const Render& x, const std::vector<Window>& w, double& widthDb)
{
    double lo = 1.0e9, hi = -1.0e9;
    widthDb = 0.0;
    for (const auto& win : w)
    {
        const double b = balanceDb (x, win);
        lo = std::min (lo, b);
        hi = std::max (hi, b);
        widthDb += sideOverMidDb (x, win) / static_cast<double> (w.size());
    }
    return hi - lo;
}
} // namespace

TEST_CASE ("SPREAD CENTRED: a note alone sits in the middle, whichever voice plays it", "[engine][spread]")
{
    const float spread = GENERATE (0.3f, 0.6f, 1.0f);
    std::vector<Event> ev;
    std::vector<Window> w;
    melody (ev, w);
    SynthParams p;
    p.voiceSpread = spread;
    p.aenvR = 0.3f;
    double width = 0.0;
    p.spreadMode = 1;
    const double byVoice = swing (play (p, ev, 8.2), w, width);
    p.spreadMode = 0;
    const double centred = swing (play (p, ev, 8.2), w, width);
    WARN ("SPREAD " << spread << ", separate notes: balance swings " << byVoice << " dB BY VOICE, " << centred << " dB CENTRED");
    CHECK (byVoice > 8.0);
    CHECK (centred < 0.05);
}

TEST_CASE ("SPREAD CENTRED: chords stop leaning and keep their width", "[engine][spread]")
{
    const float spread = GENERATE (0.3f, 0.6f, 0.85f, 1.0f);
    std::vector<Event> ev;
    std::vector<Window> w;
    chords (ev, w);
    SynthParams p;
    p.voiceSpread = spread;
    p.aenvR = 0.3f;
    double widthByVoice = 0.0, widthCentred = 0.0;
    p.spreadMode = 1;
    const double byVoice = swing (play (p, ev, 24.2), w, widthByVoice);
    p.spreadMode = 0;
    const double centred = swing (play (p, ev, 24.2), w, widthCentred);
    WARN ("SPREAD " << spread << ", triads: balance swings " << byVoice << " dB BY VOICE, " << centred << " dB CENTRED; side/mid "
                    << widthByVoice << " / " << widthCentred << " dB");
    CHECK (byVoice > 4.0);
    CHECK (centred < 1.2);
    // The width that stays is the chord's own: BY VOICE's side counts its lean too, so CENTRED may read a little less.
    CHECK (widthCentred > widthByVoice - 3.0);
    CHECK (widthCentred < widthByVoice + 0.5);
}

TEST_CASE ("SPREAD CENTRED follows PAN: a note alone sits where PAN puts the instrument", "[engine][spread]")
{
    std::vector<Event> ev;
    std::vector<Window> w;
    melody (ev, w);
    SynthParams p;
    p.voiceSpread = 0.8f;
    p.voicePan = 0.5f;
    p.aenvR = 0.3f;
    const auto x = play (p, ev, 8.2);
    const double angle = (0.5 + 1.0) * 0.25 * 3.14159265358979;
    const double expected = 20.0 * std::log10 (std::cos (angle) / std::sin (angle));
    for (const auto& win : w)
        CHECK (std::abs (balanceDb (x, win) - expected) < 0.05);
}

// Also the regression test of a determinism fault found here (both modes, 1.6 too): a voice whose release ended inside a
// segment stopped rendering at the segment's end, so its noise sequence (and the next note it played) depended on the
// host's block sizes. -3.6 dB of difference between 1-sample and 512-sample blocks on this phrase; now 0.
TEST_CASE ("SPREAD CENTRED renders the same whatever the host's block sizes", "[engine][spread][determinism]")
{
    std::vector<Event> ev;
    std::vector<Window> w;
    chords (ev, w);
    melody (ev, w);
    SynthParams p;
    p.voiceSpread = 0.7f;
    p.spreadMode = GENERATE (0, 1);
    const auto a = play (p, ev, 6.0, 1);
    const auto b = play (p, ev, 6.0, 0);
    const auto c = play (p, ev, 6.0, 512);
    CHECK (a.l == b.l);
    CHECK (a.r == b.r);
    CHECK (a.l == c.l);
    CHECK (a.r == c.r);
}

TEST_CASE ("SPREAD CENTRED leaves UNISON, DUO and one voice exactly as BY VOICE", "[engine][spread]")
{
    const int which = GENERATE (0, 1, 2);
    std::vector<Event> ev;
    std::vector<Window> w;
    chords (ev, w);
    SynthParams p;
    p.voiceSpread = 0.8f;
    p.voiceDetune = 0.5f;
    if (which == 0)
        p.unison = true;
    else if (which == 1)
    {
        p.voiceMode = 1;
        p.voiceCount = 6;
    }
    else
        p.voiceCount = 1;
    p.spreadMode = 1;
    const auto a = play (p, ev, 7.0);
    p.spreadMode = 0;
    const auto b = play (p, ev, 7.0);
    CHECK (a.l == b.l);
    CHECK (a.r == b.r);
}
