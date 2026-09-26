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
        case ModDest::CrossMod:
        case ModDest::RingLevel:
        case ModDest::SubLevel:
        case ModDest::Drive:
        case ModDest::Osc1Level:
        case ModDest::Osc2Level:
        case ModDest::NoiseLevel:   return a;
        case ModDest::Lfo2Rate:     return a * 4.0f; // octaves
        case ModDest::LfoRate:
        case ModDest::count:        break;
    }
    return 0.0f;
}
} // namespace

float SynthVoice::outputScale() noexcept
{
    return voiceOutputScale;
}

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
    modEnv.prepare (internalRate);
    lfo2.prepare (internalRate, deriveSeed (seed, 30));
    noteRng.setSeed (deriveSeed (seed, 31));
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
    modEnv.reset();
    filter.reset();
    subRing.reset();
    subState = 1.0f;
    subCount = 0;
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
    if (constantsDirty && lastSig != nullptr)
        computeConstants (*lastSig); // the voice wakes up: bring its chunk constants up to date
    else if (model >= 0)
        updateCvOffsets(); // the computer writes the new note's CV (with its own bias) immediately

    noteRandom = noteRng.nextBipolar();
    if (n.retrigger || ! ampEnv.isActive())
    {
        filterEnv.noteOn();
        ampEnv.noteOn();
        modEnv.noteOn();
        lfoDelayGain = lfoDelayInc >= 1.0f ? 1.0f : 0.0f;
        const bool retrig = lastSig == nullptr || lastSig->params == nullptr || lastSig->params->lfo2Retrig;
        lfo2.restart (retrig ? 0.0 : n.lfo2Phase);
    }
}

void SynthVoice::warmUp (const ChunkSignals& sig, int chunks) noexcept
{
    NoteOn on;
    on.note = 60;
    noteOn (on);

    const float normalCoeff = dcTrimCoeff;
    dcTrimCoeff = 0.0f; // corrections are set from measured means, not tracked
    dcTrimA = dcTrimB = filterDc = 0.0f;

    float l[ChunkSignals::maxSamples], r[ChunkSignals::maxSamples];
    const auto run = [&] (int numChunks, int phase) {
        warmPhase = phase;
        warmPos = 0.0;
        warmInc = 1.0 / std::max (1.0, static_cast<double> (numChunks * sig.numSamples));
        warmW = warmA = warmB = warmY = 0.0;
        for (int c = 0; c < numChunks; ++c)
        {
            std::fill (std::begin (l), std::end (l), 0.0f);
            std::fill (std::begin (r), std::end (r), 0.0f);
            render (sig, 0, sig.numSamples, l, r);
        }
        warmPhase = 0;
    };

    const int quarter = std::max (1, chunks / 4);
    run (quarter, 0);      // oscillators, envelopes and filter settle
    run (chunks, 1);       // mixer means
    if (warmW > 0.0)
    {
        dcTrimA = static_cast<float> (warmA / warmW);
        dcTrimB = static_cast<float> (warmB / warmW);
    }
    run (chunks, 0);       // the filter settles with the corrected mixer
    run (chunks, 2);       // filter output mean = charge of the coupling capacitor
    if (warmW > 0.0)
        filterDc = static_cast<float> (warmY / warmW);

    dcTrimCoeff = normalCoeff;

    // Silent and idle again. The filter keeps running state and the coupling capacitors keep their charge,
    // exactly as in the instrument, where the voice circuitry never stops.
    filterEnv.reset();
    ampEnv.reset();
    modEnv.reset();
    note = -1;
    held = false;
    hasPitch = false;
    polyPressure = 0.0f;
}

void SynthVoice::noteOff() noexcept
{
    held = false;
    filterEnv.noteOff();
    ampEnv.noteOff();
    modEnv.noteOff();
}

