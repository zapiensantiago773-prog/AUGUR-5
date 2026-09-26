#pragma once

#include "Util/HalfbandFir.h"
#include "Util/Simd4.h"

namespace augur
{

// Fuzz pedal in the classic "sustainer" layout of the famous 4-transistor wall-of-sound boxes:
// input stage -> two cascaded clipping stages (diode pairs in the feedback loop, each with its own
// coupling high-pass and feedback-capacitor low-pass) -> passive tone stack (a low-pass and a
// high-pass arm blended by TONE, which scoops the mids around 1 kHz) -> recovery stage.
//
// Runs on the oversampled voice bus (before decimation) and oversamples 8x more inside (768 kHz at a
// 48 kHz host) with half-band stages sized per rate (27/19/19 taps). The clippers are diode-like
// algebraic curves x / sqrt(1 + x^2) with first-order antiderivative anti-aliasing (ADAA); measured
// fold-back is in the test suite. Left and right run together in one SIMD register.
class Fuzz
{
public:
    void prepare (double sampleRate) noexcept;
    void reset() noexcept;

    // sustain 0..1 (gain into the clippers), tone 0..1 (dark .. bright), volume 0..1, mix 0..1.
    void process (float* left, float* right, int numSamples, float sustain, float tone, float volume, float mix) noexcept;

private:
    using F4 = simd::F4;

    struct OnePole
    {
        F4 s {};
        F4 lp (F4 x, F4 g) noexcept
        {
            const F4 v = (x - s) * g;
            const F4 y = v + s;
            s = y + v;
            return y;
        }
        F4 hp (F4 x, F4 g) noexcept { return x - lp (x, g); }
    };

    struct Clipper
    {
        F4 previous {};
        F4 process (F4 x) noexcept;
    };

    F4 core (F4 x, F4 gain1, F4 tone, F4 out) noexcept;
    F4 g (double hz) const noexcept; // TPT one-pole coefficient G = g / (1 + g)

    double sampleRate = 768000.0;
    F4 gInHp {}, gHp1 {}, gLp1 {}, gHp2 {}, gLp2 {}, gToneLp {}, gToneHp {}, gOutHp {};

    OnePole inHp, hp1, lp1, hp2, lp2, toneLp, toneHp, outHp;
    Clipper clip1, clip2;
    HalfbandUpsampler<27, F4> up1;
    HalfbandUpsampler<19, F4> up2, up3;
    HalfbandDownsampler<27, F4> down1;
    HalfbandDownsampler<19, F4> down2, down3;

    float smSustain = -1.0f, smTone = 0.5f, smVolume = 0.5f, smMix = 1.0f, smoothCoeff = 0.001f;
};

} // namespace augur
