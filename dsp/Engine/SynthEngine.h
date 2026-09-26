#pragma once

#include "Effects/Chorus.h"
#include "Effects/Fuzz.h"
#include "Effects/Phaser.h"
#include "Effects/PlateReverb.h"
#include "Effects/Reverb.h"
#include "Effects/TapeDelay.h"
#include "Engine/ChunkSignals.h"
#include "Engine/SynthParams.h"
#include "Modulation/Arpeggiator.h"
#include "Modulation/Lfo.h"
#include "Util/Halfband.h"
#include "Util/HalfbandFir.h"
#include "Util/Random.h"
#include "Util/Smoother.h"
#include "Voice/SynthVoice.h"

#include <array>
#include <atomic>
#include <cstdint>

namespace augur
{

// The whole instrument, independent of any plugin framework.
// Voices run at an internal rate of 1x, 2x or 4x the host rate (quality ECO / GREAT / DIVINE; GREAT keeps
// it >= 88.2 kHz), are summed, and half-band decimators bring the stereo mix back to the host rate
// before the effects.
class SynthEngine
{
public:
    static constexpr int maxVoices = 16;
    static constexpr int controlInterval = 32; // host samples per control update
    static constexpr std::uint64_t defaultUnitSeed = 0xA06E53340ull;

    // oversamplingFactor: 1, 2 or 4; 0 = GREAT (2x below 80 kHz, otherwise 1x).
    void prepare (double hostSampleRate, std::uint64_t unitSeed = defaultUnitSeed, int oversamplingFactor = 0);

    // Internal-rate factor for a quality setting: 0 = ECO (1x), 1 = GREAT (internal >= 88.2 kHz),
    // 2 = DIVINE (internal >= 176.4 kHz); never more than 4x.
    static int oversamplingFor (int quality, double hostSampleRate) noexcept;

    // Total delay of the output against the MIDI input, in host samples (BLEP kernels + decimators).
    int getLatencySamples() const noexcept;
    void reset() noexcept;

    // Called once per host block (or more often); takes effect at the next control update.
    void setParams (const SynthParams& p) noexcept { pending = p; }
    void setTransport (double bpm, bool playing, double ppqAtBlockStart) noexcept;

    // MIDI (call between process() calls for sample accuracy)
    void noteOn (int note, float velocity) noexcept;
    void noteOff (int note) noexcept;
    void setSustain (bool down) noexcept;
    void setPitchBend (float normalised) noexcept { bend = normalised; }   // -1..1
    void setModWheel (float v) noexcept { modWheel = v; }                  // 0..1
    void setChannelPressure (float v) noexcept { channelPressure = v; }    // 0..1
    void setPolyPressure (int note, float v) noexcept;
    void allNotesOff() noexcept;
    void allSoundOff() noexcept;

    // Overwrites left/right with numSamples of output.
    void process (float* left, float* right, int numSamples) noexcept;

    // Charges every voice's coupling capacitors for the current parameters (like an instrument that has
    // been switched on for a while), so even the first note carries no DC step. Not for the audio thread
    // while it is running: call after prepare(), or with processing suspended.
    void warmUp() noexcept;

    int getOversampling() const noexcept { return oversampling; }
    float getVoiceLevel (int voice) const noexcept { return voiceLevels[static_cast<size_t> (voice)].load (std::memory_order_relaxed); }

private:
    void controlUpdate() noexcept;
    void renderSegment (float* left, float* right, int offset, int numSamples) noexcept;

    // Voice assignment always follows the latest parameters (not the last control chunk), so a note in
    // the very first block, or right after VOICES/UNISON changed, is assigned correctly.
    int activeVoiceCount() const noexcept;
    bool isMonoMode() const noexcept { return pending.unison || pending.voiceCount <= 1; }
    void syncMode() noexcept;
    void monoTrigger (int note, float velocity, bool retrigger) noexcept;
    void releaseNote (int note) noexcept;
    void startVoice (SynthVoice& voice, const SynthVoice::NoteOn& on) noexcept;
    double lfo2PhaseNow() const noexcept;
    void triggerNote (int note, float velocity) noexcept; // voice assignment (keyboard or arpeggiator)
    void keyReleased (int note) noexcept;                 // a key (or the sustain pedal) let go

