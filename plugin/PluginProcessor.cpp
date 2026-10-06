#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "LegacyEffects.h"
#include "Parameters.h"

Augur5Processor::Augur5Processor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      parameters (*this, &undoManager, "AUGUR5", augur5::createParameterLayout()),
      binding (parameters),
      presets (parameters),
      augury (parameters),
      engine (std::make_unique<augur::SynthEngine>())
{
    for (size_t i = 0; i < fxOrder.size(); ++i)
        fxOrder[i].store (static_cast<int> (i));
    // User presets carry the rack order; factory sounds use the default one.
    presets.saveFxOrder = [this] { return fxOrderToString(); };
    presets.loadFxOrder = [this] (const juce::String& text) { fxOrderFromString (text); };
    // A fresh instance opens on the showcase patch, not a bare init sound.
    presets.loadFactory (presets.findFactory ("Warm Horizon"));
    presets.onPresetLoaded = [this] { warmUpEngine(); };
    undoManager.clearUndoHistory();
    qualityParam = parameters.getRawParameterValue (augur5::params::quality);
    offlineQualityParam = parameters.getRawParameterValue (augur5::params::offline_quality);
}

Augur5Processor::~Augur5Processor()
{
    cancelPendingUpdate();
}

int Augur5Processor::wantedOversampling (bool offline) const noexcept
{
    int q = juce::roundToInt (qualityParam->load (std::memory_order_relaxed));
    if (offline && juce::roundToInt (offlineQualityParam->load (std::memory_order_relaxed)) == 1)
        q = 2; // DIVINE for the render
    return augur::SynthEngine::oversamplingFor (q, getSampleRate() > 0.0 ? getSampleRate() : 48000.0);
}

void Augur5Processor::configureEngine (double sampleRate, int oversampling)
{
    engine->prepare (sampleRate, augur::SynthEngine::defaultUnitSeed, oversampling);
    fillSnapshot();
    engine->setParams (snapshot);
    engine->warmUp();
    setLatencySamples (engine->getLatencySamples());
}

void Augur5Processor::handleAsyncUpdate()
{
    if (! prepared)
        return;
    const int wanted = wantedOversampling (isNonRealtime());
    if (wanted == engine->getOversampling())
        return;
    suspendProcessing (true);
    configureEngine (getSampleRate(), wanted);
    suspendProcessing (false);
}


bool Augur5Processor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

void Augur5Processor::prepareToPlay (double sampleRate, int)
{
    // Latency (BLEP kernels + decimators) is reported so hosts can align us with other tracks.
    int q = juce::roundToInt (qualityParam->load());
    if (isNonRealtime() && juce::roundToInt (offlineQualityParam->load()) == 1)
        q = 2;
    configureEngine (sampleRate, augur::SynthEngine::oversamplingFor (q, sampleRate));
    prepared = true;
}

void Augur5Processor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    if (numChannels == 0)
        return;

    // Quality changed (or the host switched between realtime and offline rendering).
    if (const int wanted = wantedOversampling (isNonRealtime()); wanted != engine->getOversampling())
    {
        if (isNonRealtime())
            configureEngine (getSampleRate(), wanted); // offline: no deadline, switch right here
        else
            triggerAsyncUpdate();
    }

    fillSnapshot();
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

    // The on-screen keyboard and wheels play at the start of the block.
    uiMidi.drain ([this] (const juce::uint8* data, int numBytes) { handleMidi (data, numBytes); });

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

    audioTap.push (buffer.getArrayOfReadPointers(), juce::jmin (2, numChannels), numSamples); // the horizon
}

void Augur5Processor::fillSnapshot() noexcept
{
    binding.fill (snapshot);
    for (size_t i = 0; i < fxOrder.size(); ++i)
        snapshot.fx.order[i] = fxOrder[i].load (std::memory_order_relaxed);
}

bool Augur5Processor::isGateShown() const noexcept
{
    if (sustainShown.load (std::memory_order_relaxed))
        return true;
    for (const auto& k : keyHeld)
        if (k.load (std::memory_order_relaxed))
            return true;
    return false;
}

std::array<int, 8> Augur5Processor::getFxOrder() const noexcept
{
    std::array<int, 8> order {};
    for (size_t i = 0; i < order.size(); ++i)
        order[i] = fxOrder[i].load (std::memory_order_relaxed);
    return order;
}

void Augur5Processor::setFxOrder (const std::array<int, 8>& order) noexcept
{
    // Only a permutation of 0..7 reaches the audio thread.
    std::array<bool, 8> seen {};
    for (const int v : order)
    {
        if (v < 0 || v > 7 || seen[static_cast<size_t> (v)])
            return;
        seen[static_cast<size_t> (v)] = true;
    }
    for (size_t i = 0; i < order.size(); ++i)
        fxOrder[i].store (order[i], std::memory_order_relaxed);
}

juce::String Augur5Processor::fxOrderToString() const
{
    juce::StringArray parts;
    for (const int v : getFxOrder())
        parts.add (juce::String (v));
    return parts.joinIntoString (" ");
}

