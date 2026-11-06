// The bass stays mono and in phase (D-057): through every stereo unit of the rack, under the voices' panning, and
// from the sub-oscillator, which switches exactly on OSC A's resets. Plus the rack faults found on the way: DC out of
// the DRIVE, the CRUSH comb, the delay's MIX zipper, a non-finite sample living on in the tails.
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "RackMeasure.h"

#include "Engine/SynthEngine.h"
#include "Rack/FxRack.h"

#include <cmath>
#include <complex>
#include <functional>
#include <limits>
#include <vector>

using namespace augur;
using namespace augur::rack;
namespace m = augur::rackmeasure;

namespace
{
constexpr double sr = 48000.0;

struct Stereo
{
    std::vector<double> l, r;
};

// The band [lo, hi] of each channel from one FFT: energies, cross term and mono sum.
struct Band
{
    double ll = 0.0, rr = 0.0, lr = 0.0, mono = 0.0;
    double corr() const { return lr / std::sqrt (ll * rr + 1.0e-30); }
    double sideDb() const { return 10.0 * std::log10 ((0.5 * (ll + rr) - mono) / (mono + 1.0e-30) + 1.0e-30); } // side over mid
    double balanceDb() const { return 10.0 * std::log10 ((ll + 1.0e-30) / (rr + 1.0e-30)); }
    double levelDb() const { return 10.0 * std::log10 (0.5 * (ll + rr) + 1.0e-30); }
};

Band band (const Stereo& s, double lo, double hi, size_t from = 0)
{
    size_t n = 1;
    while (n < s.l.size() - from)
        n <<= 1;
    std::vector<std::complex<double>> a (n), b (n);
    for (size_t i = from; i < s.l.size(); ++i)
    {
        a[i - from] = s.l[i];
        b[i - from] = s.r[i];
    }
    m::fft (a);
    m::fft (b);
    Band out;
    for (size_t k = 1; k < n / 2; ++k)
    {
        const double f = static_cast<double> (k) * sr / static_cast<double> (n);
        if (f < lo || f > hi)
            continue;
        out.ll += std::norm (a[k]);
        out.rr += std::norm (b[k]);
        out.lr += (a[k] * std::conj (b[k])).real();
        out.mono += 0.25 * std::norm (a[k] + b[k]);
    }
    return out;
}

// A mono bass (40 + 60 Hz) under a 1 kHz tone, through the whole rack with one unit (or tank) switched on.
Stereo throughRack (const FxParams& p, double seconds)
{
    FxRack rack;
    rack.prepare (sr, 5);
    Stereo s;
    const auto n = static_cast<size_t> (seconds * sr);
    s.l.resize (n);
    s.r.resize (n);
    for (size_t i = 0; i < n; ++i)
    {
        const double t = static_cast<double> (i) / sr;
        double l = 0.3 * std::sin (2.0 * m::pi * 40.0 * t) + 0.3 * std::sin (2.0 * m::pi * 60.0 * t) + 0.2 * std::sin (2.0 * m::pi * 1000.0 * t);
        double r = l;
        rack.process (l, r, p, 120.0);
        s.l[i] = l;
        s.r[i] = r;
    }
    return s;
}

double inputBassDb()
{
    // 0.3 sin at 40 Hz + 0.3 sin at 60 Hz, as Band::levelDb measures it over the same span.
    Stereo s;
    const auto n = static_cast<size_t> (3.0 * sr);
    for (size_t i = 0; i < n; ++i)
    {
        const double t = static_cast<double> (i) / sr;
        const double x = 0.3 * std::sin (2.0 * m::pi * 40.0 * t) + 0.3 * std::sin (2.0 * m::pi * 60.0 * t);
        s.l.push_back (x);
        s.r.push_back (x);
    }
    return band (s, 20.0, 120.0, static_cast<size_t> (sr)).levelDb();
}
} // namespace

