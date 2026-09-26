#include "Voice/SynthVoice.h"

#include "Util/FastMath.h"
#include "Util/Random.h"

#include <algorithm>
#include <cmath>

namespace augur
{

namespace
{
// Nominal chip behaviour before per-unit tolerances. Provisional values, to be fitted to measurements.
struct VcoModelConstants
{
    float curvature;
    float deadTime;    // seconds
    float triPeak;
    float sawLevel, triLevel, pulseLevel;
    float jitter;
};

constexpr VcoModelConstants cem3340 { 0.035f, 1.6e-6f, 0.5f, 1.0f, 0.98f, 0.92f, 1.5e-5f };
constexpr VcoModelConstants ssm2030 { 0.11f, 3.0e-6f, 0.47f, 1.0f, 0.93f, 0.95f, 4.0e-5f };

constexpr float voiceOutputScale = 1.0f; // ~-12 dBFS per note at LEVEL -6 dB
constexpr float lowFreqDivider = 1.0f / 128.0f; // OSC 2 LO FREQ: seven octaves down
constexpr float envCutoffRange = 7.0f;          // octaves at ENV AMT = 1
constexpr float polyModPitchRange = 48.0f;      // semitones at full poly-mod
constexpr float polyModCutoffRange = 5.0f;      // octaves
constexpr float pwModRange = 0.45f;

float mapMatrixAmount (ModDest d, float a) noexcept
{
    switch (d)
    {
        case ModDest::Osc1Freq:
        case ModDest::Osc2Freq:     return (a < 0.0f ? -1.0f : 1.0f) * a * a * 24.0f; // fine control near 0
        case ModDest::Osc1Pw:
        case ModDest::Osc2Pw:       return a * pwModRange;
        case ModDest::FilterCutoff: return a * 6.0f;
        case ModDest::Resonance:
        case ModDest::AmpLevel:     return a;
        case ModDest::LfoRate:
        case ModDest::count:        break;
    }
    return 0.0f;
}
} // namespace

void SynthVoice::prepare (double internalRate, double controlRate, std::uint64_t seed) noexcept
{
    sampleRate = internalRate;
    fp = VoiceFingerprint::generate (deriveSeed (seed, 1));
    vcoA.prepare (internalRate, deriveSeed (seed, 2));
    vcoB.prepare (internalRate, deriveSeed (seed, 3));
    noise.seed (deriveSeed (seed, 4));
    driftA.prepare (controlRate, deriveSeed (seed, 5));
    driftB.prepare (controlRate, deriveSeed (seed, 6));
    driftF.prepare (controlRate, deriveSeed (seed, 7));
    filterEnv.prepare (internalRate);
    ampEnv.prepare (internalRate);
    filter.prepare (internalRate);
    reset();
}

void SynthVoice::reset() noexcept
{
    filterEnv.reset();
    ampEnv.reset();
    filter.reset();
    note = -1;
    held = false;
    hasPitch = false;
    lastB = 0.0f;
    polyPressure = 0.0f;
}

void SynthVoice::noteOn (const NoteOn& n) noexcept
{
    note = n.note;
    velocity = n.velocity;
    held = true;
    order = n.order;
    unisonOffset = n.unisonOffset;
    panPosition = n.panPosition;
    polyPressure = 0.0f;

    pitchTarget = static_cast<float> (n.note);
    if (! n.glide || ! hasPitch)
        pitch = pitchTarget;
    hasPitch = true;

    if (n.retrigger || ! ampEnv.isActive())
    {
        filterEnv.noteOn();
        ampEnv.noteOn();
        lfoDelayGain = lfoDelayInc >= 1.0f ? 1.0f : 0.0f;
    }
}

void SynthVoice::noteOff() noexcept
{
    held = false;
    filterEnv.noteOff();
    ampEnv.noteOff();
}

void SynthVoice::updateControl (const ChunkSignals& sig) noexcept
{
    const SynthParams& p = *sig.params;
    const float age = std::clamp (p.analogAge, 0.0f, 1.0f);
    const float spread = 0.25f + 0.75f * age; // how far this unit is from nominal

    // Drift always advances, even when silent, so behaviour does not depend on voice usage.
    const float driftSemis = (0.0015f + 0.035f * age); // ~0.15 .. 3.6 cents std-dev
    const float dA = driftA.next() * driftSemis;
    const float dB = driftB.next() * driftSemis;
    const float dF = driftF.next() * (0.004f + 0.03f * age); // octaves

    // VCO characters
    const auto& m = p.oscModel == 0 ? cem3340 : ssm2030;
    for (int i = 0; i < 2; ++i)
    {
        VcoCharacter c;
        c.curvature = m.curvature + fp.curvature[i] * 0.012f * spread;
        c.deadTimeSeconds = m.deadTime * std::max (0.3f, 1.0f + 0.15f * fp.deadTime[i] * spread);
        c.hfTrim = 1.0f + 0.012f * fp.hfTrim[i] * spread - 0.35f * age * (1.0f + 0.3f * fp.hfTrim[i]);
        c.triPeak = m.triPeak + fp.triPeak * 0.008f * spread;
        c.sawLevel = m.sawLevel * (1.0f + 0.02f * fp.waveLevel[0] * spread);
        c.triLevel = m.triLevel * (1.0f + 0.02f * fp.waveLevel[1] * spread);
        c.pulseLevel = m.pulseLevel * (1.0f + 0.02f * fp.waveLevel[2] * spread);
        c.pwOffset = fp.pwOffset[i] * 0.01f * spread;
        c.dcOffset = fp.dc[i] * 0.008f * spread;
        c.cycleJitter = m.jitter * (0.3f + age);
        (i == 0 ? vcoA : vcoB).setCharacter (c);
    }

    // Tuning: fingerprint spread follows VOICE DETUNE, drift and scale error follow ANALOG AGE.
    const float tuneSpread = p.voiceDetune * 0.07f; // semitones std-dev at DETUNE = 1
    tuneA = static_cast<float> (p.osc1Semi) + p.osc1Fine * 0.01f + fp.oscTune[0] * tuneSpread + dA;
    tuneB = static_cast<float> (p.osc2Semi) + p.osc2Fine * 0.01f + fp.oscTune[1] * tuneSpread + dB;
    scaleA = fp.oscScale[0] * 0.0012f * spread;
    scaleB = fp.oscScale[1] * 0.0012f * spread;

    cutoffOffset = fp.cutoff * 0.04f * spread + dF;
    resonanceTrim = 1.0f + fp.resonance * 0.03f * spread;
    vcaTrim = 1.0f + fp.vcaGain * 0.03f * spread;
    bleed = 0.0005f + 0.004f * age;

    filter.setModel (p.filterModel == 0 ? LadderFilter::Model::Cem3320 : LadderFilter::Model::Ssm2040);

    const float envSpread = 0.05f * spread;
    filterEnv.setParameters (p.fenvA * (1.0f + envSpread * fp.envTime[0]), p.fenvD * (1.0f + envSpread * fp.envTime[0]),
                             p.fenvS, p.fenvR * (1.0f + envSpread * fp.envTime[0]));
    ampEnv.setParameters (p.aenvA * (1.0f + envSpread * fp.envTime[1]), p.aenvD * (1.0f + envSpread * fp.envTime[1]),
                          p.aenvS, p.aenvR * (1.0f + envSpread * fp.envTime[1]));

    // Glide: RC portamento, GLIDE = time to cover ~95% of the interval.
    glideCoeff = p.glide <= 0.0005f ? 1.0f
                                    : static_cast<float> (1.0 - std::exp (-3.0 / (static_cast<double> (p.glide) * sampleRate)));
    keytrack = p.keytrack == 0 ? 0.0f : (p.keytrack == 1 ? 0.5f : 1.0f);
    lfoDelayInc = p.lfoDelay <= 0.001f ? 1.0f : static_cast<float> (1.0 / (static_cast<double> (p.lfoDelay) * sampleRate));

    // Constant-power pan: global pan + per-voice position * spread + a hint of unit variation.
    const float pan = std::clamp (p.voicePan + panPosition * p.voiceSpread + fp.pan * 0.02f, -1.0f, 1.0f);
    const float angle = (pan + 1.0f) * 0.25f * fastmath::pi;
    panL = panLTarget; // previous chunk's end point
    panR = panRTarget;
    panLTarget = std::cos (angle);
    panRTarget = std::sin (angle);

    numSlots = 0;
    for (const auto& s : p.matrix)
    {
        if (s.amount == 0.0f || s.dest == ModDest::LfoRate)
            continue;
        slots[static_cast<size_t> (numSlots++)] = { static_cast<int> (s.source), static_cast<int> (s.dest),
                                                    mapMatrixAmount (s.dest, s.amount) };
    }
}

void SynthVoice::render (const ChunkSignals& sig, int start, int count, float* left, float* right) noexcept
{
    const SynthParams& p = *sig.params;
    const float velGain = 1.0f - p.ampVelocity + p.ampVelocity * velocity;
    const float filterVelOct = p.filterVelocity * velocity * 3.0f;
    const float pressure = std::max (sig.channelPressure, polyPressure);
    const float atOct = p.aftertouchAmount * pressure * 3.0f;
    const bool sync = p.osc1Sync;
    const bool pmOn = p.pmOn;
    const float kbdPitchB = p.osc2Kbd ? 1.0f : 0.0f;
    const float invChunk = 1.0f / static_cast<float> (std::max (1, sig.numSamples));

    for (int j = 0; j < count; ++j)
    {
        const size_t i = static_cast<size_t> (start + j);

        pitch += (pitchTarget - pitch) * glideCoeff;
        const float fenv = filterEnv.next();
        const float aenv = ampEnv.next();
        lfoDelayGain = std::min (1.0f, lfoDelayGain + lfoDelayInc);
        const float lfo = sig.lfo[i] * lfoDelayGain;
        const float nz = noise.analog();

        // Mod matrix
        float dst[static_cast<size_t> (ModDest::count)] {};
        if (numSlots > 0)
        {
            const float src[static_cast<size_t> (ModSource::count)] { fenv, aenv, lastB, lfo, sig.modWheel, velocity, pressure, nz };
            for (int s = 0; s < numSlots; ++s)
                dst[slots[static_cast<size_t> (s)].dest] += src[slots[static_cast<size_t> (s)].source] * slots[static_cast<size_t> (s)].amount;
        }

        // Poly-Mod: filter envelope and OSC B (audio rate) into OSC A pitch / PW and the filter.
        const float pm = pmOn ? fenv * sig.pmFilterEnv[i] + lastB * sig.pmOsc2[i] : 0.0f;

        const float notePitch = pitch + unisonOffset;
        const float common = sig.bendSemitones + sig.tuneSemitones;

        const float keyB = kbdPitchB * (notePitch - 60.0f);
        const float pitchB = 60.0f + keyB * (1.0f + scaleB) + tuneB + common + dst[static_cast<size_t> (ModDest::Osc2Freq)];
        float freqB = fastmath::semitonesToHz (pitchB);
        if (p.osc2LoFreq)
            freqB *= lowFreqDivider;

        const float pitchA = 60.0f + (notePitch - 60.0f) * (1.0f + scaleA) + tuneA + common + dst[static_cast<size_t> (ModDest::Osc1Freq)]
                             + (p.pmFreqA ? pm * polyModPitchRange : 0.0f);
        const float freqA = fastmath::semitonesToHz (pitchA);

        const float pwB = std::clamp (sig.osc2Pw[i] + dst[static_cast<size_t> (ModDest::Osc2Pw)], 0.05f, 0.95f);
        const float pwA = std::clamp (sig.osc1Pw[i] + dst[static_cast<size_t> (ModDest::Osc1Pw)] + (p.pmPwA ? pm * pwModRange : 0.0f), 0.05f, 0.95f);

        const float b = vcoB.process (freqB, pwB, sig.saw2[i], sig.tri2[i], sig.pulse2[i], -1.0f);
        const float a = vcoA.process (freqA, pwA, sig.saw1[i], 0.0f, sig.pulse1[i], sync ? vcoB.lastResetD() : -1.0f);
        lastB = b;

        // Mixer: summing into the filter; DRIVE pushes the input stage like a hot mixer does.
        const float mix = a * sig.mix1[i] + b * sig.mix2[i] + nz * sig.mixNoise[i] * 1.5f + (a + b) * bleed;
        const float driveGain = 0.55f + 3.5f * sig.drive[i];
        const float bias = 0.1f * sig.drive[i]; // slight asymmetry -> even harmonics when driven
        const float x = (fastmath::tanh (mix * driveGain + bias) - fastmath::tanh (bias)) * (1.0f + sig.drive[i]);

        const float cutoffOct = sig.cutoffOct[i] + cutoffOffset + sig.envAmount[i] * fenv * envCutoffRange
                                + keytrack * (notePitch - 60.0f) * (1.0f / 12.0f) + filterVelOct + atOct
                                + dst[static_cast<size_t> (ModDest::FilterCutoff)] + (p.pmFilter ? pm * polyModCutoffRange : 0.0f);
        const float fc = fastmath::exp2 (std::min (cutoffOct, 15.0f));
        const float res = (sig.resonance[i] + dst[static_cast<size_t> (ModDest::Resonance)]) * resonanceTrim;
        const float y = filter.process (x, fc, res);

        // VCA with a little OTA colour.
        const float gain = aenv * velGain * std::max (0.0f, 1.0f + dst[static_cast<size_t> (ModDest::AmpLevel)]) * vcaTrim;
        const float out = fastmath::tanh (y * gain * 0.6f) * (voiceOutputScale / 0.6f);

        // Pan interpolates across the whole chunk by absolute position, independent of sub-ranges.
        const float t = static_cast<float> (i + 1) * invChunk;
        left[j] += out * (panL + (panLTarget - panL) * t);
        right[j] += out * (panR + (panRTarget - panR) * t);
    }
}

} // namespace augur
