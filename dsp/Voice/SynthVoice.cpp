#include "Voice/SynthVoice.h"

#include "Mixer/OtaMixer.h"
#include "Util/FastMath.h"
#include "Util/Random.h"

#include <algorithm>
#include <cmath>

namespace augur
{

namespace
{
constexpr float voiceOutputScale = 1.0f;       // ~-12 dBFS per note at LEVEL -6 dB
constexpr float loFreqSemitones = -90.0f;      // OSC B LO FREQ: -7.5 V on the 1 V/oct sum (service manual 2-4)
constexpr float envCutoffRange = 7.0f;         // octaves at ENV AMT = 1
constexpr float polyModPitchRange = 48.0f;     // semitones per unit of poly-mod CV
constexpr float polyModCutoffRange = 5.0f;     // octaves per unit of poly-mod CV
constexpr float pwModRange = 0.45f;
constexpr double shRefreshSeconds = 0.006;     // DAC/S&H loop (service manual 2-12)
constexpr float shDroopSemitones = 0.5e-3f / 0.0833f; // 0.5 mV droop per refresh on 83.3 mV/semitone

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
    for (size_t i = 0; i < 2; ++i)
    {
        cem[i].prepare (internalRate, deriveSeed (seed, 2 + i));
        ssm[i].prepare (internalRate, deriveSeed (seed, 12 + i));
        Random r (deriveSeed (seed, 22 + i));
        droopPhase[i] = r.nextFloat(); // each S/H is refreshed at its own point of the 6 ms loop
    }
    noise.seed (deriveSeed (seed, 4));
    driftA.prepare (controlRate, deriveSeed (seed, 5));
    driftB.prepare (controlRate, deriveSeed (seed, 6));
    driftF.prepare (controlRate, deriveSeed (seed, 7));
    filterEnv.prepare (internalRate);
    ampEnv.prepare (internalRate);
    filter.prepare (internalRate);

    droopInc = 1.0 / (shRefreshSeconds * internalRate);
    dcTrimCoeff = static_cast<float> (1.0 - std::exp (-1.0 / (1.0 * internalRate))); // tau = 1 s
    dcTrimA = dcTrimB = filterDc = 0.0f;
    // CV noise at the exponential converter's base (the Prophet does not band-limit it; datasheet p.4)
    cvNoiseCoeff = static_cast<float> (1.0 - std::exp (-2.0 * 3.14159265358979 * 5000.0 / internalRate));

