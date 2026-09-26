#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

class Augur5Processor;

// Placeholder editor for Phase 0. The real panel is designed in Phase 8.
class Augur5Editor final : public juce::AudioProcessorEditor
{
public:
    explicit Augur5Editor (Augur5Processor&);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    juce::Slider gainSlider;
    juce::Label gainLabel;
    juce::AudioProcessorValueTreeState::SliderAttachment gainAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Augur5Editor)
};