void SynthVoice::updateControl (const ChunkSignals& sig) noexcept
{
    lastSig = &sig;

    // Drift since the last TUNE always advances, even when silent.
    const float age = std::clamp (sig.params->analogAge, 0.0f, 1.0f);
    const float driftSemis = 0.0015f + 0.035f * age; // ~0.15 .. 3.6 cents std-dev
    driftNow[0] = driftA.next() * driftSemis;
    driftNow[1] = driftB.next() * driftSemis;
    driftNow[2] = driftF.next() * (0.004f + 0.03f * age); // octaves

    if (isActive())
        computeConstants (sig);
    else
        constantsDirty = true;
}

void SynthVoice::computeConstants (const ChunkSignals& sig) noexcept
{
    constantsDirty = false;
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

    const float dA = driftNow[0], dB = driftNow[1], dF = driftNow[2];

    // Analog (unquantised) pitch terms: drift, deliberate per-voice detune, FINE.
    const float detune = p.voiceDetune * 0.07f;
    const float trim = p.trimTune[static_cast<size_t> (trimIndex & 7)] * 0.01f;
    analogTune[0] = dA + fp.oscDetune[0] * detune + p.osc1Fine * 0.01f + trim;
    analogTune[1] = dB + fp.oscDetune[1] * detune + p.osc2Fine * 0.01f + trim;

    // Digital part of the CV: KBD + FREQ knob + autotune bias on the 14-bit grid.
    knobSemis[0] = static_cast<float> (p.osc1Semi + 12 * p.osc1Octave);
    knobSemis[1] = static_cast<float> (p.osc2Semi) * (p.osc2LoFreq ? 2.0f : 1.0f) // INIT FREQ range doubles
                   + static_cast<float> (12 * p.osc2Octave);
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
    const std::array<float, 8> dcKey { sig.saw1[last], sig.pulse1[last], sig.osc1Pw[last], sig.saw2[last],
                                       sig.tri2[last], sig.pulse2[last], sig.osc2Pw[last], static_cast<float> (model) };
    if (dcKey != dcCache)
    {
        dcCache = dcKey;
        dcA = unitDc (0, sig.saw1[last], 0.0f, sig.pulse1[last], sig.osc1Pw[last]);
        dcB = unitDc (1, sig.saw2[last], sig.tri2[last], sig.pulse2[last], sig.osc2Pw[last]);
    }

    // OSC B only has to run when it can be heard or when something depends on it.
    bool matrixUsesB = false;
    for (const auto& s : p.matrix)
        matrixUsesB = matrixUsesB
                      || (s.amount != 0.0f
                          && (s.source == ModSource::Osc2 || s.dest == ModDest::Osc2Level || s.dest == ModDest::CrossMod
                              || s.dest == ModDest::RingLevel));
    needOscB = sig.mix2[0] > 0.0f || sig.mix2[last] > 0.0f || p.osc1Sync || matrixUsesB
               || sig.crossMod[0] > 0.0f || sig.crossMod[last] > 0.0f || sig.mixRing[0] > 0.0f || sig.mixRing[last] > 0.0f
               || (p.pmOn && (sig.pmOsc2[0] > 0.0f || sig.pmOsc2[last] > 0.0f));

    cutoffOffset = fp.cutoff * 0.04f * spread + dF + p.trimCutoff[static_cast<size_t> (trimIndex & 7)];
    resonanceTrim = 1.0f + fp.resonance * 0.03f * spread;
    vcaTrim = 1.0f + fp.vcaGain * 0.03f * spread;
    bleed = 0.0005f + 0.004f * age;

    filter.setModel (static_cast<LadderFilter::Model> (std::clamp (p.filterModel, 0, 4)));
    filter.setShape (p.filterSlope == 1, static_cast<LadderFilter::Mode> (std::clamp (p.filterMode, 0, 2)));
    filter.setHighpass (p.hpfHz);

    const std::array<float, 13> envKey { p.fenvA, p.fenvD, p.fenvS, p.fenvR, p.aenvA, p.aenvD, p.aenvS, p.aenvR, age,
                                         p.menvA, p.menvD, p.menvS, p.menvR };
    if (! envCacheValid || envKey != envCache)
    {
        envCache = envKey;
        envCacheValid = true;
        modEnv.setParameters (p.menvA, p.menvD, p.menvS, p.menvR);
        const float envSpread = 0.05f * spread;
        filterEnv.setParameters (p.fenvA * (1.0f + envSpread * fp.envTime[0]), p.fenvD * (1.0f + envSpread * fp.envTime[0]),
                                 p.fenvS, p.fenvR * (1.0f + envSpread * fp.envTime[0]));
        ampEnv.setParameters (p.aenvA * (1.0f + envSpread * fp.envTime[1]), p.aenvD * (1.0f + envSpread * fp.envTime[1]),
                              p.aenvS, p.aenvR * (1.0f + envSpread * fp.envTime[1]));
    }

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
    beginRender (sig);
    for (int j = 0; j < count; ++j)
        tick (sig, static_cast<size_t> (start + j), left[j], right[j]);
}