TEST_CASE ("Chorus: the bass of its wet comes out in the middle, the dry untouched", "[fx][bassphase]")
{
    // (The phaser and the flanger are left as they were: their bass loses at most 0.84 dB in mono, and their stereo
    // comes from the panned voices they process channel by channel, which a high-pass on the side would reshuffle up
    // to ~1 kHz.)
    const int mode = GENERATE (0, 1, 2, 3, 4);
    const double mix = GENERATE (0.5, 1.0);
    FxParams p;
    p.on[static_cast<size_t> (FxId::chorus)] = true;
    p.chorus.mode = static_cast<ChorusMode> (mode);
    p.chorus.width = 1.5;
    p.chorus.mix = mix;
    const auto s = throughRack (p, 3.0);
    const auto b = band (s, 20.0, 120.0, static_cast<size_t> (sr));
    const double level = b.levelDb() - inputBassDb();
    WARN ("CHORUS mode " << mode << ", mix " << mix << ": bass corr " << b.corr() << ", side " << b.sideDb() << " dB, level " << level << " dB");
    CHECK (b.corr() > 0.99);
    CHECK (b.sideDb() < -26.0);
}

TEST_CASE ("Chorus, phaser and flanger at MIX 0 are the input itself", "[fx][bassphase]")
{
    // Only the wet is touched: at MIX 0 the unit is the input itself, sample for sample. (DIMENSION's dry keeps the
    // bass lift of that machine, so the chorus is checked in JUNO I.)
    const int unit = GENERATE (0, 1, 2);
    FxParams p;
    p.on[static_cast<size_t> (unit == 0 ? FxId::chorus : unit == 1 ? FxId::phaser : FxId::flanger)] = true;
    p.chorus.mix = p.phaser.mix = p.flanger.mix = 0.0;
    FxRack rack;
    rack.prepare (sr, 2);
    double worst = 0.0;
    for (size_t i = 0; i < static_cast<size_t> (sr); ++i)
    {
        const double t = static_cast<double> (i) / sr;
        double l = 0.4 * std::sin (2.0 * m::pi * 55.0 * t) + 0.2 * std::sin (2.0 * m::pi * 440.0 * t) + 0.1 * std::sin (2.0 * m::pi * 5000.0 * t);
        double r = l;
        const double x = l;
        rack.process (l, r, p, 120.0);
        if (i > 960) // the unit's 10 ms fade-in
            worst = std::max (worst, std::abs (l - x));
    }
    WARN ("unit " << unit << ": largest difference from the input " << worst);
    CHECK (worst < 1.0e-12);
}

TEST_CASE ("Delay, tape echo, reverbs and springs: the bass of the repeats and tails comes out in the middle", "[fx][bassphase]")
{
    const int unit = GENERATE (0, 1, 2, 3, 4, 5, 6, 7);
    FxParams p;
    const char* name = "";
    switch (unit)
    {
        case 0: p.on[static_cast<size_t> (FxId::delay)] = true; p.delay.pingPong = true; p.delay.feedback = 0.6; p.delay.mix = 0.6; name = "DELAY ping-pong"; break;
        case 1: p.on[static_cast<size_t> (FxId::echo)] = true; p.echo.width = 1.0; p.echo.mix = 0.6; p.echo.mode = 6; name = "TAPE ECHO"; break;
        case 2: p.on[static_cast<size_t> (FxId::echo)] = true; p.echoHeadsOff = true; p.echoSpring = 1.0; name = "TAPE ECHO spring"; break;
        case 3: p.on[static_cast<size_t> (FxId::reverb)] = true; p.reverb.type = ReverbType::plate; p.reverb.lowCutHz = 20.0; p.reverb.mix = 0.6; name = "PLATE"; break;
        case 4: p.on[static_cast<size_t> (FxId::reverb)] = true; p.reverb.type = ReverbType::room; p.reverb.lowCutHz = 20.0; p.reverb.mix = 0.6; name = "ROOM"; break;
        case 5: p.on[static_cast<size_t> (FxId::reverb)] = true; p.reverb.type = ReverbType::hall; p.reverb.lowCutHz = 20.0; p.reverb.width = 1.5; p.reverb.mix = 0.6; name = "HALL"; break;
        case 6: p.on[static_cast<size_t> (FxId::reverb)] = true; p.reverb.type = ReverbType::shimmer; p.reverb.lowCutHz = 20.0; p.reverb.mix = 0.6; name = "SHIMMER"; break;
        default: p.on[static_cast<size_t> (FxId::reverb)] = true; p.reverbSpring = true; p.reverb.mix = 0.6; name = "SPRING"; break;
    }
    const auto s = throughRack (p, 4.0);
    const auto b = band (s, 20.0, 120.0, static_cast<size_t> (sr));
    WARN (name << ": bass corr " << b.corr() << ", side " << b.sideDb() << " dB, balance " << b.balanceDb() << " dB");
    CHECK (b.corr() > 0.99);
    CHECK (b.sideDb() < -26.0);
    CHECK (std::abs (b.balanceDb()) < 0.5); // (the echo's heads panned hard: 2.6 dB before)
    // ... while the width stays above: the 1 kHz band is still stereo. (A steady tone through the ping-pong delay
    // comes out nearly mono anyway, every repeat on top of the others: "Ping-pong delay alternates sides" checks it.)
    if (unit == 1 || unit == 5)
        CHECK (band (s, 900.0, 1100.0, static_cast<size_t> (sr)).corr() < 0.98);
}

