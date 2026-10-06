#include "Rack/BusComp.h"

#include <cmath>

namespace augur::rack
{

void BusComp::prepare (double sampleRate)
{
    sr = sampleRate;
    makeupS.prepare (sr, 0.02);
    mixS.prepare (sr, 0.02);
    reset();
}

void BusComp::reset() noexcept
{
    for (auto& h : scHp)
        h.reset();
    envDb = autoFast = autoSlow = 0.0;
    lastGain = 1.0;
}

void BusComp::process (double& left, double& right, const BusCompParams& p) noexcept
{
    const double ratio = ratios[std::clamp (p.ratioIndex, 0, 6)];
    const double attack = attacksMs[std::clamp (p.attackIndex, 0, 5)] * 0.001;
    const double release = releasesS[std::clamp (p.releaseIndex, 0, 4)];
    const double thr = std::clamp (p.thresholdDb, -60.0, 0.0);
    constexpr double kneeDb = 4.0;

    // Detector input: the output of the VCA (feedback), side-chain high-passed, stereo-linked (max).
    for (auto& h : scHp)
        h.setCutoff (std::clamp (p.scHpfHz, 10.0, 500.0), sr);
    const double dl = scHp[0].hp (left * lastGain), dr = scHp[1].hp (right * lastGain);
    const double level = std::max (std::abs (dl), std::abs (dr));
    const double levelDb = 20.0 * std::log10 (std::max (level, 1.0e-6));

    // Static curve with a soft knee. The detector sees the compressed output, so the reduction that holds the
    // output at threshold + over/ratio is over_out * (ratio - 1): in steady state this lands exactly on the
    // feed-forward curve (L_in - thr)(ratio - 1)/ratio, but the loop reacts to its own result (the "glue").
    double over = levelDb - thr;
    if (over < -0.5 * kneeDb)
        over = 0.0;
    else if (over < 0.5 * kneeDb)
        over = (over + 0.5 * kneeDb) * (over + 0.5 * kneeDb) / (2.0 * kneeDb);
    const double r = std::min (ratio, 20.0); // LIMIT behaves as 20:1 inside the loop
    const double targetGr = over * (r - 1.0);

    // The loop GR_{n+1} = GR_n + a (T(GR_n) - GR_n) has slope 1 - a r: stable for a r < 2. Real feedback
    // compressors share this limit (very fast attack at very high ratio is not possible); keep a r <= 0.9.
    const double aCoeff = std::min (1.0 - std::exp (-1.0 / (attack * sr)), 0.9 / r);
    const double rFast = 1.0 - std::exp (-1.0 / ((release > 0.0 ? release : 0.1) * sr));
    autoFast += (targetGr > autoFast ? aCoeff : rFast) * (targetGr - autoFast);
    if (release > 0.0)
        envDb = autoFast;
    else
    {
        // AUTO: a second, slow-charging (0.5 s) and slow-releasing (1.2 s) envelope holds sustained compression;
        // short peaks only reach the fast one and recover in 0.1 s (program-dependent release).
        const double sA = 1.0 - std::exp (-1.0 / (0.5 * sr)), sR = 1.0 - std::exp (-1.0 / (1.2 * sr));
        autoSlow += (targetGr > autoSlow ? sA : sR) * (targetGr - autoSlow);
        envDb = std::max (autoFast, autoSlow);
    }
    envDb = std::clamp (envDb, 0.0, 40.0);
    const double gain = std::pow (10.0, -envDb / 20.0);
    lastGain = gain;
    grMeter.store (static_cast<float> (envDb), std::memory_order_relaxed);

    const double makeup = makeupS.next (dbToGain (std::clamp (p.makeupDb, 0.0, 24.0)));
    const double mix = mixS.next (std::clamp (p.mix, 0.0, 1.0));
    const double wl = left * gain * makeup, wr = right * gain * makeup;
    left = left + mix * (wl - left);
    right = right + mix * (wr - right);
}

} // namespace augur::rack
