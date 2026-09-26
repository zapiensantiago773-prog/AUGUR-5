#include "Voice/MonoTestVoice.h"

#include <cmath>

namespace augur
{

double MonoTestVoice::noteToHz (double note) noexcept
{
    return 440.0 * std::exp2 ((note - 69.0) / 12.0);
}

void MonoTestVoice::prepare (double sampleRate) noexcept
{
    osc.prepare (sampleRate);
    amp.reset (sampleRate, gateRampSeconds, 0.0f);
    reset();
}

void MonoTestVoice::reset() noexcept
{
    numHeld = 0;
    currentNote = -1;
    amp.snapTo (0.0f);
    osc.resetPhase();
}

void MonoTestVoice::noteOn (int note, float velocity) noexcept
{
    removeHeld (note);

    if (numHeld == maxHeldNotes) // drop the oldest held note
    {
        for (int i = 1; i < numHeld; ++i)
            held[static_cast<size_t> (i - 1)] = held[static_cast<size_t> (i)];
        --numHeld;
    }

    held[static_cast<size_t> (numHeld++)] = note;

    if (! isActive())
        osc.resetPhase(); // deterministic start from silence; legato keeps phase continuous

    currentNote = note;
    osc.setFrequency (noteToHz (note));
    amp.setTarget (velocity);
}

void MonoTestVoice::noteOff (int note) noexcept
{
    removeHeld (note);

    if (numHeld == 0)
    {
        amp.setTarget (0.0f); // keep pitch during the release ramp
        return;
    }

    const int last = held[static_cast<size_t> (numHeld - 1)];
    if (last != currentNote)
    {
        currentNote = last;
        osc.setFrequency (noteToHz (last));
    }
}

void MonoTestVoice::allNotesOff() noexcept
{
    numHeld = 0;
    amp.setTarget (0.0f);
}

void MonoTestVoice::render (float* out, int numSamples) noexcept
{
    if (! isActive())
    {
        for (int i = 0; i < numSamples; ++i)
            out[i] = 0.0f;
        return;
    }

    for (int i = 0; i < numSamples; ++i)
        out[i] = osc.process() * amp.next();
}

void MonoTestVoice::removeHeld (int note) noexcept
{
    int write = 0;
    for (int read = 0; read < numHeld; ++read)
        if (held[static_cast<size_t> (read)] != note)
            held[static_cast<size_t> (write++)] = held[static_cast<size_t> (read)];
    numHeld = write;
}

} // namespace augur
