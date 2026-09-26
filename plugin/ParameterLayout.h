#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "Engine/SynthParams.h"

namespace augur5
{

// Builds every APVTS parameter (ranges, defaults, value text) from the IDs in Parameters.h.
juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

// Choice labels shared by the layout and the editor.
const juce::StringArray& matrixSourceNames();
const juce::StringArray& matrixDestNames();

// Cached raw-value pointers; turns the APVTS into an engine SynthParams snapshot without allocating.
class ParameterBinding
{
public:
    explicit ParameterBinding (juce::AudioProcessorValueTreeState& state);
    ~ParameterBinding();
    void fill (augur::SynthParams& p) const noexcept;

private:
    struct Raw;
    std::unique_ptr<Raw> raw;
};

} // namespace augur5
