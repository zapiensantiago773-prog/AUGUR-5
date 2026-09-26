#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <functional>

namespace augur5
{

// Factory bank (defined in code, so it can never go missing) plus user presets stored as XML files in
// <user app data>/TONAL LAB/AUGUR-5/Presets. Loading sets every parameter through the host-notifying path, so
// automation, undo and the UI all follow.
class PresetManager
{
public:
    static constexpr int formatVersion = 1;

    explicit PresetManager (juce::AudioProcessorValueTreeState& state);

    int getNumFactoryPresets() const noexcept;
    // "Preset: id" for every factory setting that names no parameter (typo guard for the audit).
    juce::StringArray findUnknownFactoryIds() const;
    static bool isGlobalSetting (const juce::String& id);
    juce::String getFactoryName (int index) const;
    juce::String getFactoryCategory (int index) const;
    int findFactory (const juce::String& name) const;
    void loadFactory (int index);

    // All user presets, including installed expansion packs (sub-folders), sorted by path.
    juce::Array<juce::File> getUserPresets() const;
    // Installs an expansion pack (.zip of .augur5 presets in folders) into the user folder.
    // Returns the number of presets installed, or -1 if the file is not a valid pack.
    static int installPack (const juce::File& zip);
    // Copies every .augur5 preset found in a folder (and its sub-folders) into the user folder, keeping
    // the folder structure under the folder's own name. Returns the number of presets added.
    static int importFolder (const juce::File& folder);
    void loadUser (const juce::File& file);
    bool saveUser (const juce::String& name);
    static juce::File getUserFolder();

    // Steps through factory presets followed by user presets.
    void next();
    void previous();

    juce::String getCurrentName() const { return currentName; }
    std::function<void()> onPresetLoaded; // message thread, after every factory/user preset load
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
