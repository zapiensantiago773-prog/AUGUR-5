#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>
#include <atomic>
#include <vector>

namespace augur5
{

// AUGURY: performance tools that work on the whole sound (owned by the processor so the corners survive closing the
// editor and are saved with the session).
//
//  * MORPH: four sounds captured into the corners of an XY field. Moving the point blends them: continuous
//    parameters by bilinear weights in their normalised (skewed) range, so a cutoff glides in octaves, not Hz;
//    switches and choices follow the corner with the largest weight. Empty corners drop out of the blend.
//    The voice structure (VOICES, UNISON, POLY / DUO, ARP, LATCH) is not flown: switching it re-assigns the voices
//    and would cut the notes being held. It stays as played; RECALL (a whole new sound) applies the corner's.
//  * The point is two parameters, MORPH X and MORPH Y, so a host can automate the flight and a MIDI controller can
//    fly it. Dragging the bird writes the blend into the parameters at once (one gesture, one undo step). When the
//    host or a controller moves MORPH X / Y, the audio thread plays the blend from that very block (offline renders
//    included) and the message thread writes it into the parameters a moment later (mirror()), so the panel shows
//    it and the session keeps it. Undo, a session or a preset move the bird without a morph (ScopedNoMorph): they
//    restore the sound and the bird together.
//  * OMEN: a reproducible variation of the current sound. Every unlocked group moves by a seeded random walk
//    whose size is AMOUNT; guards keep the result playable (D-058).
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

    // The editor's drag: MORPH X / Y and the blend, written at once as one gesture. Message thread.
    void beginMorph();
    void setPosition (juce::Point<float> p);
    void endMorph();
    juce::Point<float> getPosition() const; // MORPH X / Y
    void placeBird (juce::Point<float> p);  // MORPH X / Y without a morph (a scene's saved position)
    std::array<float, numCorners> weights() const; // of the filled corners, summing to 1

    // Message thread, a few times a second: MORPH X / Y moved by the host or a controller -> the blend into the
    // parameters (the audio has been playing it since the move).
    void mirror();

    // Audio thread: the values to play instead of the parameters while MORPH X / Y have moved and the parameters
    // do not hold that blend yet (binding slots, plain values), or count 0. No allocation, no waiting: when the
    // corners are being changed at that instant, the previous blend is played for one more block.
    struct Values
    {
        const int* slots = nullptr;
        const float* plain = nullptr;
        int count = 0;
    };
    Values audioOverride() noexcept;
    // The engine binding's slot of each morphable parameter (-1 = not read by the engine). Message thread, once.
    void setBindingSlots (const std::function<int (const juce::String&)>& slotOf);
    juce::StringArray unboundParameters() const; // morphable parameters the engine binding does not read (checks)

    // Moves the bird without a morph while it lives (undo / redo, a session or preset restoring sound and bird).
    class ScopedNoMorph
    {
    public:
        explicit ScopedNoMorph (AuguryModel& m) noexcept;
        ~ScopedNoMorph();
        ScopedNoMorph (const ScopedNoMorph&) = delete;
        ScopedNoMorph& operator= (const ScopedNoMorph&) = delete;

    private:
        AuguryModel& model;
    };

    // OMEN. Returns the seed used (a new one when seed == 0).
    juce::uint32 castOmen (float amount, juce::uint32 lockedGroups, juce::uint32 seed = 0);
    juce::uint32 getLastSeed() const { return lastSeed; }

    // Changes whenever the corners change (capture, clear, a session or a preset's scene loaded): the page polls it.
    int getRevision() const noexcept { return revision.load(); }
    static bool isVoiceStructure (const juce::String& id); // kept out of the morph (see above)

    // RECALL and OMEN replace the whole sound: the processor wraps them as a preset load (fade out / in).
    std::function<void (const std::function<void()>&)> soundChange;

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
    static std::array<float, numCorners> weightsAt (float x, float y, const std::array<bool, numCorners>& filled) noexcept;
    // The blend at (x, y), normalised, into out (one per morphable parameter; voice structure left untouched).
    void blendInto (float x, float y, const std::array<bool, numCorners>& filled, const float* const* values, float* out) const noexcept;
    void setNormalised (size_t index, float v, bool gesture);
    void applyBlend (float x, float y, bool gesture); // message thread
    void setBird (juce::Point<float> p);              // MORPH X / Y, notifying the host
    void publish();                                   // corners -> the audio thread's copy
    void loadCorners (const juce::ValueTree& tree);   // with lock held
    void markMirrored() noexcept;                     // the parameters hold the sound for the current X / Y

    juce::AudioProcessorValueTreeState& state;
    std::vector<juce::RangedAudioParameter*> params; // morphable parameters (sound, not instrument settings)
    std::vector<bool> discrete, structure;
    std::array<Corner, numCorners> corners;
    juce::RangedAudioParameter *xParam = nullptr, *yParam = nullptr;
    std::atomic<float> *xRaw = nullptr, *yRaw = nullptr;
    juce::uint32 lastSeed = 0;
    bool inGesture = false;
    std::atomic<int> revision { 0 };
    // Hosts may save or restore the session from another thread than the editor's (never the audio thread).
    mutable juce::CriticalSection lock;

    // Morph by automation: the audio thread's side.
    std::atomic<float> mirroredX { 0.5f }, mirroredY { 0.5f }; // X / Y whose sound the parameters hold
    std::atomic<int> noMorph { 0 };
    juce::SpinLock rtLock;                                     // guards the copy below (the audio thread only tries)
    std::vector<float> rtValues;                               // numCorners x params, normalised
    std::array<bool, numCorners> rtFilled {};
    std::vector<int> bindingSlot;
    std::vector<float> rtNorm, rtPlain;                        // audio thread scratch and the last blend
    std::vector<int> rtSlots;
    int rtCount = 0, rtRevision = -1;
    float rtX = -1.0f, rtY = -1.0f;
};

} // namespace augur5
