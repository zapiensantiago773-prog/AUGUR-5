#include "Rack/Drive.h"

#include "Rack/RackFastMath.h"

#include <cmath>
#include <vector>

namespace augur::rack
{

namespace
{
// ---------------- TUBE: 12AX7 common-cathode stage ----------------
// Koren's triode equations with his published 12AX7 constants (mu 100, Ex 1.4, Kg1 1060, Kp 600, Kvb 300),
// B+ 250 V, plate load 100 k. The load line Vp = B+ - Rp Ip(Vgk, Vp) is solved once per grid voltage at
// start-up into a table, so the audio path is a lookup. Grid conduction above 0 V softly limits the grid.
struct TriodeTable
{
    static constexpr double vMin = -20.0, vMax = 2.0, bPlus = 250.0, rPlate = 100000.0;
    static constexpr int size = 4096;
    std::vector<double> vp;

    static double plateCurrent (double vgk, double vpk)
    {
        constexpr double mu = 100.0, ex = 1.4, kg1 = 1060.0, kp = 600.0, kvb = 300.0;
        const double e1 = vpk / kp * std::log1p (std::exp (kp * (1.0 / mu + vgk / std::sqrt (kvb + vpk * vpk))));
        return e1 > 0.0 ? 2.0 * std::pow (e1, ex) / kg1 : 0.0;
    }

    TriodeTable()
    {
        vp.resize (size);
        double v = bPlus * 0.6;
        for (int i = 0; i < size; ++i)
        {
            const double vgk = vMin + (vMax - vMin) * i / (size - 1);
            // Newton on f(Vp) = Vp - B+ + Rp Ip(Vgk, Vp) (monotone increasing).
            for (int it = 0; it < 60; ++it)
            {
                const double f = v - bPlus + rPlate * plateCurrent (vgk, v);
                const double h = 1.0e-3;
                const double df = 1.0 + rPlate * (plateCurrent (vgk, v + h) - plateCurrent (vgk, v - h)) / (2.0 * h);
                const double step = f / df;
                v = std::clamp (v - step, 1.0, bPlus);
                if (std::abs (step) < 1.0e-9)
                    break;
            }
            vp[static_cast<size_t> (i)] = v;
        }
    }

