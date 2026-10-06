// augur_preset_audit: loads the real plugin processor (as a host would), plays every factory preset
// with notes that suit its category and reports its level. Fails on silence, non-finite output or
// clipping, so the factory library can be checked after every change.

#include "LegacyEffects.h"
#include "Parameters.h"
#include "PluginEditor.h"
#include "PluginProcessor.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>

namespace
{
struct Stats
{
    float peak = 0.0f;
    double sumSquares = 0.0;
    long long count = 0;
    bool finite = true;
};

std::vector<int> notesFor (const juce::String& category)
{
    if (category == "Bass")  return { 36 };
    if (category == "Drums") return { 36 };
    if (category == "Lead")  return { 72 };
    if (category == "Pad" || category == "Stab" || category == "Keys") return { 57, 60, 64, 67 };
    return { 60, 67 };
}
} // namespace

namespace
{
// --soak "<preset>" <seconds>: plays a repeating chord pattern (1 s on, 1 s off) through the real
// processor for a long time and reports, every 10 s, what is left in the gaps (noise, hiss, whine),
// how bright the held notes are, and the slowest block. Finds problems that only appear after use.
int soak (const juce::String& presetName, double seconds)
{
    constexpr double sr = 48000.0;
    constexpr int block = 256;
    std::unique_ptr<juce::AudioProcessor> base (createPluginFilter());
    auto* proc = dynamic_cast<Augur5Processor*> (base.get());
    proc->setPlayConfigDetails (0, 2, sr, block);
    proc->prepareToPlay (sr, block);
    auto& presets = proc->getPresets();
    const int index = presets.findFactory (presetName);
    if (index < 0)
    {
        std::printf ("preset not found\n");
        return 2;
    }
    presets.loadFactory (index);

    juce::AudioBuffer<float> buffer (2, block);
    const int blocksPerSecond = static_cast<int> (sr / block);
    const int total = static_cast<int> (seconds * blocksPerSecond);
    const int chords[4][3] = { { 57, 60, 64 }, { 53, 57, 60 }, { 60, 64, 67 }, { 55, 59, 62 } };
    double gapSum = 0.0, gapHf = 0.0, noteSum = 0.0, noteHf = 0.0, worstMs = 0.0;
    long gapN = 0, noteN = 0;
    float prevL = 0.0f;
    std::printf ("%6s %10s %10s %10s %10s %9s\n", "t (s)", "gap dB", "gap HF dB", "note dB", "note HF dB", "worst ms");
    for (int b = 0; b < total; ++b)
    {
        juce::MidiBuffer midi;
        const int second = b / blocksPerSecond, inSecond = b % blocksPerSecond;
        const auto& chord = chords[(second / 2) % 4];
        if (inSecond == 0 && second % 2 == 0)
            for (int n : chord)
                midi.addEvent (juce::MidiMessage::noteOn (1, n, static_cast<juce::uint8> (100)), 0);
        if (inSecond == 0 && second % 2 == 1)
            for (int n : chords[((second - 1) / 2) % 4])
                midi.addEvent (juce::MidiMessage::noteOff (1, n), 0);
        buffer.clear();
        const auto t0 = std::chrono::steady_clock::now();
        proc->processBlock (buffer, midi);
        worstMs = std::max (worstMs, std::chrono::duration<double, std::milli> (std::chrono::steady_clock::now() - t0).count());

        // Gap = the last 0.4 s of each silent second (after release tails); note = the middle of the held second.
        const bool gap = second % 2 == 1 && inSecond > blocksPerSecond * 6 / 10;
        const bool note = second % 2 == 0 && inSecond > blocksPerSecond / 4 && inSecond < blocksPerSecond * 3 / 4;
        for (int i = 0; i < block; ++i)
        {
            const float l = buffer.getSample (0, i);
            const float d = l - prevL; // first difference: a crude high-frequency weighting (+6 dB/oct)
            prevL = l;
            if (gap) { gapSum += l * l; gapHf += d * d; ++gapN; }
            if (note) { noteSum += l * l; noteHf += d * d; ++noteN; }
        }
        if ((b + 1) % (10 * blocksPerSecond) == 0)
        {
            const auto db = [] (double s, long n) { return 10.0 * std::log10 (s / std::max (1L, n) + 1e-20); };
            std::printf ("%6d %10.1f %10.1f %10.1f %10.1f %9.3f\n", (b + 1) / blocksPerSecond, db (gapSum, gapN), db (gapHf, gapN),
                         db (noteSum, noteN), db (noteHf, noteN), worstMs);
            gapSum = gapHf = noteSum = noteHf = worstMs = 0.0;
            gapN = noteN = 0;
        }
    }
    return 0;
}
} // namespace