    // Arpeggiator. Its clock is derived from the absolute sample counter, so steps land on the same
    // samples however the host splits its blocks.
    Arpeggiator::Settings arpSettings() const noexcept;
    double beatNow() const noexcept;
    void rebaseFreeClock() noexcept;
    void stopArp() noexcept;
    void runArp() noexcept;                      // fires every arp event due now
    int samplesToArpEvent() const noexcept;      // host samples until the next one (>= 1)

    // Parameters
    SynthParams pending, params;
    bool paramsInitialised = false;

    // Rates
    double hostRate = 48000.0, internalRate = 96000.0;
    int oversampling = 2;

    // Voices
    std::array<SynthVoice, maxVoices> voices;
    std::array<LadderFilter, 4> spareFilters; // idle lanes of a partly filled SIMD group
    std::array<std::atomic<float>, maxVoices> voiceLevels {};
    std::uint64_t noteCounter = 0;
    int nextVoice = 0;

    // Keyboard state
    std::array<bool, 128> keyDown {};
    std::array<bool, 128> sustained {};
    std::array<int, 128> monoStack {};
    std::array<float, 128> keyVelocity {};
    int monoStackSize = 0;
    bool sustainDown = false;
    bool lastMonoMode = false;
    int lastVoiceCount = 5;
    bool lastArpOn = false, lastArpLatch = false;

    Arpeggiator arp;
    std::int64_t freeOriginSample = 0; // free-running arp clock (transport stopped)
    double freeOriginBeat = 0.0, freeBpm = 120.0;
    bool lastPlaying = false;

    // Performance controls
    float bend = 0.0f, modWheel = 0.0f, channelPressure = 0.0f;

    // Transport
    double bpm = 120.0, ppqBlockStart = 0.0;
    bool playing = false;
    std::int64_t samplesSinceBlockStart = 0;

    // Control-rate state
    ChunkSignals sig;
    std::int64_t sampleCounter = 0; // host samples since prepare
    int chunkPos = 0;               // host samples consumed in the current chunk

    Lfo lfo;
    LinearSmoother smOsc1Pw, smOsc2Pw, smSaw1, smPulse1, smSaw2, smTri2, smPulse2;
    LinearSmoother smMix1, smMix2, smMixNoise, smDrive, smCutoff, smReso, smEnvAmt, smPmFenv, smPmOsc2, smLfoAmount;
    LinearSmoother smLevel, smMixRing, smMixSub, smCrossMod;
    double lfo2Phase = 0.0; // free-running LFO 2 (voices start from it when not retriggered)
    float chorusMix = 0.0f, delayMix = 0.0f, reverbMix = 0.0f, fxCoeff = 0.1f;
    float fuzzMix = 0.0f, phaserMix = 0.0f, hallMix = 0.0f, plateMix = 0.0f;
    bool chorusActive = false, delayActive = false, reverbActive = false;
    bool fuzzActive = false, phaserActive = false, plateActive = false;

    // Output stage
    std::array<float, ChunkSignals::maxSamples> busL {}, busR {};
    HalfbandDecimator decimL, decimR;
    HalfbandDownsampler<27> quadL, quadR; // DIVINE: 4x -> 2x before the main decimator
    float dcL = 0.0f, dcR = 0.0f, dcCoeff = 0.0f;
    Random floorNoise;
    Chorus chorus;
    TapeDelay delay;
    Reverb reverb;
    PlateReverb plate;
    Fuzz fuzz;
    Phaser phaser;
};

} // namespace augur
