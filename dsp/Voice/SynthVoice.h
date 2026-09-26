#pragma once

#include "Analog/Drift.h"
#include "Analog/Fingerprint.h"
#include "Engine/ChunkSignals.h"
#include "Envelope/RcAdsr.h"
#include "Filter/LadderFilter.h"
#include "Oscillator/Noise.h"
#include "Oscillator/Vco.h"

#include <array>
#include <cstdint>

namespace augur
{

// One complete analog voice: VCO A + VCO B -> mixer (+noise, bleed, drive) -> 4-pole filter -> VCA,
// with two RC envelopes, poly-mod, the mod matrix and its own component fingerprint and drift.
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

private:
    struct ActiveSlot
    {
        int source;
        int dest;
        float amount; // already mapped to destination units
    };

    Vco vcoA, vcoB;
    RcAdsr filterEnv, ampEnv;
    LadderFilter filter;
    NoiseSource noise;
    Drift driftA, driftB, driftF;
    VoiceFingerprint fp {};

    double sampleRate = 96000.0;

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
    float lastB = 0.0f;

    // Per-chunk constants
    float tuneA = 0.0f, tuneB = 0.0f;         // semitones (offsets, fingerprint, drift)
    float scaleA = 0.0f, scaleB = 0.0f;       // relative V/oct error
    float cutoffOffset = 0.0f;                // octaves
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
