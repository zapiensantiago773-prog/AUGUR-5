#include "Modulation/Arpeggiator.h"

#include <algorithm>
#include <cmath>

namespace augur
{

void Arpeggiator::reset() noexcept
{
    numHeld = 0;
    physicallyDown.fill (false);
    numPhysical = 0;
    rng.setSeed (randomSeed); // RANDOM mode plays the same pattern after every reset (prepare, preset load)
    stop();
}

void Arpeggiator::stop() noexcept
{
    running = false;
    sounding = -1;
    nextOff = 1.0e300;
    position = 0;
}

bool Arpeggiator::keyDown (int note, float velocity, const Settings& s) noexcept
{
    note = std::clamp (note, 0, 127);
    // Latch: a new chord (first key after all keys were released) replaces the latched one.
    if (s.latch && numPhysical == 0)
        numHeld = 0;
    if (! physicallyDown[static_cast<size_t> (note)])
    {
        physicallyDown[static_cast<size_t> (note)] = true;
        ++numPhysical;
    }

    for (int i = 0; i < numHeld; ++i)
        if (held[static_cast<size_t> (i)].note == note)
            return false; // already part of the arpeggio
    if (numHeld < static_cast<int> (held.size()))
        held[static_cast<size_t> (numHeld++)] = { note, velocity };
    return ! running;
}

bool Arpeggiator::keyUp (int note, const Settings& s) noexcept
{
    note = std::clamp (note, 0, 127);
    if (physicallyDown[static_cast<size_t> (note)])
    {
        physicallyDown[static_cast<size_t> (note)] = false;
        --numPhysical;
    }
    if (s.latch)
        return false; // latched notes stay until the next chord

    int w = 0;
    for (int i = 0; i < numHeld; ++i)
        if (held[static_cast<size_t> (i)].note != note)
            held[static_cast<size_t> (w++)] = held[static_cast<size_t> (i)];
    numHeld = w;
    return numHeld == 0;
}

bool Arpeggiator::unlatch() noexcept
{
    int w = 0;
    for (int i = 0; i < numHeld; ++i)
        if (physicallyDown[static_cast<size_t> (held[static_cast<size_t> (i)].note)])
            held[static_cast<size_t> (w++)] = held[static_cast<size_t> (i)];
    numHeld = w;
    return numHeld == 0;
}

double Arpeggiator::stepOnBeat (std::int64_t k, const Settings& s) const noexcept
{
    const double base = gridOrigin + static_cast<double> (k) * s.stepBeats;
    return (k & 1) ? base + static_cast<double> (s.swing) * s.stepBeats : base;
}

void Arpeggiator::start (double beatNow, bool onGrid, const Settings& s) noexcept
{
    running = true;
    position = 0;
    sounding = -1;
    nextOff = 1.0e300;
    if (onGrid)
    {
        gridOrigin = 0.0;
        step = static_cast<std::int64_t> (std::ceil (beatNow / s.stepBeats - 1.0e-9));
    }
    else
    {
        gridOrigin = beatNow;
        step = 0;
    }
    nextOn = stepOnBeat (step, s);
}

double Arpeggiator::nextEventBeat() const noexcept
{
    return running ? std::min (nextOn, nextOff) : 1.0e300;
}

int Arpeggiator::buildSequence (const Settings& s) noexcept
{
    // Sorted copy (ascending) or play order, expanded over the octaves.
    std::array<Held, 64> base {};
    const int n = numHeld;
    for (int i = 0; i < n; ++i)
        base[static_cast<size_t> (i)] = held[static_cast<size_t> (i)];
    if (s.mode != Mode::Order)
        std::sort (base.begin(), base.begin() + n, [] (const Held& a, const Held& b) { return a.note < b.note; });

    int len = 0;
    const int octaves = std::clamp (s.octaves, 1, 4);
    for (int o = 0; o < octaves; ++o)
        for (int i = 0; i < n; ++i)
        {
            const int note = base[static_cast<size_t> (i)].note + 12 * o;
            if (note > 127)
                continue;
            sequence[static_cast<size_t> (len)] = note;
            sequenceVelocity[static_cast<size_t> (len)] = base[static_cast<size_t> (i)].velocity;
            ++len;
        }

    if (s.mode == Mode::Down)
    {
        std::reverse (sequence.begin(), sequence.begin() + len);
        std::reverse (sequenceVelocity.begin(), sequenceVelocity.begin() + len);
    }
    else if (s.mode == Mode::UpDown && len > 2)
    {
        // Up, then back down without repeating the top and bottom notes.
        const int up = len;
        for (int i = up - 2; i >= 1; --i)
        {
            sequence[static_cast<size_t> (len)] = sequence[static_cast<size_t> (i)];
            sequenceVelocity[static_cast<size_t> (len)] = sequenceVelocity[static_cast<size_t> (i)];
            ++len;
        }
    }
    return len;
}

int Arpeggiator::fire (double beatNow, const Settings& s, Event* out) noexcept
{
    int count = 0;
    if (! running)
        return 0;

    if (nextOff <= beatNow && nextOff <= nextOn && sounding >= 0)
    {
        out[count++] = { false, sounding, 0.0f };
        sounding = -1;
        nextOff = 1.0e300;
    }

    if (nextOn <= beatNow)
    {
        if (sounding >= 0) // gate longer than the step: tie into the next note
        {
            out[count++] = { false, sounding, 0.0f };
            sounding = -1;
        }

        const int len = buildSequence (s);
        if (len > 0)
        {
            int index;
            if (s.mode == Mode::Random)
                index = static_cast<int> (rng.nextFloat() * static_cast<float> (len)) % len;
            else
                index = position % len;
            position = (position + 1) % std::max (1, len);

            sounding = sequence[static_cast<size_t> (index)];
            out[count++] = { true, sounding, sequenceVelocity[static_cast<size_t> (index)] };
            const double stepLength = s.stepBeats * std::clamp (static_cast<double> (s.gate), 0.02, 1.0);
            nextOff = nextOn + stepLength;
        }

        ++step;
        nextOn = stepOnBeat (step, s);
    }

    if (nextOff <= beatNow && sounding >= 0 && count < 3)
    {
        out[count++] = { false, sounding, 0.0f };
        sounding = -1;
        nextOff = 1.0e300;
    }
    return count;
}

} // namespace augur
