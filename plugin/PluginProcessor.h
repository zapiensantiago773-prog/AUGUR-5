#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "Util/Smoother.h"
#include "Voice/MonoTestVoice.h"

class Augur5Processor final : public juce::AudioProcessor
{
public:
    // Bump when the saved-state layout changes; setStateInformation migrates older versions.
    static constexpr int stateVersion = 1;

    Augur5Processor();

    void prepareToPlay (double sampleRate, int maximumExpectedSamplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getParameters() noexcept { return parameters; }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void handleMidi (const juce::uint8* data, int numBytes) noexcept;
    void renderRange (juce::AudioBuffer<float>&, int start, int numSamples) noexcept;
    float currentGainTarget() const noexcept;

    juce::AudioProcessorValueTreeState parameters;
    std::atomic<float>* gainDb = nullptr;

    augur::LinearSmoother gainSmoother;
    augur::MonoTestVoice voice;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Augur5Processor)
};
