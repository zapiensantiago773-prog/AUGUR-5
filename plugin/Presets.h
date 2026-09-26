#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace augur5
{

// Factory bank (defined in code, so it can never go missing) plus user presets stored as XML files in
// <user app data>/AUGUR-5/Presets. Loading sets every parameter through the host-notifying path, so
// automation, undo and the UI all follow.
class PresetManager
{
public:
    static constexpr int formatVersion = 1;

    explicit PresetManager (juce::AudioProcessorValueTreeState& state);

    int getNumFactoryPresets() const noexcept;
    juce::String getFactoryName (int index) const;
    void loadFactory (int index);

    juce::Array<juce::File> getUserPresets() const;
    void loadUser (const juce::File& file);
    bool saveUser (const juce::String& name);
    static juce::File getUserFolder();

    // Steps through factory presets followed by user presets.
    void next();
    void previous();

    juce::String getCurrentName() const { return currentName; }
    void setCurrentName (const juce::String& n) { currentName = n; }

private:
    void resetToDefaults();
    void setParam (const juce::String& id, float value);
    int currentFlatIndex() const;
    void loadFlat (int index);

    juce::AudioProcessorValueTreeState& state;
    juce::String currentName { "Init" };
    int currentFactory = 0;
    juce::File currentUserFile;
};

} // namespace augur5
