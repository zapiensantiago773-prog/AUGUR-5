#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>
#include <vector>

namespace augur5
{

// AUGURY: performance tools that work on the whole sound (message thread only; owned by the processor so the
// corners survive closing the editor and are saved with the session).
//
//  * MORPH: four sounds captured into the corners of an XY field. Moving the point blends them: continuous
//    parameters by bilinear weights in their normalised (skewed) range, so a cutoff glides in octaves, not Hz;
//    switches and choices follow the corner with the largest weight. Empty corners drop out of the blend.
//  * OMEN: a reproducible variation of the current sound. Every unlocked group moves by a seeded random walk
//    whose size is AMOUNT; a few guards keep the result playable (cutoff floor, attack ceiling, resonance below
//    self-oscillation, effects mixes within reason, at least one audible oscillator wave).
//
// The sound engine itself (oscillators, filters, envelopes) is untouched: AUGURY only moves parameters, through
// the host-notifying path, so automation, undo and presets see every change.
class AuguryModel
{
public:
    static constexpr int numCorners = 4;
    enum Group
    {
        oscillators = 0,
        mixer,
        filter,
        envelopes,
        modulation,
        effects,
        numGroups
    };
    static const char* groupName (int g);

    explicit AuguryModel (juce::AudioProcessorValueTreeState& state);

    // Corners: 0 = top left, 1 = top right, 2 = bottom left, 3 = bottom right.
    void capture (int corner, const juce::String& soundName);
    void clear (int corner);
    bool isFilled (int corner) const;
    juce::String getName (int corner) const;
    int filledCount() const;
    void recall (int corner); // the corner's sound exactly

    // Morph point in 0..1 x 0..1; applies the blend when two or more corners hold a sound.
    void beginMorph();
    void setPosition (juce::Point<float> p);
    void endMorph();
    juce::Point<float> getPosition() const { return position; }
    std::array<float, numCorners> weights() const; // of the filled corners, summing to 1

    // OMEN. Returns the seed used (a new one when seed == 0).
    juce::uint32 castOmen (float amount, juce::uint32 lockedGroups, juce::uint32 seed = 0);
    juce::uint32 getLastSeed() const { return lastSeed; }

    juce::ValueTree toTree() const;
    void fromTree (const juce::ValueTree& tree);

private:
    struct Corner
    {
        bool filled = false;
        juce::String name;
        std::vector<float> values; // normalised, one per morphable parameter
    };
    static int groupOf (const juce::String& id);
    void setNormalised (size_t index, float v);

    juce::AudioProcessorValueTreeState& state;
    std::vector<juce::RangedAudioParameter*> params; // morphable parameters (sound, not instrument settings)
    std::vector<bool> discrete;
    std::array<Corner, numCorners> corners;
    juce::Point<float> position { 0.5f, 0.5f };
    juce::uint32 lastSeed = 0;
    bool inGesture = false;
    // Hosts may save or restore the session from another thread than the editor's (never the audio thread).
    mutable juce::CriticalSection lock;
};

} // namespace augur5