// --snapshot out.png [scale] [tab]: renders the plugin editor (design canvas) to a PNG, for layout review.
// tab: 0 MAIN, 1 MOD, 2 ARP, 3 VOICE, 4 FX, 5 AUGURY, 6 = the preset browser open, 7 = settings open,
// 10..18 = the FX tab showing effect 0..8.
int snapshot (const juce::String& path, float scale, int tab)
{
    std::unique_ptr<juce::AudioProcessor> base (createPluginFilter());
    base->setPlayConfigDetails (0, 2, 48000.0, 256);
    base->prepareToPlay (48000.0, 256);
    auto* editor = base->createEditorIfNeeded();
    editor->setScaleFactor (1.0f);
    if (auto* augur = dynamic_cast<Augur5Editor*> (editor))
    {
        augur->setScale (1.0f);
        if (tab >= 10)
            augur->showEffect (tab - 10);
        else if (tab >= 6)
            augur->showOverlay (tab - 6);
        else
            augur->showTab (tab);
    }
    const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, scale);
    juce::File file (juce::File::getCurrentWorkingDirectory().getChildFile (path));
    file.deleteFile();
    juce::FileOutputStream out (file);
    juce::PNGImageFormat png;
    const bool ok = out.openedOk() && png.writeImageToStream (image, out);
    std::printf ("%s %dx%d -> %s\n", ok ? "wrote" : "FAILED", image.getWidth(), image.getHeight(), file.getFullPathName().toRawUTF8());
    base->editorBeingDeleted (editor);
    delete editor;
    return ok ? 0 : 1;
}

