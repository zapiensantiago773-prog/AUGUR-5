#include "Rack/TapeEcho.h"

#include "Rack/RackFastMath.h"

#include <cmath>

namespace augur::rack
{

namespace
{
// Mode selector: which heads play (H1, H2, H3, H1+2, H2+3, H1+3, H1+2+3).
constexpr bool headMask[7][3] { { true, false, false }, { false, true, false }, { false, false, true }, { true, true, false },
                                { false, true, true },  { true, false, true },  { true, true, true } };
} // namespace

bool TapeEcho::headActive (int mode, int head) const noexcept
{
    return headMask[std::clamp (mode, 0, 6)][std::clamp (head, 0, 2)];
}

void TapeEcho::prepare (double sampleRate, std::uint64_t seed)
{
    sr = sampleRate;
    rng.seedWith (seed);
    tape.prepare (3.2 * sr); // head 3 at the longest speed (2.75 s) + wow
    speedCoeff = 1.0 - std::exp (-1.0 / (0.18 * sr)); // capstan motor inertia: speed changes glide (NV)
    fbHp.setCutoff (60.0, sr);
    mixS.prepare (sr, 0.02);
    intS.prepare (sr, 0.02);
    satS.prepare (sr, 0.05);
    widthS.prepare (sr, 0.05);
    reset();
}

void TapeEcho::reset() noexcept
{
    tape.reset();
    for (auto& h : headLoss)
        h.reset();
    fbHp.reset();
    fbSample = 0.0;
    wowPhase = flutPhase = wowRandom = wowTarget = 0.0;
    speedMs = -1.0; // the first block takes the knob's speed (no glide from a previous preset's tape speed)
}

void TapeEcho::process (double& left, double& right, const TapeEchoParams& p) noexcept
{
    const int mode = std::clamp (p.mode, 0, 6);
    const double targetMs = std::clamp (p.headMs, 25.0, 1000.0);
    speedMs = speedMs < 0.0 ? targetMs : speedMs + speedCoeff * (targetMs - speedMs);
    const double mix = mixS.next (std::clamp (p.mix, 0.0, 1.0));
    const double intensity = intS.next (std::clamp (p.intensity, 0.0, 1.0));
    const double sat = satS.next (std::clamp (p.saturation, 0.0, 1.0));
    const double width = widthS.next (std::clamp (p.width, 0.0, 1.0));
    const double age = std::clamp (p.age, 0.0, 1.0);

    // Wow: slow irregular speed drift (filtered random + 0.7 Hz); flutter: capstan ~8 Hz. Depth in % of delay.
    if (++wowCounter >= 64)
    {
        wowCounter = 0;
        wowTarget = rng.bipolar();
    }
    wowRandom += 0.0005 * (wowTarget - wowRandom);
    wowPhase += 0.7 / sr;
    flutPhase += 8.3 / sr;
    wowPhase -= std::floor (wowPhase);
    flutPhase -= std::floor (flutPhase);
    const double instab = 1.0 + 0.6 * age;
    const double wowDepth = p.wow * instab * 0.006;    // up to 0.6 % (NV)
    const double flutDepth = p.flutter * instab * 0.0015; // up to 0.15 % (NV)
    const double speedMod = 1.0 + wowDepth * (0.6 * std::sin (2.0 * pi * wowPhase) + 0.8 * wowRandom)
                            + flutDepth * std::sin (2.0 * pi * flutPhase);

    // Record head: the mono input plus the regenerated echoes, into the tape's soft saturation.
    const double input = 0.5 * (left + right);
    const double drive = 0.5 + 3.5 * sat;
    const double record = fast::tanh ((input + fbSample) * drive) / drive * (1.0 + 0.3 * sat);
    tape.write (record + age * 0.0008 * rng.gauss()); // hiss

    // Playback bandwidth follows the tape speed (faster = brighter), less with age (NV).
    const double bandwidth = std::clamp (11000.0 * std::sqrt (177.0 / speedMs) * (1.0 - 0.55 * age), 1500.0, 16000.0);
    double heads[3] {};
    double sum = 0.0;
    int active = 0;
    for (int h = 0; h < 3; ++h)
    {
        if (! headMask[mode][h])
            continue;
        headLoss[static_cast<size_t> (h)].setCutoff (bandwidth, sr);
        const double d = speedMs * headRatio[h] * speedMod * 0.001 * sr;
        heads[h] = headLoss[static_cast<size_t> (h)].lp (tape.read (d));
        sum += heads[h];
        ++active;
    }

    // Regeneration: all playing heads feed the record head (intensity), coupled and soft-limited so it can run
    // away into self-oscillation without blowing up. Normalised by the number of heads.
    const double loop = 1.15 * intensity / std::sqrt (std::max (1, active));
    fbSample = loop * fbHp.hp (sum);

    // Heads across the stereo field (H1 left, H2 centre, H3 right) by WIDTH.
    const double pan[3] { -width, 0.0, width };
    double wl = 0.0, wr = 0.0;
    for (int h = 0; h < 3; ++h)
    {
        const double a = 0.25 * pi * (1.0 + pan[h]);
        wl += heads[h] * std::cos (a) * 1.41421356;
        wr += heads[h] * std::sin (a) * 1.41421356;
    }
    bassL.set (false, 100.0, p.bassDb, sr);
    bassR.set (false, 100.0, p.bassDb, sr);
    trebleL.set (true, 4500.0, p.trebleDb, sr);
    trebleR.set (true, 4500.0, p.trebleDb, sr);
    wl = trebleL.process (bassL.process (wl));
    wr = trebleR.process (bassR.process (wr));

    const double gd = std::cos (0.5 * pi * mix), gw = std::sin (0.5 * pi * mix);
    left = left * gd + wl * gw;
    right = right * gd + wr * gw;
}

} // namespace augur::rack
