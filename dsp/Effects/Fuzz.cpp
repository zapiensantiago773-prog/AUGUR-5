#include "Effects/Fuzz.h"

#include <algorithm>
#include <cmath>

namespace augur
{

using simd::F4;
using simd::splat;

// Diode-pair clipper curve x / sqrt(1 + x^2): soft knee and a slow approach to the rails, like two
// silicon diodes in an inverting stage's feedback. Its antiderivative is sqrt(1 + x^2).
F4 Fuzz::Clipper::process (F4 x) noexcept
{
    const F4 one = splat (1.0f);
    // First-order ADAA: the average of the curve over the segment between the previous and current input.
    const F4 d = x - previous;
    const F4 adaa = (simd::sqrt (one + x * x) - simd::sqrt (one + previous * previous)) / (d + splat (1.0e-20f));
    const F4 m = splat (0.5f) * (x + previous);
    const F4 direct = m / simd::sqrt (one + m * m);
    previous = x;
    return simd::selectGreater (simd::abs (d), splat (1.0e-3f), adaa, direct);
}

F4 Fuzz::g (double hz) const noexcept
{
    const double w = std::tan (3.14159265358979 * std::min (hz, 0.45 * sampleRate) / sampleRate);
    return splat (static_cast<float> (w / (1.0 + w)));
}

void Fuzz::prepare (double busSampleRate) noexcept
{
    sampleRate = busSampleRate * 8.0; // the core runs at 8x the bus rate
    gInHp = g (25.0);     // input coupling
    gHp1 = g (120.0);     // stage 1 coupling (tightens the lows before the first clip)
    gLp1 = g (5500.0);    // stage 1 feedback capacitor
    gHp2 = g (80.0);      // stage 2 coupling
    gLp2 = g (3800.0);    // stage 2 feedback capacitor
    gToneLp = g (410.0);  // tone stack low-pass arm
    gToneHp = g (1850.0); // tone stack high-pass arm
    gOutHp = g (20.0);    // output coupling
    smoothCoeff = static_cast<float> (1.0 - std::exp (-1.0 / (0.02 * busSampleRate)));
    reset();
}

void Fuzz::reset() noexcept
{
    for (OnePole* f : { &inHp, &hp1, &lp1, &hp2, &lp2, &toneLp, &toneHp, &outHp })
        *f = OnePole {};
    clip1 = Clipper {};
    clip2 = Clipper {};
    up1.reset();
    up2.reset();
    up3.reset();
    down1.reset();
    down2.reset();
    down3.reset();
    smSustain = -1.0f;
}

F4 Fuzz::core (F4 x, F4 gain1, F4 tone, F4 out) noexcept
{
    F4 s = inHp.hp (x, gInHp);

    // Clipping stage 1: gain, feedback-capacitor low-pass, diodes.
    s = lp1.lp (hp1.hp (s, gHp1) * gain1, gLp1);
    s = clip1.process (s);

    // Clipping stage 2: fixed high gain into the second diode pair: the "sustain" wall.
    s = lp2.lp (hp2.hp (s, gHp2) * splat (12.0f), gLp2);
    s = clip2.process (s);

    // Passive tone stack: blend of a low-pass and a high-pass arm (mid scoop at the centre).
    const F4 lo = toneLp.lp (s, gToneLp);
    const F4 hi = toneHp.hp (s, gToneHp);
    s = lo * (splat (1.0f) - tone) + hi * tone * splat (1.6f);

    return outHp.hp (s, gOutHp) * out;
}

void Fuzz::process (float* l, float* r, int numSamples, float sustain, float tone, float volume, float mix) noexcept
{
    if (smSustain < 0.0f)
    {
        smSustain = sustain;
        smTone = tone;
        smVolume = volume;
        smMix = mix;
    }
    for (int n = 0; n < numSamples; ++n)
    {
        smSustain += (sustain - smSustain) * smoothCoeff;
        smTone += (tone - smTone) * smoothCoeff;
        smVolume += (volume - smVolume) * smoothCoeff;
        smMix += (mix - smMix) * smoothCoeff;

        // SUSTAIN: +6 .. +46 dB into the first stage. VOLUME: up to about the dry level of a full chord.
        const F4 gain1 = splat (2.0f * std::exp2 (smSustain * 6.6f));
        const F4 out = splat (0.7f * smVolume * smVolume);
        const F4 toneV = splat (smTone);
        const float wet = std::clamp (smMix, 0.0f, 1.0f);

        // Left and right in lanes 0 and 1: 8x up, core, 8x down.
        const F4 x = simd::set (l[n], r[n], 0.0f, 0.0f);
        F4 a[2], mid[2];
        up1.process (x, a[0], a[1]);
        for (int i = 0; i < 2; ++i)
        {
            F4 b[2], inner[2];
            up2.process (a[i], b[0], b[1]);
            for (int j = 0; j < 2; ++j)
            {
                F4 c0, c1;
                up3.process (b[j], c0, c1);
                const F4 y0 = core (c0, gain1, toneV, out);
                const F4 y1 = core (c1, gain1, toneV, out);
                inner[j] = down3.process (y0, y1);
            }
            mid[i] = down2.process (inner[0], inner[1]);
        }
        const F4 y = down1.process (mid[0], mid[1]);

        l[n] = l[n] * (1.0f - wet) + simd::get (y, 0) * wet;
        r[n] = r[n] * (1.0f - wet) + simd::get (y, 1) * wet;
    }
}

} // namespace augur