namespace
{
struct Measured
{
    double peakDb = -240.0, loudDb = -240.0;
    bool finite = true;
};

// Plays a loaded sound like the factory audit does (notes by category) and measures its peak and its
// short-term loudness (loudest 50 ms RMS).
Measured measure (juce::AudioProcessor& proc, const juce::String& category)
{
    constexpr double sr = 48000.0;
    constexpr int block = 256, window = 2400;
    const auto notes = notesFor (category);
    const double holdSeconds = category == "Drums" ? 0.2 : 1.5;
    juce::AudioBuffer<float> buffer (2, block);
    Measured m;
    float peak = 0.0f;
    double windowSum = 0.0, loudest = 0.0;
    int inWindow = 0;
    const int totalBlocks = static_cast<int> (4.0 * sr / block), offBlock = static_cast<int> (holdSeconds * sr / block);
    for (int b = 0; b < totalBlocks; ++b)
    {
        juce::MidiBuffer midi;
        if (b == 0)
            for (int n : notes)
                midi.addEvent (juce::MidiMessage::noteOn (1, n, static_cast<juce::uint8> (100)), 0);
        if (b == offBlock)
            for (int n : notes)
                midi.addEvent (juce::MidiMessage::noteOff (1, n), 0);
        buffer.clear();
        proc.processBlock (buffer, midi);
        for (int s = 0; s < block; ++s)
        {
            const float l = buffer.getSample (0, s), r = buffer.getSample (1, s);
            m.finite = m.finite && std::isfinite (l) && std::isfinite (r);
            peak = std::max ({ peak, std::abs (l), std::abs (r) });
            windowSum += 0.5 * (static_cast<double> (l) * l + static_cast<double> (r) * r);
            if (++inWindow == window)
            {
                loudest = std::max (loudest, windowSum / window);
                windowSum = 0.0;
                inWindow = 0;
            }
        }
    }
    m.peakDb = 20.0 * std::log10 (peak + 1e-12);
    m.loudDb = 10.0 * std::log10 (loudest + 1e-24);
    return m;
}

// --level-pack <folder>: validates and loudness-matches every .augur5 preset of an expansion pack.
// Each file carries its role ("category") and loudness target ("target", dB); amp_level is adjusted
// over up to four passes (never pushing the peak over -3 dBFS). Fails on unknown parameter ids,
// non-finite output or silence.
int levelPack (const juce::File& folder)
{
    constexpr double sr = 48000.0;
    auto files = folder.findChildFiles (juce::File::findFiles, true, "*.augur5");
    files.sort();
    int failures = 0, adjusted = 0, converted = 0;
    std::printf ("%-44s %-8s %8s %8s %8s\n", "preset", "role", "target", "peak", "loud");
    for (const auto& file : files)
    {
        auto xml = juce::XmlDocument::parse (file);
        if (xml == nullptr || ! xml->hasTagName ("AUGUR5_PRESET"))
        {
            std::printf ("%s  <-- NOT A PRESET\n", file.getFileName().toRawUTF8());
            ++failures;
            continue;
        }
        const auto category = xml->getStringAttribute ("category", "Pad");
        const double target = xml->getDoubleAttribute ("target", -18.0);
        auto* paramsXml = xml->getChildByName ("PARAMS");

        // Presets written for 1.0 carry its effect ids: rewrite them with the rack's (what loading would do anyway).
        if (paramsXml != nullptr)
        {
            augur5::Settings settings;
            bool legacy = false;
            for (auto* e : paramsXml->getChildIterator())
            {
                settings.push_back ({ e->getStringAttribute ("id"), static_cast<float> (e->getDoubleAttribute ("value")) });
                legacy = legacy || augur5::isLegacyEffectId (settings.back().first);
            }
            if (legacy)
            {
                augur5::convertLegacyEffects (settings);
                paramsXml->deleteAllChildElements();
                for (const auto& [id, value] : settings)
                {
                    auto* e = paramsXml->createNewChildElement ("P");
                    e->setAttribute ("id", id);
                    e->setAttribute ("value", value);
                }
                xml->writeTo (file);
                ++converted;
            }
        }

        Measured m;
        bool unknown = false;
        for (int pass = 0; pass < 4; ++pass)
        {
            std::unique_ptr<juce::AudioProcessor> base (createPluginFilter());
            auto* proc = dynamic_cast<Augur5Processor*> (base.get());
            proc->setPlayConfigDetails (0, 2, sr, 256);
            proc->prepareToPlay (sr, 256);
            if (pass == 0 && paramsXml != nullptr)
                for (auto* e : paramsXml->getChildIterator())
                    if (proc->getParameters().getParameter (e->getStringAttribute ("id")) == nullptr)
                    {
                        std::printf ("%s: UNKNOWN PARAMETER %s\n", file.getFileName().toRawUTF8(), e->getStringAttribute ("id").toRawUTF8());
                        unknown = true;
                    }
            proc->getPresets().loadUser (file);
            m = measure (*proc, category);
            proc->releaseResources();
            if (! m.finite || m.loudDb < -60.0)
                break;

            const double delta = std::min (target - m.loudDb, -3.0 - m.peakDb);
            if (std::abs (delta) < 0.4)
                break;
            // Rewrite amp_level in the file.
            juce::XmlElement* level = nullptr;
            for (auto* e : paramsXml->getChildIterator())
                if (e->getStringAttribute ("id") == "amp_level")
                    level = e;
            if (level == nullptr)
            {
                level = paramsXml->createNewChildElement ("P");
                level->setAttribute ("id", "amp_level");
                level->setAttribute ("value", -6.0);
            }
            const double current = level->getDoubleAttribute ("value", -6.0);
            level->setAttribute ("value", juce::jlimit (-40.0, 6.0, std::round ((current + delta) * 10.0) / 10.0));
            xml->writeTo (file);
            ++adjusted;
        }

        const bool ok = ! unknown && m.finite && m.loudDb > -40.0 && m.peakDb < -0.5 && std::abs (m.loudDb - target) < 4.0;
        failures += ok ? 0 : 1;
        std::printf ("%-44s %-8s %8.1f %8.1f %8.1f %s\n", file.getFileNameWithoutExtension().substring (0, 44).toRawUTF8(),
                     category.toRawUTF8(), target, m.peakDb, m.loudDb, ok ? "" : "  <-- CHECK");
    }
    std::printf ("\n%d presets (%d converted from 1.0 effect ids), %d level adjustments, %d need attention\n", files.size(), converted, adjusted,
                 failures);
    return failures == 0 ? 0 : 1;
}
} // namespace

