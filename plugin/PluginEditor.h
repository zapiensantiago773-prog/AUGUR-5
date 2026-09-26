#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "gui/AugurLookAndFeel.h"

class Augur5Processor;

namespace augur5::ui
{
class Canvas;
}

// The whole panel is laid out on a fixed 1536 x 1024 canvas (the design artboard) and scaled as one
// piece, so every size, gap and font matches the mockup at any window size.
class Augur5Editor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    static constexpr int designWidth = 1536;
    static constexpr int designHeight = 1024;

    explicit Augur5Editor (Augur5Processor&);
    ~Augur5Editor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

    void setScale (float scale);

private:
    void timerCallback() override;
    void showBrowserMenu();
    void showSettingsMenu();
    void showSaveDialog();

    Augur5Processor& processor;
    augur5::ui::AugurLookAndFeel lookAndFeel;
    std::unique_ptr<augur5::ui::Canvas> canvas;
    juce::TooltipWindow tooltips { this, 700 };
    std::unique_ptr<juce::AlertWindow> saveDialog;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Augur5Editor)
};
