#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "Parameters.h"

Augur5Editor::Augur5Editor (Augur5Processor& p)
    : AudioProcessorEditor (p),
      gainAttachment (p.getParameters(), augur5::params::amp_level, gainSlider)
{
    gainSlider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    gainSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 20);
    addAndMakeVisible (gainSlider);

    gainLabel.setText ("LEVEL", juce::dontSendNotification);
    gainLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (gainLabel);

    setSize (420, 260);
}

void Augur5Editor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1c1b1a));

    auto header = getLocalBounds().removeFromTop (70).reduced (20, 12);
    g.setColour (juce::Colour (0xffe8e2d6));
    g.setFont (juce::FontOptions (30.0f, juce::Font::bold));
    g.drawText ("AUGUR-5", header, juce::Justification::centredLeft);

    g.setColour (juce::Colour (0xffc8792e));
    g.setFont (juce::FontOptions (18.0f));
    g.drawText ("\"3340\"", header, juce::Justification::centredRight);
}

void Augur5Editor::resized()
{
    auto area = getLocalBounds().withTrimmedTop (70).reduced (20);
    auto knob = area.withSizeKeepingCentre (120, area.getHeight());
    gainLabel.setBounds (knob.removeFromTop (20));
    gainSlider.setBounds (knob);
}
