#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "gui/AugurLookAndFeel.h"

class Augur5Processor;

namespace augur5::ui
{
class Canvas;
class PresetBrowser;
class SettingsPanel;
} // namespace augur5::ui

// The TONAL LAB series layout on a fixed 1536 x 1024 canvas, scaled as one piece (every size, gap and font stays
// exact at any window size): header with the preset bar and the tabs (MAIN, MOD, ARP, VOICE, FX, AUGURY), the tab
// page, and the performance strip with the AUGURY frequency horizon. MAIN holds the classic instrument's panel; the
// expansions live in the other tabs.
class Augur5Editor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    static constexpr int designWidth = 1536;
    static constexpr int designHeight = 1024;
    static constexpr int numTabs = 6;

    explicit Augur5Editor (Augur5Processor&);
    ~Augur5Editor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

    void setScale (float scale);
    float fitScale() const; // largest size that shows the whole panel on this screen
    void showTab (int tab); // 0 MAIN, 1 MOD, 2 ARP, 3 VOICE, 4 FX, 5 AUGURY
    void showOverlay (int which); // 0 presets, 1 settings
    void showEffect (int id);     // FX tab with that unit's editor (0..7 rack, 8 fuzz)

private:
    void timerCallback() override;
    void showSaveDialog();
    void showInstallPackDialog();
    void showAddFolderDialog();

    Augur5Processor& processor;
    augur5::ui::AugurLookAndFeel lookAndFeel;
    std::unique_ptr<augur5::ui::Canvas> canvas;
    std::unique_ptr<augur5::ui::PresetBrowser> browser;
    std::unique_ptr<augur5::ui::SettingsPanel> settingsPanel;
    juce::TooltipWindow tooltips { this, 700 };
    std::unique_ptr<juce::AlertWindow> saveDialog;
    std::unique_ptr<juce::FileChooser> packChooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Augur5Editor)
};