void SynthVoice::beginRender (const ChunkSignals& sig) noexcept
{
    const SynthParams& p = *sig.params;
    tickVelGain = 1.0f - p.ampVelocity + p.ampVelocity * velocity;
    tickFilterVelOct = p.filterVelocity * velocity * 3.0f;
    tickPressure = std::max (sig.channelPressure, polyPressure);
    tickAtOct = p.aftertouchAmount * tickPressure * 3.0f;
    tickCommon = sig.bendSemitones + sig.tuneSemitones; // analog: master tune, pitch wheel
    tickInvChunk = 1.0f / static_cast<float> (std::max (1, sig.numSamples));
}

void SynthVoice::tick (const ChunkSignals& sig, size_t i, float& left, float& right) noexcept
{
    Front f;
    tickFront (sig, i, f);

    const float pre = f.bus * (0.9f + 3.0f * sig.drive[i]);
    const float x = fastmath::tanh (pre * 0.5f) * 2.0f;
    const float fc = fastmath::exp2 (std::min (f.cutoffOct, 15.0f));

    // AC coupling into the VCA (C4165): removes the DC the filter's own saturation produces.
    const float yRaw = filter.process (x, fc, f.resonance);
    if (warmPhase == 2)
    {
        const double w = hannWeight();
        warmY += static_cast<double> (yRaw) * w;
        warmW += w;
        warmPos += warmInc;
    }
    const float y = yRaw - filterDc;
    filterDc += y * dcTrimCoeff;

    // VCA with a little OTA colour.
    const float out = fastmath::tanh (y * f.gain * 0.6f) * (voiceOutputScale / 0.6f);
    left += out * f.panL;
    right += out * f.panR;
}