namespace
{
SynthParams bassPatch()
{
    SynthParams p;
    p.osc1Saw = true;
    p.osc2Saw = true;
    p.mixOsc1 = 0.8f;
    p.mixOsc2 = 0.6f;
    p.cutoffHz = 3000.0f;
    p.resonance = 0.1f;
    p.envAmount = 0.0f;
    p.aenvA = 0.003f;
    p.aenvS = 1.0f;
    p.levelDb = -12.0f;
    return p;
}

Stereo hold (SynthEngine& e, int note, double seconds)
{
    Stereo s;
    const auto n = static_cast<size_t> (seconds * sr);
    std::vector<float> l (n), r (n);
    e.noteOn (note, 0.8f);
    for (size_t pos = 0; pos < n; pos += 256)
        e.process (l.data() + pos, r.data() + pos, static_cast<int> (std::min<size_t> (256, n - pos)));
    e.noteOff (note);
    std::vector<float> tl (12000), tr (12000);
    e.process (tl.data(), tr.data(), 12000); // let the release go before the next note
    s.l.assign (l.begin(), l.end());
    s.r.assign (r.begin(), r.end());
    return s;
}
} // namespace

TEST_CASE ("POLY voices keep their pan at every frequency (each is a mono source panned in phase)", "[engine][bassphase]")
{
    auto p = bassPatch();
    p.voiceSpread = 0.5f; // no voice panned hard: the empty side would only show the noise floor
    SynthEngine e;
    e.prepare (sr, SynthEngine::defaultUnitSeed, 1);
    e.setParams (p);
    double worst = 0.0, widest = 0.0;
    for (int k = 0; k < 5; ++k) // each note on the next voice, each voice somewhere else in the field
    {
        const auto s = hold (e, 36, 1.0);
        const auto lowBand = band (s, 20.0, 100.0, 4800), midBand = band (s, 200.0, 600.0, 4800), highBand = band (s, 2000.0, 6000.0, 4800);
        worst = std::max ({ worst, std::abs (lowBand.balanceDb() - highBand.balanceDb()), std::abs (midBand.balanceDb() - highBand.balanceDb()) });
        widest = std::max (widest, std::abs (highBand.balanceDb()));
        CHECK (lowBand.corr() > 0.999);
    }
    WARN ("POLY, VOICE SPREAD 0.5: a voice's balance differs by " << worst << " dB at most between its bass, 200-600 Hz and 2-6 kHz; "
          "the voices spread up to " << widest << " dB");
    CHECK (worst < 0.3);
    CHECK (widest > 3.0);
}