// --check-augury: the AUGURY tools and the plugin state they add, measured.
//   corners reproduce their sounds exactly; the centre is the mean of continuous values; an OMEN seed gives the same
//   variation every time; locked groups do not move; corners and the rack order survive a session save / load.
int checkAugury()
{
    int failures = 0;
    const auto expect = [&failures] (bool ok, const char* what) {
        std::printf ("%-64s %s\n", what, ok ? "ok" : "FAIL");
        failures += ok ? 0 : 1;
    };
    const auto snapshotOf = [] (juce::AudioProcessor& p) {
        std::vector<float> v;
        for (auto* param : p.getParameters())
            v.push_back (param->getValue());
        return v;
    };
    const auto valueOf = [] (Augur5Processor& p, const char* id) { return p.getParameters().getParameter (id)->getValue(); };

    std::unique_ptr<juce::AudioProcessor> base (createPluginFilter());
    auto& proc = *dynamic_cast<Augur5Processor*> (base.get());
    auto& presets = proc.getPresets();
    auto& augury = proc.getAugury();

    presets.loadFactory (presets.findFactory ("Warm Horizon"));
    const auto a = snapshotOf (proc);
    const float cutA = valueOf (proc, augur5::params::flt_cutoff);
    augury.capture (0, presets.getCurrentName());
    presets.loadFactory (presets.findFactory ("Savanna Horn Lead"));
    const auto d = snapshotOf (proc);
    const float cutD = valueOf (proc, augur5::params::flt_cutoff);
    augury.capture (3, presets.getCurrentName());

    augury.beginMorph();
    augury.setPosition ({ 0.0f, 0.0f });
    expect (snapshotOf (proc) == a, "corner A reproduces its sound exactly");
    augury.setPosition ({ 1.0f, 1.0f });
    expect (snapshotOf (proc) == d, "corner D reproduces its sound exactly");
    augury.setPosition ({ 0.5f, 0.5f });
    const float mid = valueOf (proc, augur5::params::flt_cutoff);
    expect (std::abs (mid - 0.5f * (cutA + cutD)) < 1.0e-4f, "centre = mean of the corners (normalised cutoff)");
    augury.endMorph();

    presets.loadFactory (presets.findFactory ("Warm Horizon"));
    augury.castOmen (0.6f, 0u, 4242u);
    const auto omen1 = snapshotOf (proc);
    presets.loadFactory (presets.findFactory ("Warm Horizon"));
    augury.castOmen (0.6f, 0u, 4242u);
    expect (snapshotOf (proc) == omen1, "the same OMEN seed gives the same variation");
    expect (omen1 != a, "an OMEN changes the sound");

    presets.loadFactory (presets.findFactory ("Warm Horizon"));
    const float res0 = valueOf (proc, augur5::params::flt_reso), env0 = valueOf (proc, augur5::params::flt_env_amt);
    augury.castOmen (1.0f, 1u << augur5::AuguryModel::filter, 99u);
    expect (valueOf (proc, augur5::params::flt_reso) == res0 && valueOf (proc, augur5::params::flt_env_amt) == env0,
            "a locked group (FILTER) does not move");

    proc.setFxOrder ({ 7, 6, 5, 4, 3, 2, 1, 0 });
    juce::MemoryBlock state;
    proc.getStateInformation (state);
    std::unique_ptr<juce::AudioProcessor> other (createPluginFilter());
    auto& restored = *dynamic_cast<Augur5Processor*> (other.get());
    restored.setStateInformation (state.getData(), static_cast<int> (state.getSize()));
    expect (restored.getAugury().isFilled (0) && restored.getAugury().isFilled (3) && ! restored.getAugury().isFilled (1),
            "corners survive a session save / load");
    expect (restored.getAugury().getName (3) == "Savanna Horn Lead", "corner names survive a session save / load");
    expect (restored.getFxOrder() == std::array<int, 8> { 7, 6, 5, 4, 3, 2, 1, 0 }, "the rack order survives a session save / load");
    proc.getPresets().loadFactory (0);
    expect (proc.getFxOrder() == std::array<int, 8> { 0, 1, 2, 3, 4, 5, 6, 7 }, "a factory sound restores the default rack order");

    std::printf ("%s\n", failures == 0 ? "AUGURY: all checks passed" : "AUGURY: FAILURES");
    return failures == 0 ? 0 : 1;
}

