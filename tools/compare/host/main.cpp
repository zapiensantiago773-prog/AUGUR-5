// compare_host [--role pluck|keys|pad|lead|bass] [--root 60] <plugin.vst3> <outdir> <preset files...>
//
// Plays presets of any VST3 instrument the way augur_preset_audit --pack plays AUGUR's (same phrases, 48 kHz,
// velocity 100), so other instruments can be measured on the same terms (tools/compare/compare.py).
// Every preset file is applied as the plug-in's own state (PYTHIA 32 .pythia32 files are exactly that), a fresh
// engine is prepared, and a stereo 32-bit float WAV named after the preset is written to <outdir>.
#include <juce_audio_processors/juce_audio_processors.h>

#include <cstdio>
#include <cstring>

namespace
{
constexpr double sr = 48000.0;
constexpr int block = 256;

struct Event
{
    double time;
    int note;
    bool on;
};

// The phrases of tools/preset_audit/PackAnalysis.h (122 BPM; a sixteenth = 0.123 s).
std::vector<Event> phraseFor (const juce::String& role, int root)
{
    std::vector<Event> ev;
    const double s16 = 60.0 / 122.0 / 4.0;
    const auto note = [&ev] (double t, int n, double len) {
        ev.push_back ({ t, n, true });
        ev.push_back ({ t + len, n, false });
    };
    const auto chord = [&note] (double t, int r, std::initializer_list<int> intervals, double len) {
        for (const int i : intervals)
            note (t, r + i, len);
    };
    if (role == "pluck")
    {
        const int line[] = { 0, 7, 3, 10, 0, 7, 12, 3 };
        for (int i = 0; i < 8; ++i)
            note (i * 3.0 * s16, root + line[i], s16 * 1.2);
    }
    else if (role == "keys")
    {
        chord (0.0, root, { 0, 3, 7, 10 }, 0.42);
        chord (0.5, root, { -4, 0, 3, 7 }, 0.42);
        chord (1.0, root, { -2, 2, 5, 9 }, 0.42);
        chord (1.5, root, { 0, 3, 7, 12 }, 0.7);
    }
    else if (role == "lead")
    {
        note (0.0, root, 0.9);
        note (0.9, root + 3, 0.6);
        note (1.5, root + 7, 1.2);
    }
    else if (role == "bass")
    {
        for (int i = 0; i < 32; ++i)
            if (i % 4 != 0)
                note (i * s16, root + ((i % 16 == 14) ? 3 : 0), s16 * 0.6);
    }
    else // pad
    {
        chord (0.0, root, { 0, 3, 7, 10 }, 3.0);
    }
    return ev;
}

void writeWav (const juce::File& f, const std::vector<float>& L, const std::vector<float>& R)
{
    // RIFF / WAVE, IEEE float, 2 channels.
    f.deleteFile();
    juce::FileOutputStream os (f);
    const auto bytes = static_cast<juce::uint32> (L.size() * 2 * sizeof (float));
    os.write ("RIFF", 4);
    os.writeInt (static_cast<int> (36 + bytes));
    os.write ("WAVEfmt ", 8);
    os.writeInt (16);
    os.writeShort (3);
    os.writeShort (2);
    os.writeInt (static_cast<int> (sr));
    os.writeInt (static_cast<int> (sr) * 8);
    os.writeShort (8);
    os.writeShort (32);
    os.write ("data", 4);
    os.writeInt (static_cast<int> (bytes));
    for (size_t i = 0; i < L.size(); ++i)
    {
        os.writeFloat (L[i]);
        os.writeFloat (R[i]);
    }
}
} // namespace

int main (int argc, char** argv)
{
    juce::String role = "pluck";
    int root = 60;
    int a = 1;
    for (; a + 1 < argc && std::strncmp (argv[a], "--", 2) == 0; a += 2)
    {
        if (std::strcmp (argv[a], "--role") == 0)
            role = juce::String::fromUTF8 (argv[a + 1]);
        else if (std::strcmp (argv[a], "--root") == 0)
            root = std::atoi (argv[a + 1]);
    }
    if (argc - a < 3)
    {
        std::printf ("usage: compare_host [--role pluck|keys|pad|lead|bass] [--root 60] <plugin.vst3> <outdir> <presets...>\n");
        return 2;
    }
    juce::ScopedJuceInitialiser_GUI init;
    juce::AudioPluginFormatManager formats;
    juce::addDefaultFormatsToManager (formats);
    juce::OwnedArray<juce::PluginDescription> found;
    juce::VST3PluginFormat vst3;
    vst3.findAllTypesForFile (found, juce::String::fromUTF8 (argv[a]));
    if (found.isEmpty())
    {
        std::printf ("no plug-in in %s\n", argv[a]);
        return 2;
    }
    juce::String error;
    auto plugin = formats.createPluginInstance (*found[0], sr, block, error);
    if (plugin == nullptr)
    {
        std::printf ("cannot load: %s\n", error.toRawUTF8());
        return 2;
    }
    plugin->setPlayConfigDetails (0, 2, sr, block);
    const juce::File outDir (juce::String::fromUTF8 (argv[a + 1]));
    outDir.createDirectory();
    const auto events = phraseFor (role, root);
    const double seconds = role == "pad" ? 8.0 : 6.0;

    for (int i = a + 2; i < argc; ++i)
    {
        const juce::File preset (juce::String::fromUTF8 (argv[i]));
        juce::MemoryBlock data;
        if (! preset.loadFileAsData (data))
            continue;
        plugin->releaseResources();
        // A hosted VST3 takes JUCE's host-side state: the plug-in's own state as the IComponent stream.
        juce::XmlElement state ("VST3PluginState");
        state.createNewChildElement ("IComponent")->addTextElement (data.toBase64Encoding());
        juce::MemoryBlock wrapped;
        juce::AudioProcessor::copyXmlToBinary (state, wrapped);
        plugin->setStateInformation (wrapped.getData(), static_cast<int> (wrapped.getSize()));
        juce::MessageManager::getInstance()->runDispatchLoopUntil (50); // asynchronous parameter updates
        plugin->prepareToPlay (sr, block);

        const int total = static_cast<int> (seconds * sr);
        std::vector<float> L (static_cast<size_t> (total)), R (L.size());
        juce::AudioBuffer<float> buffer (2, block);
        for (int pos = 0; pos < total; pos += block)
        {
            juce::MidiBuffer midi;
            for (const auto& e : events)
            {
                const int s = static_cast<int> (e.time * sr);
                if (s >= pos && s < pos + block)
                    midi.addEvent (e.on ? juce::MidiMessage::noteOn (1, e.note, static_cast<juce::uint8> (100))
                                        : juce::MidiMessage::noteOff (1, e.note),
                                   s - pos);
            }
            buffer.clear();
            plugin->processBlock (buffer, midi);
            const int n = std::min (block, total - pos);
            for (int k = 0; k < n; ++k)
            {
                L[static_cast<size_t> (pos + k)] = buffer.getSample (0, k);
                R[static_cast<size_t> (pos + k)] = buffer.getSample (1, k);
            }
        }
        writeWav (outDir.getChildFile (preset.getFileNameWithoutExtension() + ".wav"), L, R);
        std::printf ("%s\n", preset.getFileNameWithoutExtension().toRawUTF8());
    }
    plugin->releaseResources();
    plugin.reset();
    return 0;
}
