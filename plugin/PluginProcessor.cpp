#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Parameters.h"

namespace params = augur5::params;

static constexpr float minGainDb = -60.0f; // bottom of the range means -inf

Augur5Processor::Augur5Processor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      parameters (*this, nullptr, "AUGUR5", createParameterLayout())
{
    gainDb = parameters.getRawParameterValue (params::amp_level);
}

juce::AudioProcessorValueTreeState::ParameterLayout Augur5Processor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { params::amp_level, 1 },
        "Level",
        juce::NormalisableRange<float> (minGainDb, 6.0f, 0.1f),
        -12.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));
    return layout;
}

float Augur5Processor::currentGainTarget() const noexcept
{
    return juce::Decibels::decibelsToGain (gainDb->load (std::memory_order_relaxed), minGainDb);
}

bool Augur5Processor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

void Augur5Processor::prepareToPlay (double sampleRate, int)
{
    voice.prepare (sampleRate);
    gainSmoother.reset (sampleRate, 0.02, currentGainTarget());
}

void Augur5Processor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    if (numChannels == 0)
        return;

    gainSmoother.setTarget (currentGainTarget());

    // Sample-accurate MIDI: render up to each event, then apply it.
    // Raw bytes are parsed directly so no MidiMessage (and no allocation) is built.
    int position = 0;
    for (const auto metadata : midi)
    {
        const int eventPosition = juce::jlimit (0, numSamples, metadata.samplePosition);
        if (eventPosition > position)
        {
            renderRange (buffer, position, eventPosition - position);
            position = eventPosition;
        }
        handleMidi (metadata.data, metadata.numBytes);
    }
    if (position < numSamples)
        renderRange (buffer, position, numSamples - position);

    for (int ch = 1; ch < numChannels; ++ch)
        buffer.copyFrom (ch, 0, buffer, 0, 0, numSamples);
}

void Augur5Processor::renderRange (juce::AudioBuffer<float>& buffer, int start, int numSamples) noexcept
{
    float* out = buffer.getWritePointer (0, start);
    voice.render (out, numSamples);
    for (int i = 0; i < numSamples; ++i)
        out[i] *= gainSmoother.next();
}

void Augur5Processor::handleMidi (const juce::uint8* data, int numBytes) noexcept
{
    if (numBytes < 3)
        return;

    const int status = data[0] & 0xF0;
    const int data1 = data[1];
    const int data2 = data[2];

    if (status == 0x90 && data2 > 0)
        voice.noteOn (data1, static_cast<float> (data2) / 127.0f);
    else if (status == 0x80 || status == 0x90)
        voice.noteOff (data1);
    else if (status == 0xB0 && (data1 == 120 || data1 == 123)) // All Sound Off / All Notes Off
        voice.allNotesOff();
}

void Augur5Processor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    state.setProperty ("stateVersion", stateVersion, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void Augur5Processor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (parameters.state.getType()))
            parameters.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* Augur5Processor::createEditor()
{
    return new Augur5Editor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new Augur5Processor();
}