void Augur5Processor::fxOrderFromString (const juce::String& text)
{
    const auto parts = juce::StringArray::fromTokens (text, " ", "");
    std::array<int, 8> order { 0, 1, 2, 3, 4, 5, 6, 7 };
    setFxOrder (order);
    if (parts.size() != 8)
        return;
    for (size_t i = 0; i < order.size(); ++i)
        order[i] = parts[static_cast<int> (i)].getIntValue();
    setFxOrder (order); // ignored unless it is a permutation of the eight units
}

void Augur5Processor::handleMidi (const juce::uint8* data, int numBytes) noexcept
{
    if (numBytes < 2)
        return;

    const int status = data[0] & 0xF0;
    const int d1 = data[1];
    const int d2 = numBytes > 2 ? data[2] : 0;

    const auto releaseAllKeys = [this] {
        for (auto& k : keyHeld)
            k.store (false, std::memory_order_relaxed);
    };
    switch (status)
    {
        case 0x90:
            keyHeld[static_cast<size_t> (d1 & 127)].store (d2 > 0, std::memory_order_relaxed);
            if (d2 > 0)
                engine->noteOn (d1, static_cast<float> (d2) / 127.0f);
            else
                engine->noteOff (d1);
            break;
        case 0x80:
            keyHeld[static_cast<size_t> (d1 & 127)].store (false, std::memory_order_relaxed);
            engine->noteOff (d1);
            break;
        case 0xA0: engine->setPolyPressure (d1, static_cast<float> (d2) / 127.0f); break;
        case 0xD0:
            pressureShown.store (static_cast<float> (d1) / 127.0f, std::memory_order_relaxed);
            engine->setChannelPressure (static_cast<float> (d1) / 127.0f);
            break;
        case 0xE0:
        {
            const float bend = static_cast<float> ((d2 << 7 | d1) - 8192) / 8192.0f;
            bendShown.store (bend, std::memory_order_relaxed);
            engine->setPitchBend (bend);
            break;
        }
        case 0xB0:
            switch (d1)
            {
                case 1:
                    modShown.store (static_cast<float> (d2) / 127.0f, std::memory_order_relaxed);
                    engine->setModWheel (static_cast<float> (d2) / 127.0f);
                    break;
                case 64:
                    sustainShown.store (d2 >= 64, std::memory_order_relaxed);
                    engine->setSustain (d2 >= 64);
                    break;
                case 120:
                    releaseAllKeys();
                    engine->allSoundOff();
                    break;
                case 121:
                    modShown.store (0.0f, std::memory_order_relaxed);
                    bendShown.store (0.0f, std::memory_order_relaxed);
                    sustainShown.store (false, std::memory_order_relaxed);
                    engine->setModWheel (0.0f);
                    engine->setPitchBend (0.0f);
                    engine->setSustain (false);
                    break;
                case 123:
                    releaseAllKeys();
                    engine->allNotesOff();
                    break;
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
    state.setProperty ("uiScaleChosen", uiScaleChosen, nullptr);
    state.setProperty ("fxOrder", fxOrderToString(), nullptr);
    for (auto old = state.getChildWithName ("AUGURY"); old.isValid(); old = state.getChildWithName ("AUGURY"))
        state.removeChild (old, nullptr);
    state.appendChild (augury.toTree(), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void Augur5Processor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        if (xml->hasTagName (parameters.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            // Sessions saved with 1.0 carry its own effects (chorus_*, phaser_*, echo_*, reverb_*): convert them.
            augur5::Settings legacy;
            for (const auto& child : tree)
                if (child.hasProperty ("id") && augur5::isLegacyEffectId (child["id"].toString()))
                    legacy.push_back ({ child["id"].toString(), static_cast<float> (child["value"]) });
            // Plugin state that is not a parameter: rack order and the AUGURY corners.
            fxOrderFromString (tree.getProperty ("fxOrder").toString());
            augury.fromTree (tree.getChildWithName ("AUGURY"));
            for (auto old = tree.getChildWithName ("AUGURY"); old.isValid(); old = tree.getChildWithName ("AUGURY"))
                tree.removeChild (old, nullptr);
            uiScaleChosen = static_cast<bool> (tree.getProperty ("uiScaleChosen", false));
            parameters.replaceState (tree);
            if (! legacy.empty())
            {
                augur5::convertLegacyEffects (legacy);
                for (const auto& [id, value] : legacy)
                    if (auto* p = parameters.getParameter (id))
                        p->setValueNotifyingHost (p->convertTo0to1 (value));
            }
            presets.setCurrentName (tree.getProperty ("presetName", "Init").toString());
            uiScale = static_cast<float> (tree.getProperty ("uiScale", 0.75));
            undoManager.clearUndoHistory();
            warmUpEngine();
        }
    }
}

void Augur5Processor::warmUpEngine()
{
    if (! prepared)
        return; // prepareToPlay will do it
    suspendProcessing (true);
    fillSnapshot();
    engine->setParams (snapshot);
    engine->warmUp();
    suspendProcessing (false);
}

juce::AudioProcessorEditor* Augur5Processor::createEditor()
{
    return new Augur5Editor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new Augur5Processor();
}
