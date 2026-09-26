#pragma once

#include "Oscillator/TestSine.h"
#include "Util/Smoother.h"

#include <array>

namespace augur
{

// Monophonic last-note-priority voice used to validate the MIDI -> audio pipeline.
// Gate is a short linear ramp so note on/off never clicks.
class MonoTestVoice
{
public:
    static constexpr int maxHeldNotes = 32;
    static constexpr double gateRampSeconds = 0.005;

    void prepare (double sampleRate) noexcept;
    void reset() noexcept;

    void noteOn (int note, float velocity) noexcept;
    void noteOff (int note) noexcept;
    void allNotesOff() noexcept;

    // Overwrites out[0..numSamples).
    void render (float* out, int numSamples) noexcept;

    bool isActive() const noexcept { return amp.isSmoothing() || amp.getCurrent() != 0.0f; }
    int getCurrentNote() const noexcept { return currentNote; }

    static double noteToHz (double note) noexcept;

private:
    void removeHeld (int note) noexcept;

    std::array<int, maxHeldNotes> held {};
    int numHeld = 0;
    int currentNote = -1;

    TestSine osc;
    LinearSmoother amp;
};

} // namespace augur