TEST_CASE ("UNISON and DUO: the bass sits where PAN puts the instrument, the spread stays above", "[engine][bassphase]")
{
    const bool duo = GENERATE (false, true);
    const float pan = GENERATE (0.0f, -0.6f);
    auto p = bassPatch();
    if (duo)
        p.voiceMode = 1;
    else
    {
        p.unison = true;
        p.voiceCount = 5;
    }
    p.voiceSpread = 1.0f;
    p.voiceDetune = 0.6f;
    p.voicePan = pan;
    SynthEngine e;
    e.prepare (sr, SynthEngine::defaultUnitSeed, 1);
    e.setParams (p);
    const auto s = hold (e, 36, 3.0);
    const auto b = band (s, 20.0, 100.0, 4800);
    const double angle = (pan + 1.0) * 0.25 * m::pi; // a sound panned at PAN (constant power)
    const double expected = 20.0 * std::log10 (std::cos (angle) / std::sin (angle));
    WARN ((duo ? "DUO" : "UNISON x5") << ", VOICE SPREAD 1, PAN " << pan << ": bass corr " << b.corr() << ", balance " << b.balanceDb()
                                      << " dB (PAN puts it at " << expected << ")");
    CHECK (b.corr() > 0.99);
    if (pan == 0.0f)
        CHECK (b.sideDb() < -26.0); // panned on purpose otherwise: the balance says where it sits
    CHECK (std::abs (b.balanceDb() - expected) < 0.5);
    CHECK (band (s, 2000.0, 6000.0, 4800).corr() < 0.95); // the stack is still wide above
}

TEST_CASE ("Sub-oscillator: it switches exactly on OSC A's resets, note after note", "[engine][sub]")
{
    // OSC A saw + sub, filter wide open: every reset of A is one step down of the output; the sub flips on the same
    // sample, so the steps alternate saw + sub / saw - sub (one octave down) without a single extra or missing flip.
    const int octave = GENERATE (0, 1);
    SynthParams p;
    p.osc1Saw = true;
    p.mixOsc1 = 0.5f;
    p.mixOsc2 = 0.0f;
    p.mixSub = 0.25f; // both steps (saw + sub, saw - sub) stay clearly downwards
    p.subOctave = octave;
    p.cutoffHz = 20000.0f;
    p.resonance = 0.0f;
    p.envAmount = 0.0f;
    p.aenvA = 0.001f;
    p.aenvS = 1.0f;
    p.voiceSpread = 0.0f;
    p.levelDb = -6.0f;
    SynthEngine e;
    e.prepare (sr, SynthEngine::defaultUnitSeed, 1);
    e.setParams (p);
    for (int note = 0; note < 3; ++note)
    {
        const auto s = hold (e, 36, 2.0);
        std::vector<double> steps;
        for (size_t i = 4800; i + 3 < s.l.size(); ++i)
        {
            const double d = s.l[i + 2] - s.l[i - 2];
            if (d < -0.05 && d < s.l[i + 1] - s.l[i - 3] && d <= s.l[i + 3] - s.l[i - 1])
                steps.push_back (d);
        }
        REQUIRE (steps.size() > 100);
        // The step repeats every 2 resets one octave down (the sub flips on each), every 4 two octaves down (it flips
        // on every other one): any flip off a reset, extra or missing, breaks the pattern from there on.
        double lo = 0.0, hi = -1.0e9;
        for (const double d : steps)
        {
            lo = std::min (lo, d);
            hi = std::max (hi, d);
        }
        int breaks = 0;
        const size_t period = octave == 0 ? 2 : 4;
        for (size_t i = period; i < steps.size(); ++i)
            breaks += std::abs (steps[i] - steps[i - period]) > 0.2 * (hi - lo) ? 1 : 0;
        WARN ("sub -" << (octave + 1) << " oct, note " << note + 1 << ": " << steps.size() << " resets, steps " << hi << " / " << lo
                      << ", pattern breaks " << breaks);
        CHECK (hi - lo > 0.1);
        CHECK (breaks == 0);
    }
}

