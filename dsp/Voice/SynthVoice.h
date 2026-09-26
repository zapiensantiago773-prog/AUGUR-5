#pragma once

#include "Analog/Autotune.h"
#include "Analog/Drift.h"
#include "Analog/Fingerprint.h"
#include "Engine/ChunkSignals.h"
#include "Envelope/RcAdsr.h"
#include "Filter/LadderFilter.h"
#include "Modulation/PolyLfo.h"
#include "Oscillator/BlepRing.h"
#include "Oscillator/Cem3340.h"
#include "Oscillator/Noise.h"
#include "Oscillator/Ssm2030.h"

#include <array>
#include <cmath>
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
        double lfo2Phase = 0.0;      // engine's free-running LFO 2 phase (used when not retriggered)
    };

    void prepare (double internalRate, double controlRate, std::uint64_t seed) noexcept;
    void reset() noexcept;

    void noteOn (const NoteOn& n) noexcept;
    void noteOff() noexcept;
    void setPressure (float p) noexcept { polyPressure = p; }

    // Charges the voice's coupling capacitors (mixer AC coupling, VCF->VCA C4165) by running it silently,
    // as a powered-up instrument would have them. Non-realtime use (prepare, state/preset changes).
    void warmUp (const ChunkSignals& sig, int chunks) noexcept;
    // All voices see the same average signal: a voice that wakes up takes the charge of a running one.
    void copyCouplingFrom (const SynthVoice& other) noexcept
    {
        dcTrimA = other.dcTrimA;
        dcTrimB = other.dcTrimB;
        filterDc = other.filterDc;
        filter.copyStateFrom (other.filter); // the real filter never stops: it sits at the same operating point
    }

    // Once per control chunk, for every voice (active or not) so drift keeps evolving identically.
    void updateControl (const ChunkSignals& sig) noexcept;

    // Adds this voice's stereo output for samples [start, start + count) of the chunk.
    void render (const ChunkSignals& sig, int start, int count, float* left, float* right) noexcept;

    // Sample-by-sample form of render(), so the engine can interleave voices: their dependency chains
    // are independent, which lets the CPU overlap them. Call beginRender() once per segment.
    void beginRender (const ChunkSignals& sig) noexcept;
    void tick (const ChunkSignals& sig, size_t i, float& left, float& right) noexcept;

    // Front half of a sample: everything up to the filter input (oscillators, CV path, modulation,
    // mixer). The engine runs the back half (drive, filter, coupling, VCA, pan) for four voices at once.
    struct Front
    {
        float bus = 0.0f;       // mixer bus into the DRIVE stage
        float cutoffOct = 0.0f; // log2 of the filter cutoff in Hz
        float resonance = 0.0f;
        float gain = 0.0f;      // VCA gain
        float panL = 0.0f, panR = 0.0f;
    };
    void tickFront (const ChunkSignals& sig, size_t i, Front& front) noexcept;

    // State the SIMD back half works on (loaded per segment, written back afterwards).
    LadderFilter& getFilter() noexcept { return filter; }
    float& couplingCharge() noexcept { return filterDc; }
    float couplingCoeff() const noexcept { return dcTrimCoeff; }
    static float outputScale() noexcept;

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
    double hannWeight() const noexcept
    {
        const double s = std::sin (3.14159265358979323846 * warmPos);
        return s * s;
    }
    void computeConstants (const ChunkSignals& sig) noexcept;
    float dacOffset (int osc, float keyPitch, float knob) const noexcept;
    void updateCvOffsets() noexcept;

    std::array<Cem3340Vco, 2> cem;
    std::array<Ssm2030Vco, 2> ssm;
    std::array<AutotuneTable, 2> tuning;
    RcAdsr filterEnv, ampEnv, modEnv;
    PolyLfo lfo2;
    Random noteRng;
    float noteRandom = 0.0f;      // NOTE RANDOM matrix source
    float lfo2RateMod = 0.0f;     // previous sample's LFO 2 RATE modulation (octaves)
    BlepRing subRing;             // sub oscillator: flip-flop on OSC A's resets, band-limited steps
    float subState = 1.0f;
    int subCount = 0;
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

    // Control-rate bookkeeping: silent voices only advance their drift; their constants are computed when
    // a note starts (from the same chunk's signals, so rendering stays block-size independent).
    const ChunkSignals* lastSig = nullptr;
    float tickVelGain = 1.0f, tickFilterVelOct = 0.0f, tickPressure = 0.0f, tickAtOct = 0.0f, tickCommon = 0.0f, tickInvChunk = 1.0f;
    bool constantsDirty = true;
    std::array<float, 3> driftNow {};
    bool needOscB = true;
    std::array<float, 13> envCache {};
    bool envCacheValid = false;
    std::array<float, 8> dcCache { -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f };

    // Mixer DC (AC coupling): analytic mean per chunk + a slowly learned residual that persists across
    // notes (a unit's offset is static for given settings), so note starts carry no DC step.
    float dcA = 0.0f, dcB = 0.0f;
    float dcTrimA = 0.0f, dcTrimB = 0.0f, dcTrimCoeff = 0.0f;
    float filterDc = 0.0f; // C4165 2.2 uF between VCF and VCA: its charge persists (the real voice never stops)

    // Warm-up measurement: Hann-weighted means over many cycles (a one-pole would follow the ripple).
    int warmPhase = 0;           // 0 = off, 1 = measure mixer means, 2 = measure filter output mean
    double warmPos = 0.0, warmInc = 0.0;
    double warmW = 0.0, warmA = 0.0, warmB = 0.0, warmY = 0.0;
    float cutoffOffset = 0.0f;
    float resonanceTrim = 1.0f;
    float vcaTrim = 1.0f;
    float bleed = 0.0f;
    float glideCoeff = 1.0f;
    float keytrack = 1.0f;
    float panL = 0.7071f, panR = 0.7071f, panLTarget = 0.7071f, panRTarget = 0.7071f; // chunk start / end
    std::array<ActiveSlot, 8> slots {};
    int numSlots = 0;
};

} // namespace augur
