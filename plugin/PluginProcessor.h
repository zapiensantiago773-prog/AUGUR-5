#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "AudioTap.h"
#include "AuguryModel.h"
#include "Engine/SynthEngine.h"
#include "ParameterLayout.h"
#include "Presets.h"

class Augur5Processor final : public juce::AudioProcessor,
                              private juce::AsyncUpdater
{
public:
    // Bump when the saved-state layout changes; setStateInformation migrates older versions.
    // 2: effects rack order ("fxOrder") and the AUGURY corners.
    static constexpr int stateVersion = 2;

    Augur5Processor();
    ~Augur5Processor() override;

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
    double getTailLengthSeconds() const override { return 12.0; } // delay/reverb tails, so offline renders keep them

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getParameters() noexcept { return parameters; }
    juce::UndoManager& getUndoManager() noexcept { return undoManager; }
    augur5::PresetManager& getPresets() noexcept { return presets; }
    float getVoiceLevel (int v) const noexcept { return engine->getVoiceLevel (v); }
    const augur::SynthEngine& getEngine() const noexcept { return *engine; }
    augur5::AuguryModel& getAugury() noexcept { return augury; }
    augur5::AudioTap& getAudioTap() noexcept { return audioTap; }

    // On-screen keyboard and wheels: queued lock-free, played at the start of the next block.
    void sendUiMidi (int status, int data1, int data2) noexcept
    {
        uiMidi.push (static_cast<juce::uint8> (status), static_cast<juce::uint8> (data1), static_cast<juce::uint8> (data2));
    }
    // What the performance strip shows (audio -> GUI, relaxed).
    float getBendShown() const noexcept { return bendShown.load (std::memory_order_relaxed); }
    float getModWheelShown() const noexcept { return modShown.load (std::memory_order_relaxed); }
    float getPressureShown() const noexcept { return pressureShown.load (std::memory_order_relaxed); }
    bool isKeyHeld (int note) const noexcept { return keyHeld[static_cast<size_t> (note & 127)].load (std::memory_order_relaxed); }
    bool isGateShown() const noexcept;

    // Effects rack order (a permutation of the eight units), saved with the session and in user presets.
    std::array<int, 8> getFxOrder() const noexcept;
    void setFxOrder (const std::array<int, 8>& order) noexcept;
    juce::String fxOrderToString() const;
    void fxOrderFromString (const juce::String& text); // empty or invalid = the default order

    // Editor size, persisted with the session.
    float getUiScale() const noexcept { return uiScale; }
    void setUiScale (float s) noexcept { uiScale = s; }
    bool isUiScaleChosen() const noexcept { return uiScaleChosen; }
    void setUiScaleChosen (bool chosen) noexcept { uiScaleChosen = chosen; }

    // After a preset or session change: charge the coupling capacitors for the new sound (~2 ms, with
    // processing briefly suspended), so the first note carries no DC step.
    void warmUpEngine();

private:
    void handleMidi (const juce::uint8* data, int numBytes) noexcept;
    void fillSnapshot() noexcept; // parameters + rack order -> snapshot

    // Quality (oversampling) changes rebuild the voices: done off the audio thread with processing
    // suspended, or directly while the host renders offline (no deadline there).
    int wantedOversampling (bool offline) const noexcept;
    void configureEngine (double sampleRate, int oversampling);
    void handleAsyncUpdate() override;
    std::atomic<float>* qualityParam = nullptr;
    std::atomic<float>* offlineQualityParam = nullptr;

    juce::UndoManager undoManager;
    juce::AudioProcessorValueTreeState parameters;
    augur5::ParameterBinding binding;
    augur5::PresetManager presets;
    augur5::AuguryModel augury;
    augur5::AudioTap audioTap;
    augur5::UiMidiQueue uiMidi;
    std::array<std::atomic<int>, 8> fxOrder;
    std::atomic<float> bendShown { 0.0f }, modShown { 0.0f }, pressureShown { 0.0f };
    std::atomic<bool> sustainShown { false };
    std::array<std::atomic<bool>, 128> keyHeld {};

    std::unique_ptr<augur::SynthEngine> engine;
    augur::SynthParams snapshot;
    float uiScale = 0.65f;
    bool uiScaleChosen = false;
    bool prepared = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Augur5Processor)
};