    model = -1;
    tunedAgeBucket = -1;
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
    lastOscB = 0.0f;
    polyPressure = 0.0f;
    cvNoise.fill (0.0f);
}

void SynthVoice::configureUnits (int newModel, float age) noexcept
{
    const float spread = 0.35f + 0.65f * age; // distance of this unit from nominal

    for (size_t i = 0; i < 2; ++i)
    {
        const auto& d = fp.osc[i];

        Cem3340Unit c;
        c.expo.tuneCents = d[0] * 25.0f;                          // untuned error: removed by autotune
        c.expo.scaleError = d[1] * 0.0005f * (1.0f + 3.0f * age); // 0.05 % trimmed typical (datasheet)
        c.expo.bulkCentsAt10k = 3.0f * (1.0f + 0.3f * d[2]);
        c.asymmetry = d[3] * (0.006f + 0.02f * age);              // datasheet: symmetry 45..55 %
        c.comparatorDelay = 100e-9f * (1.0f + 0.2f * d[4]);
        c.converterGain = 1.0f + d[5] * 0.002f * (1.0f + 2.0f * age);
        c.converterOffset = d[6] * 0.0015f * (1.0f + age);        // +-15 mV on 10 V
        c.syncHold = 4.4e-6f * (1.0f + 0.15f * d[7]);
        c.pulseFallDelay = 0.5e-6f * (1.0f + 0.2f * d[8]);
        c.pwOffset = d[9] * 0.004f * (1.0f + age);
        c.sawLevel = 1.0f + d[10] * 0.015f;                       // 9.4..10.6 V
        c.triLevel = 1.0f + d[11] * 0.01f;                        // 4.85..5.15 V
        c.pulseLevel = 1.0f + d[12] * 0.015f;
        cem[i].setUnit (c);

        Ssm2030Unit s;
        s.expo.tuneCents = d[0] * 25.0f;
        s.expo.scaleError = d[1] * 0.001f * (1.0f + 3.0f * age);
        s.expo.bulkCentsAt10k = 4.0f * (1.0f + 0.3f * d[2]);
        s.curvature = 0.08f + d[3] * 0.02f * spread;
        s.deadTimeSeconds = 3.0e-6f * std::max (0.3f, 1.0f + 0.15f * d[4]);
        s.triPeak = 0.47f + d[5] * 0.01f * spread;
        s.pwOffset = d[9] * 0.01f * spread;
        s.cycleJitter = 4.0e-5f * (0.3f + age);
        s.sawLevel = 1.0f + d[10] * 0.02f;
        s.triLevel = 1.0f + d[11] * 0.02f;
        s.pulseLevel = 1.0f + d[12] * 0.02f;
        ssm[i].setUnit (s);

        droopDepth[i] = shDroopSemitones * (0.5f + 0.5f * std::abs (d[13])) * (0.6f + 0.8f * age);

        if (newModel == 0)
            tuning[i].tune (cem[i]);
        else
            tuning[i].tune (ssm[i]);
    }

    // CV noise: ~0.12 cents rms (fresh) .. 0.18 (worn)
    const float sigma = 0.0012f * (0.5f + age);
    const float lowpassRms = 0.57735f * std::sqrt (cvNoiseCoeff / (2.0f - cvNoiseCoeff));
    cvNoiseGain = sigma / lowpassRms;

    model = newModel;
}

double SynthVoice::staticFrequency (int osc, double p) const noexcept
{
    return model == 1 ? ssm[static_cast<size_t> (osc)].staticFrequency (p) : cem[static_cast<size_t> (osc)].staticFrequency (p);
}

float SynthVoice::dacOffset (int osc, float keyPitch, float knob) const noexcept
{
    // OSC S/H CV = KBD + OSC FREQ knob + autotune bias, through the 14-bit DAC.
    const float command = keyPitch + knob;
    const float bias = tuning[static_cast<size_t> (osc)].biasFor (command);
    return quantiseOscCv (command + bias) - keyPitch;
}

void SynthVoice::updateCvOffsets() noexcept
{
    const float keyB = kbdB ? pitchTarget : 60.0f;
    cvOffset[0] = dacOffset (0, pitchTarget, knobSemis[0]);
    cvOffset[1] = dacOffset (1, keyB, knobSemis[1]) + loFreqOffset;
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
    if (model >= 0)
        updateCvOffsets(); // the computer writes the new note's CV (with its own bias) immediately

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
    const float spread = 0.25f + 0.75f * age;

    // Units and their autotune are rebuilt only when the model or the age bucket changes.
    const int newModel = std::clamp (p.oscModel, 0, 1);
    const int ageBucket = static_cast<int> (std::lround (age * 50.0f));
    if (newModel != model || ageBucket != tunedAgeBucket)
    {
        tunedAgeBucket = ageBucket;
        configureUnits (newModel, age);
    }

    // Drift since the last TUNE always advances, even when silent.
    const float driftSemis = 0.0015f + 0.035f * age; // ~0.15 .. 3.6 cents std-dev
    const float dA = driftA.next() * driftSemis;
    const float dB = driftB.next() * driftSemis;
    const float dF = driftF.next() * (0.004f + 0.03f * age); // octaves

    // Analog (unquantised) pitch terms: drift, deliberate per-voice detune, FINE.
    const float detune = p.voiceDetune * 0.07f;
    analogTune[0] = dA + fp.oscDetune[0] * detune + p.osc1Fine * 0.01f;
    analogTune[1] = dB + fp.oscDetune[1] * detune + p.osc2Fine * 0.01f;

    // Digital part of the CV: KBD + FREQ knob + autotune bias on the 14-bit grid.
    knobSemis[0] = static_cast<float> (p.osc1Semi);
    knobSemis[1] = static_cast<float> (p.osc2Semi) * (p.osc2LoFreq ? 2.0f : 1.0f); // INIT FREQ range doubles
    loFreqOffset = p.osc2LoFreq ? loFreqSemitones : 0.0f;
    kbdB = p.osc2Kbd;
    updateCvOffsets();

    // Mixer AC coupling: exact mean of the differential-pair output for the selected waves, including
    // this unit's output levels and PW comparator offset.
    const auto last = static_cast<size_t> (std::max (0, sig.numSamples - 1));
    const auto unitDc = [&] (size_t o, float gS, float gT, float gP, float w) {
        float ls = 1.0f, lt = 1.0f, lp = 1.0f, pwo = 0.0f;
        if (model == 1)
        {
            const auto& u = ssm[o].getUnit();
            ls = u.sawLevel; lt = u.triLevel; lp = u.pulseLevel; pwo = u.pwOffset;
        }
        else
        {
            const auto& u = cem[o].getUnit();
            ls = u.sawLevel; lt = u.triLevel; lp = u.pulseLevel; pwo = u.pwOffset;
        }
        return ota::dc (gS * ls, gT * lt, gP * lp, w + pwo);
    };
    dcA = unitDc (0, sig.saw1[last], 0.0f, sig.pulse1[last], sig.osc1Pw[last]);
    dcB = unitDc (1, sig.saw2[last], sig.tri2[last], sig.pulse2[last], sig.osc2Pw[last]);

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
    const bool rev3 = model == 0;
    const float common = sig.bendSemitones + sig.tuneSemitones; // analog: master tune, pitch wheel
    const float invChunk = 1.0f / static_cast<float> (std::max (1, sig.numSamples));

    for (int j = 0; j < count; ++j)
    {
        const size_t i = static_cast<size_t> (start + j);

        pitch += (pitchTarget - pitch) * glideCoeff;
        const float fenv = filterEnv.next();
        const float aenv = ampEnv.next();
        lfoDelayGain = std::min (1.0f, lfoDelayGain + lfoDelayInc);
        const float lfo = sig.lfo[i] * lfoDelayGain;
        const float white = noise.white();
        pinkState += (white - pinkState) * 0.05f;
        const float pink = pinkState * 4.0f;

        // CV path extras: S/H droop (saw-shaped, refreshed every 6 ms) and wide-band CV noise.
        float cvExtra[2];
        for (size_t o = 0; o < 2; ++o)
        {
            droopPhase[o] += droopInc;
            if (droopPhase[o] >= 1.0)
                droopPhase[o] -= 1.0;
            cvNoise[o] += (noise.white() * cvNoiseGain - cvNoise[o]) * cvNoiseCoeff;
            cvExtra[o] = cvNoise[o] - droopDepth[o] * static_cast<float> (droopPhase[o]);
        }

        // Mod matrix
        float dst[static_cast<size_t> (ModDest::count)] {};
        if (numSlots > 0)
        {
            const float src[static_cast<size_t> (ModSource::count)] { fenv, aenv, lastOscB, lfo, sig.modWheel, velocity, pressure, pink };
            for (int s = 0; s < numSlots; ++s)
                dst[slots[static_cast<size_t> (s)].dest] += src[slots[static_cast<size_t> (s)].source] * slots[static_cast<size_t> (s)].amount;
        }

        const float notePitch = pitch + unisonOffset;

        // OSC B first: it is the sync master and the poly-mod source.
        const float pitchB = (kbdB ? notePitch : 60.0f) + cvOffset[1] + analogTune[1] + common + cvExtra[1]
                             + dst[static_cast<size_t> (ModDest::Osc2Freq)];
        const float pwB = std::clamp (sig.osc2Pw[i] + dst[static_cast<size_t> (ModDest::Osc2Pw)], 0.0f, 1.0f);
        const VcoOutputs b = rev3 ? cem[1].process (pitchB, pwB, -1.0f) : ssm[1].process (pitchB, pwB, -1.0f);
        const float resetB = rev3 ? cem[1].lastResetD() : ssm[1].lastResetD();

        // OSC B through the mixer / poly-mod differential pairs (same waveform switches feed both).
        const float mB = ota::mix (b.saw, b.tri, b.pulse, sig.saw2[i], sig.tri2[i], sig.pulse2[i]);
        lastOscB = mB - dcB - dcTrimB;
        dcTrimB += (lastOscB) * dcTrimCoeff;

        // Poly-Mod: filter envelope and OSC B (with its DC, as on the hardware) -> FREQ A / PW A / filter.
        const float pm = pmOn ? fenv * sig.pmFilterEnv[i] + mB * sig.pmOsc2[i] : 0.0f;

        const float pitchA = notePitch + cvOffset[0] + analogTune[0] + common + cvExtra[0]
                             + dst[static_cast<size_t> (ModDest::Osc1Freq)] + (p.pmFreqA ? pm * polyModPitchRange : 0.0f);
        const float pwA = std::clamp (sig.osc1Pw[i] + dst[static_cast<size_t> (ModDest::Osc1Pw)] + (p.pmPwA ? pm * pwModRange : 0.0f), 0.0f, 1.0f);
        const float syncD = sync ? resetB : -1.0f;
        const VcoOutputs a = rev3 ? cem[0].process (pitchA, pwA, syncD) : ssm[0].process (pitchA, pwA, syncD);
        const float mA = ota::mix (a.saw, 0.0f, a.pulse, sig.saw1[i], 0.0f, sig.pulse1[i]) - dcA - dcTrimA;
        dcTrimA += mA * dcTrimCoeff;

        // Mixer bus (AC-coupled) + noise + OTA feed-through, then the DRIVE stage into the filter.
        const float bus = mA * sig.mix1[i] + lastOscB * sig.mix2[i] + white * sig.mixNoise[i] * 0.8f + (mA + lastOscB) * bleed;
        const float pre = bus * (0.9f + 3.0f * sig.drive[i]);
        const float x = fastmath::tanh (pre * 0.5f) * 2.0f;

        const float cutoffOct = sig.cutoffOct[i] + cutoffOffset + sig.envAmount[i] * fenv * envCutoffRange
                                + keytrack * (notePitch - 60.0f) * (1.0f / 12.0f) + filterVelOct + atOct
                                + dst[static_cast<size_t> (ModDest::FilterCutoff)] + (p.pmFilter ? pm * polyModCutoffRange : 0.0f);
        const float fc = fastmath::exp2 (std::min (cutoffOct, 15.0f));
        const float res = (sig.resonance[i] + dst[static_cast<size_t> (ModDest::Resonance)]) * resonanceTrim;
        // AC coupling into the VCA (C4165): removes the DC the filter's own saturation produces.
        const float yRaw = filter.process (x, fc, res);
        const float y = yRaw - filterDc;
        filterDc += y * dcTrimCoeff;

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
