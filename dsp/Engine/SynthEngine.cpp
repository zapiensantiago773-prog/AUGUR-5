#include "Engine/SynthEngine.h"

#include "Util/FastMath.h"

#include <algorithm>
#include <cmath>

namespace augur
{

namespace
{
float dbToGain (float db) noexcept
{
    return db <= -59.9f ? 0.0f : std::pow (10.0f, db * 0.05f);
}

float panPositionFor (int index, int count) noexcept
{
    return count <= 1 ? 0.0f : -1.0f + 2.0f * static_cast<float> (index) / static_cast<float> (count - 1);
}
} // namespace

void SynthEngine::prepare (double hostSampleRate, std::uint64_t unitSeed)
{
    hostRate = hostSampleRate;
    oversampling = hostSampleRate < 80000.0 ? 2 : 1;
    internalRate = hostSampleRate * oversampling;
    const double controlRate = hostSampleRate / controlInterval;

    for (size_t i = 0; i < voices.size(); ++i)
        voices[i].prepare (internalRate, controlRate, deriveSeed (unitSeed, 100 + i));

    lfo.prepare (internalRate, deriveSeed (unitSeed, 7));
    floorNoise.setSeed (deriveSeed (unitSeed, 8));
    chorus.prepare (hostSampleRate);
    delay.prepare (hostSampleRate);
    reverb.prepare (hostSampleRate);

    dcCoeff = static_cast<float> (1.0 - std::exp (-2.0 * 3.14159265358979 * 6.0 / hostSampleRate));
    fxCoeff = static_cast<float> (1.0 - std::exp (-(controlInterval / hostSampleRate) / 0.03));
    sig.sampleRate = internalRate;

    paramsInitialised = false;
    reset();
}

void SynthEngine::reset() noexcept
{
    for (auto& v : voices)
        v.reset();
    for (auto& l : voiceLevels)
        l.store (0.0f, std::memory_order_relaxed);

    keyDown.fill (false);
    sustained.fill (false);
    monoStackSize = 0;
    sustainDown = false;
    noteCounter = 0;
    nextVoice = 0;

    decimL.reset();
    decimR.reset();
    dcL = dcR = 0.0f;
    chorus.reset();
    delay.reset();
    reverb.reset();
    chorusMix = delayMix = reverbMix = 0.0f;
    chorusActive = delayActive = reverbActive = false;

    sampleCounter = 0;
    chunkPos = 0;
}

void SynthEngine::setTransport (double newBpm, bool isPlaying, double ppqAtBlockStart) noexcept
{
    bpm = newBpm > 1.0 ? newBpm : 120.0;
    playing = isPlaying;
    ppqBlockStart = ppqAtBlockStart;
    samplesSinceBlockStart = 0;
}

//==============================================================================
// Voice allocation

int SynthEngine::activeVoiceCount() const noexcept
{
    return std::clamp (params.voiceCount, 1, maxVoices);
}

void SynthEngine::noteOn (int note, float velocity) noexcept
{
    note = std::clamp (note, 0, 127);
    const auto n = static_cast<size_t> (note);
    keyDown[n] = true;
    sustained[n] = false;
    keyVelocity[n] = velocity;

    if (isMonoMode())
    {
        int w = 0;
        for (int i = 0; i < monoStackSize; ++i)
            if (monoStack[static_cast<size_t> (i)] != note)
                monoStack[static_cast<size_t> (w++)] = monoStack[static_cast<size_t> (i)];
        monoStackSize = w;
        const bool wasHolding = monoStackSize > 0;
        if (monoStackSize < static_cast<int> (monoStack.size()))
            monoStack[static_cast<size_t> (monoStackSize++)] = note;
        monoTrigger (note, velocity, ! (params.legato && wasHolding));
        return;
    }

    const int count = activeVoiceCount();
    int chosen = -1;

    // 1. The voice that last played this note (keeps a key's "personality", avoids double voices).
    for (int i = 0; i < count && chosen < 0; ++i)
        if (voices[static_cast<size_t> (i)].getNote() == note && voices[static_cast<size_t> (i)].isActive())
            chosen = i;

    // 2. Rotate through free voices (shows the per-voice analog variation, like the original assigner).
    for (int k = 0; k < count && chosen < 0; ++k)
    {
        const int i = (nextVoice + k) % count;
        if (! voices[static_cast<size_t> (i)].isActive())
            chosen = i;
    }

    // 3. Oldest releasing voice, 4. oldest held voice.
    for (int pass = 0; pass < 2 && chosen < 0; ++pass)
    {
        std::uint64_t oldest = UINT64_MAX;
        for (int i = 0; i < count; ++i)
        {
            const auto& v = voices[static_cast<size_t> (i)];
            if ((pass == 0 ? v.isReleasing() : true) && v.getOrder() < oldest)
            {
                oldest = v.getOrder();
                chosen = i;
            }
        }
    }

    SynthVoice::NoteOn on;
    on.note = note;
    on.velocity = velocity;
    on.retrigger = true;
    on.glide = params.glide > 0.0005f;
    on.panPosition = panPositionFor (chosen, count);
    on.order = ++noteCounter;
    voices[static_cast<size_t> (chosen)].noteOn (on);
    nextVoice = (chosen + 1) % count;
}

void SynthEngine::monoTrigger (int note, float velocity, bool retrigger) noexcept
{
    const int count = params.unison ? activeVoiceCount() : 1;
    for (int i = 0; i < count; ++i)
    {
        SynthVoice::NoteOn on;
        on.note = note;
        on.velocity = velocity;
        on.retrigger = retrigger;
        on.glide = params.glide > 0.0005f;
        // Unison: symmetric detune, up to +-25 cents at VOICE DETUNE = 1, spread across the stereo field.
        on.unisonOffset = count > 1 ? panPositionFor (i, count) * params.voiceDetune * 0.25f : 0.0f;
        on.panPosition = panPositionFor (i, count);
        on.order = ++noteCounter;
        voices[static_cast<size_t> (i)].noteOn (on);
    }
}

void SynthEngine::releaseNote (int note) noexcept
{
    if (isMonoMode())
    {
        int w = 0;
        for (int i = 0; i < monoStackSize; ++i)
            if (monoStack[static_cast<size_t> (i)] != note)
                monoStack[static_cast<size_t> (w++)] = monoStack[static_cast<size_t> (i)];
        const bool wasTop = monoStackSize > 0 && monoStack[static_cast<size_t> (monoStackSize - 1)] == note;
        monoStackSize = w;

        if (monoStackSize > 0)
        {
            if (wasTop)
            {
                const int top = monoStack[static_cast<size_t> (monoStackSize - 1)];
                monoTrigger (top, keyVelocity[static_cast<size_t> (top)], ! params.legato);
            }
        }
        else
        {
            for (auto& v : voices)
                if (v.isHeld())
                    v.noteOff();
        }
        return;
    }

    for (auto& v : voices)
        if (v.isHeld() && v.getNote() == note)
            v.noteOff();
}

void SynthEngine::noteOff (int note) noexcept
{
    note = std::clamp (note, 0, 127);
    keyDown[static_cast<size_t> (note)] = false;
    if (sustainDown)
    {
        sustained[static_cast<size_t> (note)] = true;
        return;
    }
    releaseNote (note);
}

void SynthEngine::setSustain (bool down) noexcept
{
    sustainDown = down;
    if (down)
        return;
    for (int n = 0; n < 128; ++n)
    {
        if (sustained[static_cast<size_t> (n)])
        {
            sustained[static_cast<size_t> (n)] = false;
            if (! keyDown[static_cast<size_t> (n)])
                releaseNote (n);
        }
    }
}

void SynthEngine::setPolyPressure (int note, float v) noexcept
{
    for (auto& voice : voices)
        if (voice.isActive() && voice.getNote() == note)
            voice.setPressure (v);
}

void SynthEngine::allNotesOff() noexcept
{
    for (auto& v : voices)
        if (v.isActive())
            v.noteOff();
    keyDown.fill (false);
    sustained.fill (false);
    monoStackSize = 0;
}

void SynthEngine::allSoundOff() noexcept
{
    allNotesOff();
    for (auto& v : voices)
        v.reset();
}

void SynthEngine::applyModeChange() noexcept
{
    const bool mono = isMonoMode();
    if (mono != lastMonoMode || params.voiceCount != lastVoiceCount)
    {
        for (auto& v : voices)
            if (v.isActive())
                v.noteOff();
        monoStackSize = 0;
        lastMonoMode = mono;
        lastVoiceCount = params.voiceCount;
    }
}

//==============================================================================
// Rendering

void SynthEngine::process (float* left, float* right, int numSamples) noexcept
{
    int done = 0;
    while (done < numSamples)
    {
        if (chunkPos == 0)
            controlUpdate();

        const int len = std::min (numSamples - done, controlInterval - chunkPos);
        renderSegment (left + done, right + done, chunkPos, len);

        chunkPos += len;
        if (chunkPos == controlInterval)
            chunkPos = 0;
        done += len;
        sampleCounter += len;
        samplesSinceBlockStart += len;
    }
}

void SynthEngine::controlUpdate() noexcept
{
    params = pending;
    const int n = controlInterval * oversampling;
    sig.numSamples = n;
    sig.params = &params;

    const auto initSmoother = [&] (LinearSmoother& s, double seconds, float value, double rate) {
        s.reset (rate, seconds, value);
    };

    const float cutoffOct = std::log2 (std::clamp (params.cutoffHz, 10.0f, 25000.0f));
    const float level = dbToGain (params.levelDb);
    const auto onOff = [] (bool b) { return b ? 1.0f : 0.0f; };

    if (! paramsInitialised)
    {
        initSmoother (smOsc1Pw, 0.02, params.osc1Pw, internalRate);
        initSmoother (smOsc2Pw, 0.02, params.osc2Pw, internalRate);
        initSmoother (smSaw1, 0.005, onOff (params.osc1Saw), internalRate);
        initSmoother (smPulse1, 0.005, onOff (params.osc1Pulse), internalRate);
        initSmoother (smSaw2, 0.005, onOff (params.osc2Saw), internalRate);
        initSmoother (smTri2, 0.005, onOff (params.osc2Tri), internalRate);
        initSmoother (smPulse2, 0.005, onOff (params.osc2Pulse), internalRate);
        initSmoother (smMix1, 0.02, params.mixOsc1, internalRate);
        initSmoother (smMix2, 0.02, params.mixOsc2, internalRate);
        initSmoother (smMixNoise, 0.02, params.mixNoise, internalRate);
        initSmoother (smDrive, 0.02, params.mixDrive, internalRate);
        initSmoother (smCutoff, 0.01, cutoffOct, internalRate);
        initSmoother (smReso, 0.02, params.resonance, internalRate);
        initSmoother (smEnvAmt, 0.02, params.envAmount, internalRate);
        initSmoother (smPmFenv, 0.02, params.pmFilterEnv, internalRate);
        initSmoother (smPmOsc2, 0.02, params.pmOsc2, internalRate);
        initSmoother (smLfoAmount, 0.02, params.lfoAmount, internalRate);
        initSmoother (smLevel, 0.02, level, hostRate);
        lastMonoMode = isMonoMode();
        lastVoiceCount = params.voiceCount;
        paramsInitialised = true;
    }

    applyModeChange();

    const auto fill = [n] (LinearSmoother& s, float target, ChunkSignals::Buffer& buf) {
        s.setTarget (target);
        for (size_t i = 0; i < static_cast<size_t> (n); ++i)
            buf[i] = s.next();
    };

    fill (smOsc1Pw, params.osc1Pw, sig.osc1Pw);
    fill (smOsc2Pw, params.osc2Pw, sig.osc2Pw);
    fill (smSaw1, onOff (params.osc1Saw), sig.saw1);
    fill (smPulse1, onOff (params.osc1Pulse), sig.pulse1);
    fill (smSaw2, onOff (params.osc2Saw), sig.saw2);
    fill (smTri2, onOff (params.osc2Tri), sig.tri2);
    fill (smPulse2, onOff (params.osc2Pulse), sig.pulse2);
    fill (smMix1, params.mixOsc1, sig.mix1);
    fill (smMix2, params.mixOsc2, sig.mix2);
    fill (smMixNoise, params.mixNoise, sig.mixNoise);
    fill (smDrive, params.mixDrive, sig.drive);
    fill (smCutoff, cutoffOct, sig.cutoffOct);
    fill (smReso, params.resonance, sig.resonance);
    fill (smEnvAmt, params.envAmount, sig.envAmount);
    fill (smPmFenv, params.pmFilterEnv, sig.pmFilterEnv);
    fill (smPmOsc2, params.pmOsc2, sig.pmOsc2);
    smLevel.setTarget (level);

    sig.modWheel = modWheel;
    sig.channelPressure = channelPressure;
    sig.bendSemitones = bend * static_cast<float> (params.pitchBendRange);
    sig.tuneSemitones = params.masterTuneCents * 0.01f;

    // Shared LFO (free-running, or locked to the host's beat position when synced and playing).
    double rate = std::clamp (static_cast<double> (params.lfoRate), 0.01, 50.0);
    if (params.lfoSync)
    {
        const double pos = std::log (std::max (rate, 0.05) / 0.05) / std::log (600.0);
        const auto idx = static_cast<size_t> (std::clamp (std::lround (pos * 15.0), 0L, 15L));
        const double beats = lfoSyncBeats[idx];
        rate = bpm / 60.0 / beats;
        if (playing)
        {
            const double ppqNow = ppqBlockStart + static_cast<double> (samplesSinceBlockStart) * bpm / (60.0 * hostRate);
            lfo.setPhase (ppqNow / beats);
        }
    }
    for (const auto& s : params.matrix)
    {
        if (s.dest != ModDest::LfoRate || s.amount == 0.0f)
            continue;
        const float src = s.source == ModSource::ModWheel ? modWheel : (s.source == ModSource::Aftertouch ? channelPressure : 0.0f);
        rate *= static_cast<double> (fastmath::exp2 (s.amount * src * 3.0f));
    }
    // A hint of analog instability on the LFO rate.
    rate *= 1.0 + 0.004 * params.analogAge * static_cast<double> (floorNoise.nextBipolar());

    const auto wave = static_cast<Lfo::Wave> (std::clamp (params.lfoWave, 0, 3));
    smLfoAmount.setTarget (params.lfoAmount);
    for (size_t i = 0; i < static_cast<size_t> (n); ++i)
        sig.lfo[i] = lfo.process (rate, wave) * smLfoAmount.next();

    for (auto& v : voices)
        v.updateControl (sig);

    // Effect send levels fade in/out instead of switching.
    chorusMix += ((params.chorusOn ? params.chorusMix : 0.0f) - chorusMix) * fxCoeff;
    delayMix += ((params.delayOn ? params.delayMix : 0.0f) - delayMix) * fxCoeff;
    reverbMix += ((params.reverbOn ? params.reverbMix : 0.0f) - reverbMix) * fxCoeff;
}

void SynthEngine::renderSegment (float* left, float* right, int offset, int numSamples) noexcept
{
    const int iStart = offset * oversampling;
    const int iCount = numSamples * oversampling;
    std::fill_n (busL.begin(), iCount, 0.0f);
    std::fill_n (busR.begin(), iCount, 0.0f);

    for (size_t v = 0; v < voices.size(); ++v)
    {
        auto& voice = voices[v];
        if (voice.isActive())
            voice.render (sig, iStart, iCount, busL.data(), busR.data());
        voiceLevels[v].store (voice.isActive() ? voice.getLevel() : 0.0f, std::memory_order_relaxed);
    }

    const float floorAmp = 1.6e-5f * params.analogAge;
    for (int k = 0; k < numSamples; ++k)
    {
        const auto kk = static_cast<size_t> (k);
        float l, r;
        if (oversampling == 2)
        {
            l = decimL.process (busL[2 * kk], busL[2 * kk + 1]);
            r = decimR.process (busR[2 * kk], busR[2 * kk + 1]);
        }
        else
        {
            l = busL[kk];
            r = busR[kk];
        }
        // DC blocker (AC-coupled output) + a whisper of analog noise floor.
        dcL += (l - dcL) * dcCoeff;
        dcR += (r - dcR) * dcCoeff;
        left[k] = l - dcL + floorAmp * floorNoise.nextBipolar();
        right[k] = r - dcR + floorAmp * floorNoise.nextBipolar();
    }

    // Effects: chorus -> delay -> reverb. Each runs only while audible, and clears when it goes idle.
    const auto runFx = [&] (bool on, float mix, bool& active, auto&& processFx, auto&& resetFx) {
        if (on || mix > 1.0e-4f)
        {
            active = true;
            processFx();
        }
        else if (active)
        {
            active = false;
            resetFx();
        }
    };

    runFx (params.chorusOn, chorusMix, chorusActive,
           [&] { chorus.process (left, right, numSamples, params.chorusRate, params.chorusDepth, chorusMix); },
           [&] { chorus.reset(); });
    runFx (params.delayOn, delayMix, delayActive,
           [&] { delay.process (left, right, numSamples, params.delayTime, params.delayFeedback, delayMix); },
           [&] { delay.reset(); });
    runFx (params.reverbOn, reverbMix, reverbActive,
           [&] { reverb.process (left, right, numSamples, params.reverbSize, params.reverbDecay, reverbMix); },
           [&] { reverb.reset(); });

    for (int k = 0; k < numSamples; ++k)
    {
        const float g = smLevel.next();
        left[k] *= g;
        right[k] *= g;
    }
}

} // namespace augur
