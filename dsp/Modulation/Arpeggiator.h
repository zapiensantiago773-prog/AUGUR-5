#pragma once

#include "Util/Random.h"

#include <array>
#include <cstdint>

namespace augur
{

// Arpeggiator. Works in beats: the engine converts beats <-> samples from an absolute sample counter,
// so steps land on exact samples and never depend on how the host splits its blocks. With the host
// transport running the grid is the song's beat grid; stopped, it starts on the first key.
class Arpeggiator
{
public:
    enum class Mode
    {
        Up,
        Down,
        UpDown,
        Random,
        Order // as played
    };

    struct Settings
    {
        Mode mode = Mode::Up;
        int octaves = 1;         // 1..4
        double stepBeats = 0.25; // 1/16
        float gate = 0.5f;       // fraction of a step
        float swing = 0.0f;      // 0..0.5 of a step, applied to every second step
        bool latch = false;
    };

    struct Event
    {
        bool on;
        int note;
        float velocity;
    };

    void reset() noexcept;

    // Keyboard input. keyDown returns true when the arpeggio has to (re)start.
    bool keyDown (int note, float velocity, const Settings& s) noexcept;
    // Returns true when no note is left (the engine then releases the sounding step).
    bool keyUp (int note, const Settings& s) noexcept;
    // Latch switched off: drops the notes that are no longer held. Returns true when none is left.
    bool unlatch() noexcept;
    bool hasNotes() const noexcept { return numHeld > 0; }
    // Beat of the next step (for transport-jump detection).
    double nextStepBeat() const noexcept { return nextOn; }

    bool isRunning() const noexcept { return running; }
    int soundingNote() const noexcept { return sounding; }

    // Starts the step clock at `beatNow`; `onGrid` = wait for the next grid line (host playing).
    void start (double beatNow, bool onGrid, const Settings& s) noexcept;
    void stop() noexcept;

    // Beat of the next event (gate off or next step).
    double nextEventBeat() const noexcept;

    // Emits every event due at or before beatNow (at most three). Returns how many were written.
    int fire (double beatNow, const Settings& s, Event* out) noexcept;

private:
    int buildSequence (const Settings& s) noexcept;
    double stepOnBeat (std::int64_t k, const Settings& s) const noexcept;

    struct Held
    {
        int note;
        float velocity;
    };

    std::array<Held, 64> held {};
    int numHeld = 0;
    std::array<bool, 128> physicallyDown {};
    int numPhysical = 0;

    std::array<int, 512> sequence {};
    std::array<float, 512> sequenceVelocity {};

    bool running = false;
    double gridOrigin = 0.0;
    std::int64_t step = 0;     // index of the next step to play
    int position = 0;          // position inside the sequence
    double nextOn = 0.0, nextOff = 1.0e300;
    int sounding = -1;
    Random rng { 0xA4B1u };
};

} // namespace augur
