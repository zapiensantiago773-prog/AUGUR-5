#pragma once

#include "Analog/Autotune.h"
#include "Analog/Drift.h"
#include "Analog/Fingerprint.h"
#include "Engine/ChunkSignals.h"
#include "Envelope/RcAdsr.h"
#include "Filter/LadderFilter.h"
#include "Oscillator/Cem3340.h"
#include "Oscillator/Noise.h"
#include "Oscillator/Ssm2030.h"

#include <array>
#include <cstdint>

namespace augur
{

// One complete Prophet-style voice:
//   CV path : 14-bit DAC (1/128 semitone) + autotune biases + S/H droop + drift + CV noise
//   VCOs    : CEM3340 (Rev 3, triangle core) or SSM2030 (Rev 1/2, ramp core), capacitor-discharge sync
//   mixer   : CA3280 differential pairs (saw on +, pulse on -), AC-coupled
//   poly-mod: OSC B through its own CA3280 + filter envelope -> FREQ A / PW A / filter (exponential)
//   filter  : 4-pole ZDF (CEM3320 / SSM2040), CEM3310 RC envelopes, OTA-coloured VCA
class SynthVoice
{
public:
    struct NoteOn
    {
        int note = 60;
        float velocity = 1.0f;
        bool retrigger = true;       // false = legato: envelopes keep running
        bool glide = false;          // slide from the current pitch
        float unisonOffset = 0.0f;   // semitones
        float panPosition = 0.0f;    // -1..1 before VOICE SPREAD
        std::uint64_t order = 0;     // allocation timestamp
    };

    void prepare (double internalRate, double controlRate, std::uint64_t seed) noexcept;
    void reset() noexcept;

    void noteOn (const NoteOn& n) noexcept;
    void noteOff() noexcept;
    void setPressure (float p) noexcept { polyPressure = p; }

    // Once per control chunk, for every voice (active or not) so drift keeps evolving identically.
    void updateControl (const ChunkSignals& sig) noexcept;

    // Adds this voice's stereo output for samples [start, start + count) of the chunk.
    void render (const ChunkSignals& sig, int start, int count, float* left, float* right) noexcept;

    bool isActive() const noexcept { return ampEnv.isActive(); }
    bool isHeld() const noexcept { return held; }
    bool isReleasing() const noexcept { return ampEnv.isReleasing(); }
    int getNote() const noexcept { return note; }
    std::uint64_t getOrder() const noexcept { return order; }
    float getLevel() const noexcept { return ampEnv.getLevel(); }

    // Testing/inspection: the autotune table of oscillator 0 (A) or 1 (B) for the current model.
    const AutotuneTable& getTuning (int osc) const noexcept { return tuning[static_cast<size_t> (osc)]; }
    double staticFrequency (int osc, double pitch) const noexcept;

private:
    struct ActiveSlot
    {
        int source;
        int dest;
        float amount; // already mapped to destination units
    };

    void configureUnits (int model, float age) noexcept;
    float dacOffset (int osc, float keyPitch, float knob) const noexcept;
    void updateCvOffsets() noexcept;

    std::array<Cem3340Vco, 2> cem;
    std::array<Ssm2030Vco, 2> ssm;
    std::array<AutotuneTable, 2> tuning;
    RcAdsr filterEnv, ampEnv;
    LadderFilter filter;
    NoiseSource noise;
    Drift driftA, driftB, driftF;
    VoiceFingerprint fp {};

    double sampleRate = 96000.0;
    int model = -1;          // 0 = CEM3340 (Rev 3), 1 = SSM2030 (Rev 1/2)
    int tunedAgeBucket = -1; // units/autotune are rebuilt when ANALOG AGE moves to another bucket

    // Note state
    int note = -1;
    float velocity = 1.0f;
    bool held = false;
    std::uint64_t order = 0;
    float pitch = 60.0f, pitchTarget = 60.0f;
    bool hasPitch = false;
    float unisonOffset = 0.0f;
    float panPosition = 0.0f;
    float polyPressure = 0.0f;
    float lfoDelayGain = 1.0f, lfoDelayInc = 1.0f;
    float lastOscB = 0.0f;   // OSC B mixer signal (AC), matrix source
    float pinkState = 0.0f;  // low-passed noise for the NOISE modulation source

    // CV path (per chunk)
    std::array<float, 2> cvOffset {};   // DAC quantisation + autotune bias + knob settings (semitones)
    std::array<float, 2> knobSemis {};  // OSC FREQ knobs as last seen by updateControl
    float loFreqOffset = 0.0f;
    bool kbdB = true;
    std::array<float, 2> analogTune {}; // drift + deliberate detune (semitones)
    std::array<double, 2> droopPhase {};
    std::array<float, 2> droopDepth {};  // semitones of S/H droop per 6 ms refresh
    std::array<float, 2> cvNoise {};     // filtered CV noise state (semitones)
    float cvNoiseCoeff = 0.1f, cvNoiseGain = 0.0f;
    double droopInc = 0.0;

    // Mixer DC (AC coupling): analytic mean per chunk + a slowly learned residual that persists across
    // notes (a unit's offset is static for given settings), so note starts carry no DC step.
    float dcA = 0.0f, dcB = 0.0f;
    float dcTrimA = 0.0f, dcTrimB = 0.0f, dcTrimCoeff = 0.0f;
    float filterDc = 0.0f; // C4165 2.2 uF between VCF and VCA: its charge persists (the real voice never stops)
    float cutoffOffset = 0.0f;
    float resonanceTrim = 1.0f;
    float vcaTrim = 1.0f;
    float bleed = 0.0f;
    float glideCoeff = 1.0f;
    float keytrack = 1.0f;
    float panL = 0.7071f, panR = 0.7071f, panLTarget = 0.7071f, panRTarget = 0.7071f; // chunk start / end
    std::array<ActiveSlot, 4> slots {};
    int numSlots = 0;
};

} // namespace augur
