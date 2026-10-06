#include "Rack/Reverb.h"

#include "Rack/RackFastMath.h"

#include <cmath>

namespace augur::rack
{

namespace
{
// Dattorro's plate, lengths in samples at his 29761 Hz design rate.
constexpr double dattorroRate = 29761.0;
constexpr double inDiffLen[4] { 142.0, 107.0, 379.0, 277.0 };
constexpr double inDiffGain[4] { 0.75, 0.75, 0.625, 0.625 };
constexpr double apL = 672.0, del1L = 4453.0, ap2L = 1800.0, del2L = 3720.0;
constexpr double apR = 908.0, del1R = 4217.0, ap2R = 2656.0, del2R = 3163.0;
constexpr double excursion = 16.0;

// FDN line lengths (ms) for the hall; the room scales them down. Chosen mutually incommensurate.
constexpr double hallMs[8] { 41.0, 47.3, 53.9, 60.2, 67.1, 74.9, 82.7, 91.3 };
constexpr double earlyMs[6] { 7.3, 11.9, 17.1, 23.5, 29.1, 37.7 };
constexpr double earlyGain[6] { 0.72, -0.61, 0.53, -0.44, 0.37, -0.3 };
} // namespace

void Reverb::prepare (double sampleRate, std::uint64_t seed)
{
    sr = sampleRate;
    rng.seedWith (seed);
    const double maxScale = sr / dattorroRate * 2.05; // size up to 2
    preL.prepare (0.55 * sr);
    preR.prepare (0.55 * sr);
    for (int i = 0; i < 4; ++i)
        inDiff[static_cast<size_t> (i)].line.prepare (inDiffLen[i] * maxScale + 4);
    tankApL.line.prepare ((apL + excursion * 2.0) * maxScale + 8);
    tankApR.line.prepare ((apR + excursion * 2.0) * maxScale + 8);
    tankAp2L.line.prepare (ap2L * maxScale + 4);
    tankAp2R.line.prepare (ap2R * maxScale + 4);
    tankDel1L.prepare (del1L * maxScale + 4);
    tankDel2L.prepare (del2L * maxScale + 4);
    tankDel1R.prepare (del1R * maxScale + 4);
    tankDel2R.prepare (del2R * maxScale + 4);
    for (auto& l : fdnLine)
        l.prepare (0.0925 * 2.05 * sr + 64);
    for (int i = 0; i < 4; ++i)
        fdnDiff[static_cast<size_t> (i)].line.prepare (0.02 * sr);
    early.prepare (0.05 * sr);
    shiftLine.prepare (0.12 * sr);
    shimmerLp.setCutoff (7000.0, sr);
    shimmerHp.setCutoff (250.0, sr);
    mixS.prepare (sr, 0.02);
    widthS.prepare (sr, 0.05);
    sizeS.prepare (sr, 0.3);
    sizeS.snap (1.0);
    for (int i = 0; i < lines; ++i)
        fdnPhase[static_cast<size_t> (i)] = rng.uniform();
    reset();
}

void Reverb::reset() noexcept
{
    preL.reset();
    preR.reset();
    for (auto& a : inDiff)
        a.line.reset();
    for (auto* a : { &tankApL, &tankApR, &tankAp2L, &tankAp2R })
        a->line.reset();
    for (auto* d : { &tankDel1L, &tankDel2L, &tankDel1R, &tankDel2R })
        d->reset();
    for (auto& l : fdnLine)
        l.reset();
    for (auto& a : fdnDiff)
        a.line.reset();
    early.reset();
    shiftLine.reset();
    fdnState.fill (0.0);
    fdnLp.fill (0.0);
    bwState = dampL = dampR = tankFbL = tankFbR = shimmerFb = 0.0;
    lfoPhase = shiftPhase = 0.0;
    lowCutL.reset();
    lowCutR.reset();
}

// Pitch shifter for the shimmer: two read heads sweeping a 60 ms window in opposite phase, Hann-faded, so the
// read speed (and pitch) is `ratio` with no clicks at the wrap.
double Reverb::pitchShift (double x, double semis) noexcept
{
    shiftLine.write (x);
    const double ratio = std::exp2 (semis / 12.0);
    const double window = 0.06 * sr;
    shiftPhase += (ratio - 1.0) / window;
    shiftPhase -= std::floor (shiftPhase);
    double y = 0.0;
    for (int k = 0; k < 2; ++k)
    {
        double ph = shiftPhase + 0.5 * k;
        ph -= std::floor (ph);
        const double gain = std::sin (pi * ph);
        y += gain * gain * shiftLine.read (window * (1.0 - ph) + 2.0);
    }
    return y;
}

void Reverb::plate (double inL, double inR, double& outL, double& outR, const ReverbParams& p, double decay, double dampCoef) noexcept
{
    // Input: mono sum through the bandwidth low-pass and four input diffusers.
    bwState += 0.9995 * (0.5 * (inL + inR) - bwState);
    double x = bwState;
    for (int i = 0; i < 4; ++i)
        x = inDiff[static_cast<size_t> (i)].process (x, inDiffLen[i] * scale, inDiffGain[i]);

    lfoPhase += 1.0 / sr;
    lfoPhase -= std::floor (lfoPhase);
    const double exc = excursion * scale * (0.25 + 1.75 * p.modulation);
    const double modL = exc * std::sin (2.0 * pi * lfoPhase), modR = exc * std::cos (2.0 * pi * lfoPhase * 1.13);

    // Left half of the figure-eight.
    double l = x + decay * tankFbR;
    l = tankApL.process (l, apL * scale + exc + modL, -0.70);
    tankDel1L.write (l);
    l = tankDel1L.read (del1L * scale);
    dampL += (1.0 - dampCoef) * (l - dampL);
    l = tankAp2L.process (dampL * decay, ap2L * scale, 0.50);
    tankDel2L.write (l);
    const double newFbL = tankDel2L.read (del2L * scale);

    // Right half.
    double r = x + decay * tankFbL;
    r = tankApR.process (r, apR * scale + exc + modR, -0.70);
    tankDel1R.write (r);
    r = tankDel1R.read (del1R * scale);
    dampR += (1.0 - dampCoef) * (r - dampR);
    r = tankAp2R.process (dampR * decay, ap2R * scale, 0.50);
    tankDel2R.write (r);
    const double newFbR = tankDel2R.read (del2R * scale);
    tankFbL = newFbL;
    tankFbR = newFbR;

    // Output taps (Dattorro's table, scaled).
    const auto t = [this] (const DelayLine& d, double n) { return d.read (std::max (1.0, n * scale)); };
    outL = 0.6 * (t (tankDel1R, 266) + t (tankDel1R, 2974) - t (tankAp2R.line, 1913) + t (tankDel2R, 1996) - t (tankDel1L, 1990)
                  - t (tankAp2L.line, 187) - t (tankDel2L, 1066));
    outR = 0.6 * (t (tankDel1L, 353) + t (tankDel1L, 3627) - t (tankAp2L.line, 1228) + t (tankDel2L, 2673) - t (tankDel1R, 2111)
                  - t (tankAp2R.line, 335) - t (tankDel2R, 121));
}

void Reverb::fdn (double inL, double inR, double& outL, double& outR, const ReverbParams& p, bool room) noexcept
{
    const double sizeMul = sizeS.v * (room ? 0.38 : 1.0);
    // Early reflections (room and hall).
    early.write (0.5 * (inL + inR));
    double erL = 0.0, erR = 0.0;
    for (int i = 0; i < 6; ++i)
    {
        const double v = earlyGain[i] * early.read (earlyMs[i] * 0.001 * sr * (room ? 0.7 : 1.3) * sizeS.v);
        (i % 2 == 0 ? erL : erR) += v;
    }

    // Input diffusion: two all-passes per side, different lengths (decorrelated left / right).
    const double dm = room ? 0.6 : 1.0;
    double xL = fdnDiff[0].process (inL, 3.1 * dm * 0.001 * sr, 0.6);
    xL = fdnDiff[1].process (xL, 5.3 * dm * 0.001 * sr, 0.6);
    double xR = fdnDiff[2].process (inR, 3.7 * dm * 0.001 * sr, 0.6);
    xR = fdnDiff[3].process (xR, 4.9 * dm * 0.001 * sr, 0.6);

    // Absorption per line (Jot): g_i sets the low RT60, the one-pole b_i tilts it so the highs die
    // DAMPING faster (alpha = RT60(pi) / RT60(0)).
    const double rt = p.freeze ? 1.0e9 : std::clamp (p.decayS, 0.1, 60.0) * (room ? 0.6 : 1.0);
    const double alpha = 1.0 - 0.9 * std::clamp (p.damping, 0.0, 1.0);
    std::array<double, lines> s {};
    double sum = 0.0;
    for (int i = 0; i < lines; ++i)
    {
        const auto ui = static_cast<size_t> (i);
        fdnPhase[ui] += (0.21 + 0.07 * i) / sr;
        fdnPhase[ui] -= std::floor (fdnPhase[ui]);
        const double lenS = hallMs[i] * 0.001 * sizeMul;
        const double mod = (0.0002 + 0.0008 * p.modulation) * sr * std::sin (2.0 * pi * fdnPhase[ui]);
        const double read = fdnLine[ui].read (lenS * sr + mod + 4.0);
        const double g = std::pow (10.0, -3.0 * lenS / rt);
        const double b = p.freeze ? 0.0 : std::clamp (std::log (10.0) / 4.0 * std::log10 (g) * (1.0 - 1.0 / (alpha * alpha)), 0.0, 0.95);
        fdnLp[ui] = g * (1.0 - b) * read + b * fdnLp[ui];
        s[ui] = fdnLp[ui];
        sum += s[ui];
    }
    // Householder feedback: v = s - (2/N) sum(s). Inputs: left on even lines, right on odd ones.
    const double h = 2.0 / lines * sum;
    const double inGain = p.freeze ? 0.0 : 0.35;
    double oL = 0.0, oR = 0.0;
    for (int i = 0; i < lines; ++i)
    {
        const auto ui = static_cast<size_t> (i);
        const double inj = (i % 2 == 0 ? xL : xR) * inGain + (p.type == ReverbType::shimmer ? shimmerFb * 0.35 : 0.0);
        fdnLine[ui].write (s[ui] - h + inj);
        const double sign = (i & 2) ? -1.0 : 1.0;
        (i % 2 == 0 ? oL : oR) += sign * s[ui];
    }
    outL = 0.5 * oL + 0.35 * erL;
    outR = 0.5 * oR + 0.35 * erR;

    if (p.type == ReverbType::shimmer)
    {
        // Pitch-shift the tail and feed it back: each pass climbs another interval (the Eno / Lanois shimmer).
        const double shifted = pitchShift (0.5 * (outL + outR), p.pitchSemis);
        shimmerFb = std::clamp (p.shimmer, 0.0, 1.0) * shimmerHp.hp (shimmerLp.lp (fast::tanh (shifted)));
    }
}

void Reverb::process (double& left, double& right, const ReverbParams& p) noexcept
{
    if (p.type != lastType)
    {
        lastType = p.type;
        reset();
    }
    const double mix = mixS.next (std::clamp (p.mix, 0.0, 1.0));
    const double width = widthS.next (std::clamp (p.width, 0.0, 1.5));
    sizeS.next (std::clamp (p.size, 0.3, 2.0));
    scale = sr / dattorroRate * sizeS.v;

    preL.write (left);
    preR.write (right);
    const double pd = std::clamp (p.predelayMs, 0.0, 500.0) * 0.001 * sr;
    const double inL = p.freeze && p.type == ReverbType::plate ? 0.0 : preL.read (std::max (1.0, pd));
    const double inR = p.freeze && p.type == ReverbType::plate ? 0.0 : preR.read (std::max (1.0, pd));

    double wL = 0.0, wR = 0.0;
    if (p.type == ReverbType::plate)
    {
        // Decay per half loop from the RT60: two decay multiplies per half (after the damping, at the cross).
        const double halfLoop = (apL + del1L + ap2L + del2L) / dattorroRate * sizeS.v;
        const double decay = p.freeze ? 1.0 : std::min (0.9995, std::pow (10.0, -1.5 * halfLoop / std::clamp (p.decayS, 0.1, 60.0)));
        const double dampCoef = p.freeze ? 0.0 : 0.0005 + 0.85 * std::clamp (p.damping, 0.0, 1.0);
        plate (inL, inR, wL, wR, p, decay, dampCoef);
    }
    else
        fdn (inL, inR, wL, wR, p, p.type == ReverbType::room);

    lowCutL.setCutoff (std::clamp (p.lowCutHz, 20.0, 1000.0), sr);
    lowCutR.setCutoff (std::clamp (p.lowCutHz, 20.0, 1000.0), sr);
    wL = lowCutL.hp (wL);
    wR = lowCutR.hp (wR);
    const double mid = 0.5 * (wL + wR), side = 0.5 * (wL - wR) * width;
    wL = mid + side;
    wR = mid - side;

    const double gd = std::cos (0.5 * pi * mix), gw = std::sin (0.5 * pi * mix);
    left = left * gd + wL * gw;
    right = right * gd + wR * gw;
}

} // namespace augur::rack
