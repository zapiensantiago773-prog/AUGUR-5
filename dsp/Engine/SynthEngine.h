#pragma once

#include "Effects/Fuzz.h"
#include "Engine/ChunkSignals.h"
#include "Rack/FxRack.h"
#include "Engine/SynthParams.h"
#include "Modulation/Arpeggiator.h"
#include "Modulation/Lfo.h"
#include "Modulation/WheelModNoise.h"
#include "Util/Halfband.h"
#include "Util/HalfbandFir.h"
#include "Util/Lr4.h"
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
    void clearTails() noexcept; // the effects' tails (a new sound must not inherit the last one's echoes)
    // A new sound fades in (6 ms, raised cosine) on the voices' bus, ahead of FUZZ, the decimators and the effects.
    void beginFadeIn() noexcept { fadeInPos = 0.0f; }

    int getOversampling() const noexcept { return oversampling; }

    // Effect read-outs for the GUI (audio -> GUI, relaxed).
    float getCompGainReduction() const noexcept { return compGr.load (std::memory_order_relaxed); }
    float getPhaserHz() const noexcept { return phaserHzMeter.load (std::memory_order_relaxed); }
    float getFlangerMs() const noexcept { return flangerMsMeter.load (std::memory_order_relaxed); }
    float getEchoHeadMs() const noexcept { return echoHeadMeter.load (std::memory_order_relaxed); }
    double getTempo() const noexcept { return tempoMeter.load (std::memory_order_relaxed); }
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

    // SPREAD CENTRED (D-060): POLY voices keep their places relative to each other, shifted together so that the
    // sounding energy is balanced left / right. panCentreFor() is that shift (pan units) for the active voices, leaving
    // out `skip` (a voice being taken over) and adding a note about to start (its place and weight).
    static bool spreadCentred (const SynthParams& p) noexcept
    {
        return p.spreadMode == 0 && ! p.unison && p.voiceCount > 1 && p.voiceMode == 0;
    }
    float panCentreFor (const SynthParams& p, const SynthVoice* skip, float extraPlace, float extraWeight) const noexcept;
    float panGlide = 1.0f; // per control chunk: the sounding voices follow the centre over 30 ms

    // Chunk-rate glides of PAN, SPREAD and the HPF, and the short dip of the voice bus around a filter switch.
    float panNow = 0.0f, spreadNow = 0.3f, hpfOctNow = 0.0f;
    std::array<int, 3> heldFilter { 0, 0, 0 }; // MODEL, slope, mode the voices are running
    int duckState = 0;                         // 0 idle, 1 fading out, 2 fading back in
    float duckFrom = 1.0f, duckTo = 1.0f;      // bus gain at the start / end of the current chunk

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
    SynthVoice warmVoice; // warmUp() runs on a copy of voice 0, so a note held on voice 0 keeps sounding
    float fadeInPos = 1.0f, fadeInStep = 1.0f;
    std::array<LadderFilter, 4> spareFilters; // idle lanes of a partly filled SIMD group
    std::array<std::atomic<float>, maxVoices> voiceLevels {};
    // A voice that sounds at any point of a control chunk renders to the end of that chunk, also after its release ends
    // (silent: its gain is 0). Its noise and oscillators then advance the same number of samples however the host
    // slices its blocks; stopping at the end of the segment made the next note on that voice depend on the block sizes.
    std::array<bool, maxVoices> inChunk {};
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
    int lastVoiceMode = 0;
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
    LinearSmoother smLevel, smMixRing, smMixSub, smCrossMod, smWmMix, smWmDepth;
    WheelModNoise wheelModNoise;
    double lfo2Phase = 0.0; // free-running LFO 2 (voices start from it when not retriggered)
    float fxCoeff = 0.1f;
    bool fuzzActive = false;
    std::atomic<float> compGr { 0.0f }, phaserHzMeter { 600.0f }, flangerMsMeter { 2.0f }, echoHeadMeter { 177.0f };
    std::atomic<double> tempoMeter { 120.0 };

    // Output stage
    std::array<float, ChunkSignals::maxSamples> busL {}, busR {};
    // The voices' front halves for one SIMD group, four lanes per sample (see SynthVoice::renderFront).
    struct alignas (16) FrontBuffers
    {
        std::array<float, ChunkSignals::maxSamples * 4> bus {}, cutoffOct {}, resonance {}, gain {}, panL {}, panR {};
    };
    FrontBuffers front;
    HalfbandDecimator decimL, decimR;
    HalfbandDownsampler<27> quadL, quadR; // DIVINE: 4x -> 2x before the main decimator
    float dcL = 0.0f, dcR = 0.0f, dcCoeff = 0.0f;
    // UNISON and DUO stack detuned voices on one note across the stereo field: their beating differs left and right,
    // so the bass decorrelates (correlation 0.47 and -1.4 dB in mono at VOICE SPREAD 1). There the voices' panning
    // acts only above stereoBassHz and their bass sits where PAN puts the instrument (D-057). POLY voices are each
    // a mono source panned in phase: they are left exactly as they are (a high-pass on the side there turned the
    // side's phase up to ~1 kHz and narrowed every voice's pan). The axis follows PAN over 20 ms; the stage fades
    // in and out over 20 ms when the mode changes.
    MonoBass voiceBass;
    double panAngle = 0.25 * 3.14159265358979, panAxisL = 0.70710678118654752, panAxisR = 0.70710678118654752, panCoeff = 1.0;
    double stackBlend = 0.0, stackStep = 0.001;
    bool outputStageFresh = true; // after prepare / reset the first block takes PAN and the mode as they are
    Random floorNoise;
    Fuzz fuzz;
    rack::FxRack rack;
};

} // namespace augur