void SynthVoice::tickFront (const ChunkSignals& sig, size_t i, Front& front) noexcept
{
    const SynthParams& p = *sig.params;
    const float velGain = tickVelGain;
    const float filterVelOct = tickFilterVelOct;
    const float pressure = tickPressure;
    const float atOct = tickAtOct;
    const bool sync = p.osc1Sync;
    const bool pmOn = p.pmOn;
    const bool rev3 = model == 0;
    const float common = tickCommon;
    const float invChunk = tickInvChunk;
    {

        pitch += (pitchTarget - pitch) * glideCoeff;
        const float fenv = filterEnv.next();
        const float aenv = ampEnv.next();
        const float menv = modEnv.next();
        const double lfo2Inc = lfo2RateMod == 0.0f ? sig.lfo2Inc : sig.lfo2Inc * static_cast<double> (fastmath::exp2 (lfo2RateMod));
        const float lfo2Value = lfo2.next (lfo2Inc, static_cast<PolyLfo::Wave> (std::clamp (p.lfo2Wave, 0, 6)));
        lfoDelayGain = std::min (1.0f, lfoDelayGain + lfoDelayInc);
        const float lfo = sig.lfo[i] * lfoDelayGain;
        float white, cvWhite[2];
        noise.white3 (white, cvWhite[0], cvWhite[1]);
        pinkState += (white - pinkState) * 0.05f;
        const float pink = pinkState * 4.0f;

        // CV path extras: S/H droop (saw-shaped, refreshed every 6 ms) and wide-band CV noise.
        float cvExtra[2];
        for (size_t o = 0; o < 2; ++o)
        {
            droopPhase[o] += droopInc;
            if (droopPhase[o] >= 1.0)
                droopPhase[o] -= 1.0;
            cvNoise[o] += (cvWhite[o] * cvNoiseGain - cvNoise[o]) * cvNoiseCoeff;
            cvExtra[o] = cvNoise[o] - droopDepth[o] * static_cast<float> (droopPhase[o]);
        }

        // Mod matrix
        float dst[static_cast<size_t> (ModDest::count)];
        if (numSlots == 0)
        {
            for (auto& d : dst)
                d = 0.0f;
        }
        else
        {
            for (auto& d : dst)
                d = 0.0f;
            const float keySrc = (pitch - 60.0f) * (1.0f / 60.0f);
            const float src[static_cast<size_t> (ModSource::count)] { fenv, aenv, lastOscB, lfo, sig.modWheel, velocity, pressure, pink,
                                                                      menv, lfo2Value, keySrc, noteRandom };
            for (int s = 0; s < numSlots; ++s)
                dst[slots[static_cast<size_t> (s)].dest] += src[slots[static_cast<size_t> (s)].source] * slots[static_cast<size_t> (s)].amount;
        }
        lfo2RateMod = dst[static_cast<size_t> (ModDest::Lfo2Rate)];

        const float notePitch = pitch + unisonOffset;

        // OSC B first: it is the sync master and the poly-mod source.
        const float pitchB = (kbdB ? notePitch : 60.0f) + cvOffset[1] + analogTune[1] + common + cvExtra[1]
                             + dst[static_cast<size_t> (ModDest::Osc2Freq)];
        float resetB = -1.0f;
        float mB = 0.0f;
        lastOscB = 0.0f;
        if (needOscB)
        {
            const float pwB = std::clamp (sig.osc2Pw[i] + dst[static_cast<size_t> (ModDest::Osc2Pw)], 0.0f, 1.0f);
            const VcoOutputs b = rev3 ? cem[1].process (pitchB, pwB, -1.0f) : ssm[1].process (pitchB, pwB, -1.0f);
            resetB = rev3 ? cem[1].lastResetD() : ssm[1].lastResetD();

            // OSC B through the mixer / poly-mod differential pairs (same waveform switches feed both).
            mB = ota::mix (b.saw, b.tri, b.pulse, sig.saw2[i], sig.tri2[i], sig.pulse2[i]);
            lastOscB = mB - dcB - dcTrimB;
            dcTrimB += lastOscB * dcTrimCoeff;
            if (warmPhase == 1)
                warmB += static_cast<double> (lastOscB) * hannWeight();
        }

        // Poly-Mod: filter envelope and OSC B (with its DC, as on the hardware) -> FREQ A / PW A / filter.
        const float pm = pmOn ? fenv * sig.pmFilterEnv[i] + mB * sig.pmOsc2[i] : 0.0f;

        // Linear FM (cross-mod): OSC B moves OSC A's frequency around its centre, so the pitch holds while
        // sidebands grow (bells, metallic plucks). Up to +-3x the carrier frequency at full depth.
        float fmSemis = 0.0f;
        const float fmDepth = sig.crossMod[i] + dst[static_cast<size_t> (ModDest::CrossMod)];
        if (fmDepth > 0.0f)
        {
            const float factor = 1.0f + std::min (fmDepth, 1.0f) * std::min (fmDepth, 1.0f) * 6.0f * lastOscB;
            fmSemis = 12.0f * fastmath::log2 (std::max (factor, 0.02f));
        }

        const float pitchA = notePitch + cvOffset[0] + analogTune[0] + common + cvExtra[0] + fmSemis
                             + dst[static_cast<size_t> (ModDest::Osc1Freq)] + (p.pmFreqA ? pm * polyModPitchRange : 0.0f);
        const float pwA = std::clamp (sig.osc1Pw[i] + dst[static_cast<size_t> (ModDest::Osc1Pw)] + (p.pmPwA ? pm * pwModRange : 0.0f), 0.0f, 1.0f);
        const float syncD = sync ? resetB : -1.0f;
        const VcoOutputs a = rev3 ? cem[0].process (pitchA, pwA, syncD) : ssm[0].process (pitchA, pwA, syncD);
        const float mA = ota::mix (a.saw, 0.0f, a.pulse, sig.saw1[i], 0.0f, sig.pulse1[i]) - dcA - dcTrimA;
        dcTrimA += mA * dcTrimCoeff;
        if (warmPhase == 1)
        {
            const double w = hannWeight();
            warmA += static_cast<double> (mA) * w;
            warmW += w;
            warmPos += warmInc;
        }

        // Sub oscillator: a flip-flop clocked by OSC A's resets (one or two octaves down), square wave with
        // band-limited steps on the same timeline and latency as the VCO outputs.
        const float resetA = rev3 ? cem[0].lastResetD() : ssm[0].lastResetD();
        if (resetA >= 0.0f && (p.subOctave == 0 || (++subCount & 1) == 0))
        {
            subRing.add (BlepTable::get(), resetA, -subState, 0.0f);
            subState = -subState;
        }
        const float sub = subRing.push (subState * 0.5f);

        // Ring modulator: OSC A x OSC B (both AC-coupled mixer signals).
        const float ring = 2.0f * mA * lastOscB;

        // Mixer bus (AC-coupled) + noise + OTA feed-through, then the DRIVE stage into the filter.
        const auto lvl = [&dst] (float base, ModDest d) { return std::max (0.0f, base + dst[static_cast<size_t> (d)]); };
        float bus = mA * lvl (sig.mix1[i], ModDest::Osc1Level) + lastOscB * lvl (sig.mix2[i], ModDest::Osc2Level)
                    + white * lvl (sig.mixNoise[i], ModDest::NoiseLevel) * 0.8f + ring * lvl (sig.mixRing[i], ModDest::RingLevel)
                    + sub * lvl (sig.mixSub[i], ModDest::SubLevel) + (mA + lastOscB) * bleed;
        const float driveMod = dst[static_cast<size_t> (ModDest::Drive)];
        if (driveMod != 0.0f) // the engine applies (0.9 + 3 * DRIVE); rescale for this voice's modulated drive
            bus *= (0.9f + 3.0f * std::max (0.0f, sig.drive[i] + driveMod)) / (0.9f + 3.0f * sig.drive[i]);
        front.bus = bus;
        front.cutoffOct = sig.cutoffOct[i] + cutoffOffset + sig.envAmount[i] * fenv * envCutoffRange
                                + keytrack * (notePitch - 60.0f) * (1.0f / 12.0f) + filterVelOct + atOct
                                + dst[static_cast<size_t> (ModDest::FilterCutoff)] + (p.pmFilter ? pm * polyModCutoffRange : 0.0f);
        front.resonance = (sig.resonance[i] + dst[static_cast<size_t> (ModDest::Resonance)]) * resonanceTrim;
        front.gain = aenv * velGain * std::max (0.0f, 1.0f + dst[static_cast<size_t> (ModDest::AmpLevel)]) * vcaTrim;

        // Pan interpolates across the whole chunk by absolute position, independent of sub-ranges.
        const float t = static_cast<float> (i + 1) * invChunk;
        front.panL = panL + (panLTarget - panL) * t;
        front.panR = panR + (panRTarget - panR) * t;
    }
}

} // namespace augur
