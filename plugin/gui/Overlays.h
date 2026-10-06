#pragma once

#include "../Presets.h"
#include "Widgets.h"

#include <functional>
#include <memory>
#include <vector>

namespace augur5::ui
{

// Full-canvas overlay panels. They live inside the scaled canvas (design coordinates), so they stay sharp and
// proportional at every window size; a click on the veiled backdrop closes them.
class Overlay : public juce::Component
{
public:
    void open();
    void close();
    std::function<void()> onClosed;

protected:
    juce::Rectangle<int> panel; // the card, in canvas coordinates
    void paintCard (juce::Graphics& g, const juce::String& title) const;
    void mouseDown (const juce::MouseEvent& e) override;
    bool keyPressed (const juce::KeyPress& key) override;
    virtual void opened() {}
};

// PRESETS: collections (FACTORY, each installed pack, USER) and their categories on the left, a searchable list on
// the right. Click = audition, double-click / Enter = load and close, Up / Down = step and audition, Esc = close.
class PresetBrowser final : public Overlay
{
public:
    struct Entry
    {
        juce::String name, collection, category;
        int factory = -1; // factory index, or -1 for a file
        juce::File file;
    };
    struct Actions
    {
        std::function<void()> save, install, addFolder;
    };

    PresetBrowser (PresetManager& presets, Actions actions);
    ~PresetBrowser() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void openOnCategory (const juce::String& category); // FACTORY, that category

private:
    struct ListModel;
    void opened() override;
    void rescan();
    void refreshCategories();
    void refreshPresets();
    void load (int row, bool andClose);

    PresetManager& presets;
    Actions actions;
    std::vector<Entry> entries;
    std::vector<int> visible;
    juce::StringArray collections, categories;
    int collection = 0, category = 0;
    juce::String pendingCategory;
    std::unique_ptr<ListModel> collectionModel, categoryModel, presetModel;
    juce::ListBox collectionList, categoryList, presetList;
    juce::TextEditor search;
    std::vector<std::unique_ptr<ActionButton>> buttons;
};

// SETTINGS: window size, sound quality, the circuit revision, display, presets, version.
class SettingsPanel final : public Overlay
{
public:
    struct Actions
    {
        std::function<float()> currentScale;
        std::function<void (float)> setScale; // a chosen size (remembered)
        std::function<void()> fitScale;       // largest size that fits the screen
        std::function<void()> save, install, addFolder, openFolder, refreshIds;
    };
    SettingsPanel (APVTS& state, Actions actions);
    void paint (juce::Graphics&) override;

private:
    void opened() override;
    APVTS& state;
    Actions actions;
    std::vector<std::unique_ptr<juce::Component>> owned;
    std::vector<std::pair<ActionButton*, float>> sizeButtons;
    ActionButton* idsButton = nullptr;
};

} // namespace augur5::ui