TEST_CASE ("Drive: no DC at the output, whatever the model and BIAS", "[fx][drive]")
{
    const auto model = GENERATE (DriveModel::tube, DriveModel::diode, DriveModel::tape, DriveModel::fold, DriveModel::crush);
    Drive d;
    d.prepare (sr);
    DriveParams p;
    p.model = model;
    p.driveDb = 30.0;
    p.bias = 0.8;
    p.mix = 1.0;
    double sum = 0.0, sq = 0.0;
    const auto n = static_cast<size_t> (3.0 * sr);
    for (size_t i = 0; i < n; ++i)
    {
        double l = 0.4 * std::sin (2.0 * m::pi * 110.0 * static_cast<double> (i) / sr), r = l;
        d.process (l, r, p);
        if (i >= n / 2)
        {
            sum += l;
            sq += l * l;
        }
    }
    const double mean = sum / static_cast<double> (n / 2), rms = std::sqrt (sq / static_cast<double> (n / 2));
    WARN ("model " << static_cast<int> (model) << ": DC " << mean << " (" << 20.0 * std::log10 (std::abs (mean) / rms + 1.0e-12) << " dB re the signal)");
    CHECK (std::abs (mean) < 1.0e-3 * rms);
}

TEST_CASE ("Drive CRUSH: MIX blends time-aligned signals (no comb)", "[fx][drive]")
{
    // At 16 bits and the full rate CRUSH is transparent, so 50 % of it with 50 % dry must be the input itself.
    const double hz = GENERATE (457.0, 1371.0, 3000.0);
    Drive d;
    d.prepare (sr);
    DriveParams p;
    p.model = DriveModel::crush;
    p.driveDb = 0.0;
    p.tone = 1.0;
    p.mix = 0.5;
    double in = 0.0, out = 0.0;
    for (size_t i = 0; i < static_cast<size_t> (sr); ++i)
    {
        double l = 0.3 * std::sin (2.0 * m::pi * hz * static_cast<double> (i) / sr), r = l;
        const double x = l;
        d.process (l, r, p);
        if (i > 4800)
        {
            in += x * x;
            out += l * l;
        }
    }
    const double db = 10.0 * std::log10 (out / in);
    WARN (hz << " Hz: " << db << " dB");
    CHECK (std::abs (db) < 0.3);
}

TEST_CASE ("Delay: MIX and FEEDBACK changes glide (no zipper)", "[fx][delay]")
{
    FxRack rack;
    rack.prepare (sr, 4);
    FxParams p;
    p.on[static_cast<size_t> (FxId::delay)] = true;
    p.delay.timeSeconds = 0.05;
    p.delay.feedback = 0.0;
    p.delay.mix = 0.0;
    double worstJump = 0.0, prev = 0.0;
    for (size_t i = 0; i < static_cast<size_t> (sr); ++i)
    {
        if (i == static_cast<size_t> (0.5 * sr))
            p.delay.mix = 1.0; // a jump, as from a host's automation step
        double l = 0.5, r = 0.5; // DC: the wet is a constant once the line is full, so any step is the zipper
        rack.process (l, r, p, 120.0);
        if (i > static_cast<size_t> (0.3 * sr))
            worstJump = std::max (worstJump, std::abs (l - prev));
        prev = l;
    }
    WARN ("largest sample-to-sample step after MIX 0 -> 1: " << worstJump);
    CHECK (worstJump < 0.01);
}

TEST_CASE ("Rack: a non-finite sample never lives on in the delay lines and tails", "[fx][rack]")
{
    FxRack rack;
    rack.prepare (sr, 6);
    FxParams p;
    p.on.fill (true);
    p.delay.feedback = 0.9;
    p.reverb.decayS = 20.0;
    bool finite = true;
    for (size_t i = 0; i < static_cast<size_t> (2.0 * sr); ++i)
    {
        double l = i == 1000 ? std::numeric_limits<double>::quiet_NaN() : 0.3 * std::sin (2.0 * m::pi * 220.0 * static_cast<double> (i) / sr);
        double r = i == 2000 ? std::numeric_limits<double>::infinity() : l;
        rack.process (l, r, p, 120.0);
        finite = finite && std::isfinite (l) && std::isfinite (r);
    }
    CHECK (finite);
}
