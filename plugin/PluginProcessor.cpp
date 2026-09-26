#include "PluginProcessor.h"
#include "PluginEditor.h"

Augur5Processor::Augur5Processor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      parameters (*this, &undoManager, "AUGUR5", augur5::createParameterLayout()),
      binding (parameters),
      presets (parameters),
      engine (std::make_unique<augur::SynthEngine>())
{
    // A fresh instance opens on the showcase patch, not a bare init sound.
    presets.loadFactory (1);
    undoManager.clearUndoHistory();
}

Augur5Processor::~Augur5Processor() = default;

bool Augur5Processor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

void Augur5Processor::prepareToPlay (double sampleRate, int)
{
    engine->prepare (sampleRate);
    binding.fill (snapshot);
    engine->setParams (snapshot);

    // Oversampling decimator + BLEP delay, so hosts can align us with other tracks.
    const int latency = engine->getOversampling() == 2 ? (augur::HalfbandDecimator::centre + augur::Vco::latencySamples) / 2
                                                       : augur::Vco::latencySamples;
    setLatencySamples (latency);
}

void Augur5Processor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    if (numChannels == 0)
        return;

    binding.fill (snapshot);
    engine->setParams (snapshot);

    if (auto* ph = getPlayHead())
    {
        if (const auto pos = ph->getPosition())
            engine->setTransport (pos->getBpm().orFallback (120.0), pos->getIsPlaying(), pos->getPpqPosition().orFallback (0.0));
    }
    else
    {
        engine->setTransport (120.0, false, 0.0);
    }

    float* left = buffer.getWritePointer (0);
    float* right = numChannels > 1 ? buffer.getWritePointer (1) : nullptr;

    const auto render = [&] (int start, int count) {
        if (right != nullptr)
        {
            engine->process (left + start, right + start, count);
        }
        else
        {
            // Mono: process in small stack chunks, then fold to one channel.
            float tmp[256];
            for (int done = 0; done < count;)
            {
                const int n = juce::jmin (256, count - done);
                engine->process (left + start + done, tmp, n);
                for (int i = 0; i < n; ++i)
                    left[start + done + i] = 0.5f * (left[start + done + i] + tmp[i]);
                done += n;
            }
        }
    };

    // Sample-accurate MIDI: render up to each event, then apply it.
    int position = 0;
    for (const auto metadata : midi)
    {
        const int eventPosition = juce::jlimit (0, numSamples, metadata.samplePosition);
        if (eventPosition > position)
        {
            render (position, eventPosition - position);
            position = eventPosition;
        }
        handleMidi (metadata.data, metadata.numBytes);
    }
    if (position < numSamples)
        render (position, numSamples - position);

    for (int ch = 2; ch < numChannels; ++ch)
        buffer.clear (ch, 0, numSamples);
}

void Augur5Processor::handleMidi (const juce::uint8* data, int numBytes) noexcept
{
    if (numBytes < 2)
        return;

    const int status = data[0] & 0xF0;
    const int d1 = data[1];
    const int d2 = numBytes > 2 ? data[2] : 0;

    switch (status)
    {
        case 0x90:
            if (d2 > 0)
                engine->noteOn (d1, static_cast<float> (d2) / 127.0f);
            else
                engine->noteOff (d1);
            break;
        case 0x80: engine->noteOff (d1); break;
        case 0xA0: engine->setPolyPressure (d1, static_cast<float> (d2) / 127.0f); break;
        case 0xD0: engine->setChannelPressure (static_cast<float> (d1) / 127.0f); break;
        case 0xE0: engine->setPitchBend (static_cast<float> ((d2 << 7 | d1) - 8192) / 8192.0f); break;
        case 0xB0:
            switch (d1)
            {
                case 1:   engine->setModWheel (static_cast<float> (d2) / 127.0f); break;
                case 64:  engine->setSustain (d2 >= 64); break;
                case 120: engine->allSoundOff(); break;
                case 121: engine->setModWheel (0.0f); engine->setPitchBend (0.0f); engine->setSustain (false); break;
                case 123: engine->allNotesOff(); break;
                default: break;
            }
            break;
        default: break;
    }
}

void Augur5Processor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    state.setProperty ("stateVersion", stateVersion, nullptr);
    state.setProperty ("presetName", presets.getCurrentName(), nullptr);
    state.setProperty ("uiScale", uiScale, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void Augur5Processor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        if (xml->hasTagName (parameters.state.getType()))
        {
            const auto tree = juce::ValueTree::fromXml (*xml);
            parameters.replaceState (tree);
            presets.setCurrentName (tree.getProperty ("presetName", "Init").toString());
            uiScale = static_cast<float> (tree.getProperty ("uiScale", 0.75));
            undoManager.clearUndoHistory();
        }
    }
}

juce::AudioProcessorEditor* Augur5Processor::createEditor()
{
    return new Augur5Editor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new Augur5Processor();
}
