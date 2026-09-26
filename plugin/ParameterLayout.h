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

    // Audio thread. With VINTAGE 7-BIT KNOBS on, the Prophet-5 Rev 3 panel knobs are digitised like the
    // original (128 steps, a knob must move two steps to register), so sweeps step audibly.
    void fill (augur::SynthParams& p) noexcept;

private:
    struct Raw;
    std::unique_ptr<Raw> raw;
};

} // namespace augur5