    double operator() (double vgk) const noexcept
    {
        if (vgk > 0.0)
            vgk = 0.35 * fast::tanh (vgk / 0.35); // grid current through the grid stopper
        const double pos = std::clamp ((vgk - vMin) / (vMax - vMin) * (size - 1), 0.0, static_cast<double> (size - 1) - 1.0e-9);
        const int i = static_cast<int> (pos);
        const double f = pos - i;
        return vp[static_cast<size_t> (i)] + f * (vp[static_cast<size_t> (i + 1)] - vp[static_cast<size_t> (i)]);
    }
};

const TriodeTable& triode()
{
    static const TriodeTable t;
    return t;
}

// ---------------- TAPE: Jiles-Atherton hysteresis (ferric oxide, Chowdhury DAFx 2019) ----------------
constexpr double jaMs = 3.5e5, jaA = 2.2e4, jaAlpha = 1.6e-3, jaK = 2.7e4, jaC = 1.7e-1;

double langevin (double x) noexcept
{
    if (std::abs (x) < 1.0e-4)
        return x / 3.0;
    return 1.0 / std::tanh (x) - 1.0 / x;
}

double langevinD (double x) noexcept
{
    if (std::abs (x) < 1.0e-4)
        return 1.0 / 3.0;
    const double ct = 1.0 / std::tanh (x);
    return 1.0 / (x * x) - ct * ct + 1.0;
}

// dM/dt for the field H moving at dH/dt (eq. 18 of the paper).
double jaDerivative (double m, double h, double dh) noexcept
{
    const double q = (h + jaAlpha * m) / jaA;
    const double man = jaMs * langevin (q);
    const double lPrime = langevinD (q);
    const double s = dh >= 0.0 ? 1.0 : -1.0;
    const double diff = man - m;
    const double deltaM = (s * diff) > 0.0 ? 1.0 : 0.0;
    const double denom = (1.0 - jaC) * s * jaK - jaAlpha * diff;
    const double irr = std::abs (denom) > 1.0e-9 ? (1.0 - jaC) * deltaM * diff / denom : 0.0;
    const double num = irr * dh + jaC * (jaMs / jaA) * dh * lPrime;
    return num / (1.0 - jaC * jaAlpha * (jaMs / jaA) * lPrime);
}
} // namespace

void Drive::prepare (double sampleRate)
{
    sr = sampleRate;
    osRate = sr * 4.0;
    triode(); // build the table outside the audio thread
    for (int c = 0; c < 2; ++c)
    {
        // host <-> 2x must stop the band just above 20 kHz (transition ~0.056 fs): 95 taps, 85 dB.
        // 2x <-> 4x only has to stop images beyond 76 kHz: 23 taps, 90 dB.
        upA[static_cast<size_t> (c)].design (95, 85.0);
        downA[static_cast<size_t> (c)].design (95, 85.0);
        upB[static_cast<size_t> (c)].design (23, 90.0);
        downB[static_cast<size_t> (c)].design (23, 90.0);
        dryDelay[static_cast<size_t> (c)].prepare (256);
        preHp[static_cast<size_t> (c)].setCutoff (15.0, osRate);
        diodeHp[static_cast<size_t> (c)].setCutoff (720.0, osRate); // TS: 4.7 k + 47 nF in the feedback leg
    }
    // Latency of up(A) + down(A) (2x rate) and up(B) + down(B) (4x rate), in host samples.
    dryLatency = upA[0].latency() + upB[0].latency() / 2.0;
    gainIn.prepare (sr, 0.02);
    gainOut.prepare (sr, 0.02);
    mixS.prepare (sr, 0.02);
    toneS.prepare (sr, 0.02);
    biasS.prepare (sr, 0.05);
    // Small-signal gain of the tube stage at the default bias, to normalise its output.
    const double q = triode() (-1.5);
    tubeNorm = 1.0 / std::max (1.0, (triode() (-1.5 - 0.01) - q) / 0.01);
    reset();
}

void Drive::reset() noexcept
{
    for (int c = 0; c < 2; ++c)
    {
        upA[static_cast<size_t> (c)].reset();
        upB[static_cast<size_t> (c)].reset();
        downA[static_cast<size_t> (c)].reset();
        downB[static_cast<size_t> (c)].reset();
        dryDelay[static_cast<size_t> (c)].reset();
        preHp[static_cast<size_t> (c)].reset();
        toneLp[static_cast<size_t> (c)].reset();
        diodeHp[static_cast<size_t> (c)].reset();
        tapeM[static_cast<size_t> (c)] = tapeH[static_cast<size_t> (c)] = 0.0;
        crushHold[static_cast<size_t> (c)] = crushPhase[static_cast<size_t> (c)] = 0.0;
    }
}

double Drive::tube (double x, double bias) const noexcept
{
    const double vk = 1.5 - 0.9 * bias;       // cathode bias: BIAS moves the operating point (0.6 .. 2.4 V)
    const double vq = triode() (-vk);
    return (vq - triode() (x - vk)) * tubeNorm; // inverting stage, re-inverted; DC removed later
}

// Clipping stage of the Tube Screamer: DRIVE is Rf, so the gain (1 + Rf/Ri) applies above 720 Hz only (series
// RC to ground) while the lows pass at unity; two diodes across Rf limit the feedback voltage:
// V + Is Rf (e^{V/nVt} - a e^{-V/nVt}) = Vlin, solved by Newton. BIAS unbalances the diodes (even harmonics).
double Drive::diode (double x, double bias, double gain, int ch) noexcept
{
    const double hp = gain * diodeHp[static_cast<size_t> (ch)].hp (x);
    constexpr double nVt = 0.045, isRf = 2.0e-6;
    const double ratioNeg = std::exp (-1.5 * bias); // diode mismatch
    const double vlin = hp;
    double v = std::clamp (vlin, -1.0, 1.0);
    for (int it = 0; it < 12; ++it)
    {
        const double ep = std::exp (std::clamp (v / nVt, -60.0, 60.0)), en = std::exp (std::clamp (-v / nVt, -60.0, 60.0));
        const double f = v + isRf * (ep - ratioNeg * en) - vlin;
        const double df = 1.0 + isRf * (ep + ratioNeg * en) / nVt;
        const double step = f / df;
        v -= std::clamp (step, -0.2, 0.2);
        if (std::abs (step) < 1.0e-10)
            break;
    }
    return (x - hp / gain) + v; // the low band passes at unity, the clipped high band rides on it
}

// Tape. A correctly biased recorder lays the magnetisation on the anhysteretic curve Ms L(H/a) (soft, smooth
// saturation); an under-biased one follows the hysteresis loop (crossover "deadzone", grit). BIAS blends the
// two: +1 = ideal bias, -1 = no bias. The loop is the Jiles-Atherton model, RK4 at the 4x rate.
double Drive::tape (double x, int ch) noexcept
{
    const auto c = static_cast<size_t> (ch);
    const double h1 = x * 3.0 * jaA;
    const double h0 = tapeH[c];
    const double dh = h1 - h0; // per sample
    double m = tapeM[c];
    const double k1 = jaDerivative (m, h0, dh);
    const double k2 = jaDerivative (m + 0.5 * k1, h0 + 0.5 * dh, dh);
    const double k3 = jaDerivative (m + 0.5 * k2, h0 + 0.5 * dh, dh);
    const double k4 = jaDerivative (m + k3, h1, dh);
    m += (k1 + 2.0 * k2 + 2.0 * k3 + k4) / 6.0;
    if (! std::isfinite (m))
        m = 0.0;
    m = std::clamp (m, -jaMs, jaMs);
    tapeM[c] = m;
    tapeH[c] = h1;
    // H = 3a x: the anhysteretic small-signal slope Ms / (3a) makes quiet signals pass at unity (M / Ms ~ x).
    const double anhysteretic = langevin (h1 / jaA);
    const double w = 0.5 + 0.5 * biasS.v;
    return w * anhysteretic + (1.0 - w) * m / jaMs;
}

// Smooth parallel-fold shaper (Serge / Buchla-lineage behaviour: each extra fold adds a new set of partials).
// BIAS offsets the signal before folding (even harmonics, "timbre symmetry").
double Drive::fold (double x, double bias) const noexcept
{
    return std::sin (0.5 * pi * (x + 0.5 * bias)) - std::sin (0.25 * pi * bias);
}

double Drive::shape (double x, int ch, const DriveParams& p) noexcept
{
    switch (p.model)
    {
        case DriveModel::tube: return tube (x, biasS.v);
        case DriveModel::diode: return diode (x, biasS.v, stageGain, ch);
        case DriveModel::tape: return tape (x, ch);
        case DriveModel::fold: return fold (x, biasS.v);
        case DriveModel::crush: return x;
    }
    return x;
}

void Drive::process (double& left, double& right, const DriveParams& p) noexcept
{
    const double driveGain = gainIn.next (dbToGain (std::clamp (p.driveDb, 0.0, 48.0)));
    // Make-up: a saturating stage's level rises 1:1 with drive and then flattens near its ceiling, so the
    // inverse of that curve, -10 log10(g^2 (1 + k^2) / (1 + (k g)^2)), keeps the level of a nominal line
    // signal steady while the colour changes. k per model from the measured curves (tests/test_fx.cpp).
    double ceiling = 0.0;
    switch (p.model)
    {
        case DriveModel::tube: ceiling = 0.1225; break;
        case DriveModel::diode: ceiling = 0.35; break;
        case DriveModel::tape: ceiling = 0.139; break;
        case DriveModel::fold:
        case DriveModel::crush: ceiling = 0.0; break;
    }
    double compDb = 0.0;
    if (ceiling > 0.0)
    {
        const double g = dbToGain (std::clamp (p.driveDb, 0.0, 48.0));
        const double c2 = ceiling * ceiling;
        compDb = -10.0 * std::log10 (g * g * (1.0 + c2) / (1.0 + c2 * g * g));
    }
    stageGain = driveGain;
    const double outGain = gainOut.next (dbToGain (p.outputDb + compDb));
    const double mix = mixS.next (std::clamp (p.mix, 0.0, 1.0));
    const double tone = toneS.next (std::clamp (p.tone, 0.0, 1.0));
    biasS.next (std::clamp (p.bias, -1.0, 1.0));

    if (p.model != lastModel)
    {
        lastModel = p.model;
        reset(); // a different circuit: start from its rest state
    }
    // Tone stage corner, once per host sample (both channels).
    double toneHz = 3000.0 * std::pow (6.0, tone);            // TUBE / TAPE: plate & head losses, 3 .. 18 kHz
    if (p.model == DriveModel::diode)
        toneHz = 600.0 * std::pow (16.0, tone);                // TS tone low-pass, 0.6 .. 9.6 kHz
    const bool toned = p.model == DriveModel::tube || p.model == DriveModel::tape || p.model == DriveModel::diode;
    for (auto& t : toneLp)
        t.setCutoff (toneHz, osRate);

    double io[2] { left, right };
    for (int c = 0; c < 2; ++c)
    {
        const auto uc = static_cast<size_t> (c);
        dryDelay[uc].write (io[c]);
        const double dry = dryDelay[uc].read (static_cast<double> (dryLatency));
        double wet;
        if (p.model == DriveModel::crush)
        {
            // Sample-rate reduction (hold) + bit depth: DRIVE 0..48 dB -> 16 .. 2 bits, TONE -> 1 kHz .. full rate.
            const double bits = 16.0 - 14.0 * std::clamp (p.driveDb / 48.0, 0.0, 1.0);
            const double targetRate = 1000.0 * std::pow (sr / 1000.0, tone);
            crushPhase[uc] += targetRate / sr;
            if (crushPhase[uc] >= 1.0)
            {
                crushPhase[uc] -= std::floor (crushPhase[uc]);
                const double levels = std::exp2 (bits - 1.0);
                crushHold[uc] = std::round (io[c] * levels) / levels;
            }
            wet = crushHold[uc] * dbToGain (p.outputDb);
            io[c] = dry + mix * (wet - dry);
            continue;
        }
        double a1, a2;
        upA[uc].up (p.model == DriveModel::diode ? io[c] : io[c] * driveGain, a1, a2); // DIODE: gain is inside
        double halfRate[2] { a1, a2 };
        double outHalf[2];
        for (int k = 0; k < 2; ++k)
        {
            double b1, b2;
            upB[uc].up (halfRate[k], b1, b2);
            double quad[2] { b1, b2 };
            for (auto& q : quad)
            {
                q = shape (preHp[uc].hp (q), c, p);
                if (toned)
                    q = toneLp[uc].lp (q);
            }
            outHalf[k] = downB[uc].down (quad[0], quad[1]);
        }
        wet = downA[uc].down (outHalf[0], outHalf[1]) * outGain;
        io[c] = dry + mix * (wet - dry);
    }
    left = io[0];
    right = io[1];
}

} // namespace augur::rack