int main (int argc, char** argv)
{
    if (argc >= 2 && std::strcmp (argv[1], "--check-augury") == 0)
        return checkAugury();
    juce::ScopedJuceInitialiser_GUI juceInit;
    if (argc >= 3 && (std::strcmp (argv[1], "--install-pack") == 0 || std::strcmp (argv[1], "--import-folder") == 0))
    {
        // Same code paths as the browser's "Install expansion pack" / "Add presets folder".
        const auto source = juce::File::getCurrentWorkingDirectory().getChildFile (juce::String::fromUTF8 (argv[2]));
        const int count = std::strcmp (argv[1], "--install-pack") == 0 ? augur5::PresetManager::installPack (source)
                                                                        : augur5::PresetManager::importFolder (source);
        std::unique_ptr<juce::AudioProcessor> base (createPluginFilter());
        const auto visible = dynamic_cast<Augur5Processor*> (base.get())->getPresets().getUserPresets().size();
        std::printf ("installed %d presets; the browser now lists %d user presets in %s\n", count, visible,
                     augur5::PresetManager::getUserFolder().getFullPathName().toRawUTF8());
        return count > 0 ? 0 : 1;
    }
    if (argc >= 3 && std::strcmp (argv[1], "--level-pack") == 0)
        return levelPack (juce::File::getCurrentWorkingDirectory().getChildFile (juce::String::fromUTF8 (argv[2])));
    if (argc >= 3 && std::strcmp (argv[1], "--snapshot") == 0)
        return snapshot (juce::String::fromUTF8 (argv[2]), argc >= 4 ? static_cast<float> (std::atof (argv[3])) : 1.0f, argc >= 5 ? std::atoi (argv[4]) : 0);
    if (argc >= 4 && std::strcmp (argv[1], "--soak") == 0)
        return soak (juce::String::fromUTF8 (argv[2]), std::atof (argv[3]));
    constexpr double sr = 48000.0;
    constexpr int block = 256;
    constexpr int window = 2400; // 50 ms short-term loudness window

    std::unique_ptr<juce::AudioProcessor> first (createPluginFilter());
    const int numPresets = dynamic_cast<Augur5Processor*> (first.get())->getPresets().getNumFactoryPresets();
    const auto unknown = dynamic_cast<Augur5Processor*> (first.get())->getPresets().findUnknownFactoryIds();
    first.reset();
    for (const auto& u : unknown)
        std::printf ("UNKNOWN PARAMETER  %s\n", u.toRawUTF8());

    int failures = 0;
    std::printf ("%-24s %-12s %9s %9s\n", "preset", "category", "peak dB", "loud dB");

    for (int i = 0; i < numPresets; ++i)
    {
        // A fresh processor per preset: every measurement is deterministic and independent.
        std::unique_ptr<juce::AudioProcessor> base (createPluginFilter());
        auto* proc = dynamic_cast<Augur5Processor*> (base.get());
        proc->setPlayConfigDetails (0, 2, sr, block);
        proc->prepareToPlay (sr, block);
        auto& presets = proc->getPresets();
        presets.loadFactory (i);

        const auto category = presets.getFactoryCategory (i);
        const auto notes = notesFor (category);
        const double holdSeconds = category == "Drums" ? 0.2 : 1.5;

        juce::AudioBuffer<float> buffer (2, block);
        Stats st;
        double windowSum = 0.0, loudest = 0.0;
        int inWindow = 0;
        const int totalBlocks = static_cast<int> (4.0 * sr / block);
        const int offBlock = static_cast<int> (holdSeconds * sr / block);
        for (int b = 0; b < totalBlocks; ++b)
        {
            juce::MidiBuffer midi;
            if (b == 0)
                for (int n : notes)
                    midi.addEvent (juce::MidiMessage::noteOn (1, n, static_cast<juce::uint8> (100)), 0);
            if (b == offBlock)
                for (int n : notes)
                    midi.addEvent (juce::MidiMessage::noteOff (1, n), 0);
            buffer.clear();
            proc->processBlock (buffer, midi);
            for (int s = 0; s < block; ++s)
            {
                const float l = buffer.getSample (0, s), r = buffer.getSample (1, s);
                st.finite = st.finite && std::isfinite (l) && std::isfinite (r);
                st.peak = std::max ({ st.peak, std::abs (l), std::abs (r) });
                windowSum += 0.5 * (static_cast<double> (l) * l + static_cast<double> (r) * r);
                if (++inWindow == window)
                {
                    loudest = std::max (loudest, windowSum / window);
                    windowSum = 0.0;
                    inWindow = 0;
                }
            }
        }
        proc->releaseResources();

        const double peakDb = 20.0 * std::log10 (st.peak + 1e-12);
        const double loudDb = 10.0 * std::log10 (loudest + 1e-24);
        const bool ok = st.finite && peakDb < -0.5 && loudDb > -40.0;
        failures += ok ? 0 : 1;
        std::printf ("%-24s %-12s %9.1f %9.1f %s\n", presets.getFactoryName (i).toRawUTF8(), category.toRawUTF8(),
                     peakDb, loudDb, ok ? "" : (st.finite ? "  <-- level out of range" : "  <-- NON-FINITE"));
    }
    std::printf ("\n%d preset(s) out of range\n", failures);
    return failures == 0 && unknown.isEmpty() ? 0 : 1;
}
