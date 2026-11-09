// augur_preset_audit: loads the real plugin processor (as a host would), plays every factory preset
// with notes that suit its category and reports its level. Fails on silence, non-finite output or
// clipping, so the factory library can be checked after every change.

#include "LegacyEffects.h"
#include "PackAnalysis.h"
#include "gui/Theme.h"
#include "Engine/SynthParams.h"
#include "Parameters.h"
#include "PluginEditor.h"
#include "PluginProcessor.h"

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <thread>

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

// --snapshot out.png [scale] [tab] [skin]: renders the plugin editor (design canvas) to a PNG, for layout review.
// skin: 0 LIGHT, 1 NEON, 2 IMPERIAL (this run only: the user's saved choice is not touched).
// tab: 0 MAIN, 1 MOD, 2 ARP, 3 VOICE, 4 FX, 5 AUGURY, 6 = the preset browser open, 7 = settings open,
// 10..18 = the FX tab showing effect 0..8, 19 = the MOD tab with a busy test matrix.
int snapshot (const juce::String& path, float scale, int tab, int skin)
{
    if (skin >= 0)
        Augur5Editor::previewSkin (skin);
    std::unique_ptr<juce::AudioProcessor> base (createPluginFilter());
    base->setPlayConfigDetails (0, 2, 48000.0, 256);
    base->prepareToPlay (48000.0, 256);
    if (tab == 19)
    {
        // A busy matrix for the ROUTING review: positive, negative, shared destination, the same route twice and a
        // route the engine cannot apply (a per-voice source to the shared LFO's rate).
        auto& st = dynamic_cast<Augur5Processor*> (base.get())->getParameters();
        const auto set = [&st] (const juce::String& id, float v) {
            if (auto* p = st.getParameter (id))
                p->setValueNotifyingHost (p->convertTo0to1 (v));
        };
        const float routes[8][3] { { 3, 4, 0.4f }, { 4, 7, 0.6f }, { 5, 7, 0.5f }, { 8, 0, -0.3f },
                                   { 9, 4, 0.2f }, { 3, 4, -0.25f }, { 6, 5, 0.7f }, { 11, 3, 1.0f } };
        for (int s = 0; s < 8; ++s)
        {
            set (augur5::params::mmSrc (s + 1), routes[s][0]);
            set (augur5::params::mmDst (s + 1), routes[s][1]);
            set (augur5::params::mmAmt (s + 1), routes[s][2]);
        }
        tab = 1;
    }
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
// How a factory sound is used in a track: the pack audit's role for its category (Signature sounds by their name).
juce::String factoryRole (const juce::String& category, const juce::String& name)
{
    const auto c = category.toLowerCase();
    if (c == "pluck" || c == "bass" || c == "lead" || c == "keys" || c == "arp" || c == "pad")
        return c;
    if (c == "stab")
        return "keys"; // chord stabs
    if (c.contains ("fx"))
        return "fx";
    const auto n = name.toLowerCase();
    for (const char* r : { "pluck", "bass", "lead", "keys", "arp" })
        if (n.contains (r))
            return r;
    return "pad";
}

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

// --level-pack <folder> [--pack-scale]: validates and loudness-matches every .augur5 preset of an expansion pack.
// Each file carries its role ("category"), its loudness target ("target", dB) and how loudness is measured:
//  - loudness="pack": the loudest 400 ms of the role's phrase, peak <= -1 dBFS (the scale of the atlases, D-045);
//  - otherwise the loudest 50 ms of a held chord, peak <= -3 dBFS (one-shot drums).
// --pack-scale first moves every melodic preset to the pack scale at -16 dB. amp_level is adjusted over up to four
// passes. Fails on unknown parameter ids, non-finite output or silence.
int levelPack (const juce::File& folder, bool toPackScale)
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
        if (toPackScale && category != "Drums" && xml->getStringAttribute ("loudness") != "pack")
        {
            xml->setAttribute ("target", "-16.0");
            xml->setAttribute ("loudness", "pack");
            xml->writeTo (file);
        }
        const double target = xml->getDoubleAttribute ("target", -18.0);
        const bool packScale = xml->getStringAttribute ("loudness") == "pack";
        const double ceiling = packScale ? -1.0 : -3.0;
        const auto role = factoryRole (category, xml->getStringAttribute ("name"));
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
            if (packScale)
            {
                const auto pm = packaudit::analyse (*proc, role, role == "bass" ? 36 : 60, role == "pad" || role == "fx" ? 8.0 : 6.0);
                m.peakDb = pm.peakDb;
                m.loudDb = pm.loudDb;
                m.finite = pm.finite;
            }
            else
            {
                m = measure (*proc, category);
            }
            proc->releaseResources();
            if (! m.finite || m.loudDb < -60.0)
                break;

            const double delta = std::min (target - m.loudDb, ceiling - m.peakDb);
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
//   corners reproduce their sounds exactly (the voice structure stays as played; RECALL applies it); the centre is the
//   mean of continuous values; an OMEN seed gives the same variation every time; locked groups do not move; 20 OMEN in
//   a row stay audible; corners and the rack order survive a session save / load, and a user preset carries its scene.
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
    // Every parameter but the voice structure, which a flight leaves as played.
    const auto soundOf = [] (juce::AudioProcessor& p) {
        std::vector<float> v;
        for (auto* param : p.getParameters())
            if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (param); rp != nullptr && ! augur5::AuguryModel::isVoiceStructure (rp->getParameterID()))
                v.push_back (param->getValue());
        return v;
    };

    std::unique_ptr<juce::AudioProcessor> base (createPluginFilter());
    auto& proc = *dynamic_cast<Augur5Processor*> (base.get());
    auto& presets = proc.getPresets();
    auto& augury = proc.getAugury();

    presets.loadFactory (presets.findFactory ("Warm Horizon"));
    const auto a = snapshotOf (proc);
    const auto soundA = soundOf (proc);
    const float cutA = valueOf (proc, augur5::params::flt_cutoff), voicesA = valueOf (proc, augur5::params::voice_count);
    augury.capture (0, presets.getCurrentName());
    presets.loadFactory (presets.findFactory ("Savanna Horn Lead")); // UNISON, 4 voices (Warm Horizon: POLY, 5)
    const auto d = snapshotOf (proc);
    const auto soundD = soundOf (proc);
    const float cutD = valueOf (proc, augur5::params::flt_cutoff);
    augury.capture (3, presets.getCurrentName());

    presets.loadFactory (presets.findFactory ("Warm Horizon"));
    augury.beginMorph();
    augury.setPosition ({ 0.0f, 0.0f });
    expect (snapshotOf (proc) == a, "corner A reproduces its sound exactly");
    augury.setPosition ({ 1.0f, 1.0f });
    expect (soundOf (proc) == soundD, "corner D reproduces its sound exactly (all but the voice structure)");
    expect (valueOf (proc, augur5::params::unison) < 0.5f && valueOf (proc, augur5::params::voice_count) == voicesA,
            "flying to D keeps the voice structure as played (POLY, 5 voices)");
    augury.setPosition ({ 0.5f, 0.5f });
    const float mid = valueOf (proc, augur5::params::flt_cutoff);
    expect (std::abs (mid - 0.5f * (cutA + cutD)) < 1.0e-4f, "centre = mean of the corners (normalised cutoff)");
    augury.endMorph();
    augury.recall (3);
    expect (snapshotOf (proc) == d, "RECALL D gives sound D exactly, its UNISON and VOICES included");
    augury.beginMorph();
    augury.setPosition ({ 0.0f, 0.0f });
    augury.endMorph();
    expect (soundOf (proc) == soundA && valueOf (proc, augur5::params::unison) > 0.5f, "flying back to A keeps UNISON as played");

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

    // OMEN cast again and again (20 times in a row at 100 %, as a user hunting for a sound does) on every 8th factory
    // sound: the variation may move the level, but it must stay a playable sound, never close to silent.
    {
        proc.setPlayConfigDetails (0, 2, 48000.0, 256);
        const auto roleOf = [] (const juce::String& category) -> std::pair<const char*, int> {
            if (category == "Bass" || category == "Drums") return { "bass", 36 };
            if (category == "Lead") return { "lead", 60 };
            if (category == "Pluck" || category == "Arp") return { "pluck", 60 };
            return { "pad", 57 };
        };
        double worstDrop = 0.0;
        int nearSilent = 0, casts = 0;
        juce::String worstName;
        const auto only = juce::SystemStats::getEnvironmentVariable ("AUGUR_OMEN_ONLY", {}); // DIAG: one sound
        for (int index = 0; index < presets.getNumFactoryPresets(); index += 8)
        {
            if (only.isNotEmpty() && presets.getFactoryName (index) != only)
                continue;
            const auto [role, root] = roleOf (presets.getFactoryCategory (index));
            presets.loadFactory (index);
            proc.prepareToPlay (48000.0, 256);
            const double base = packaudit::analyse (proc, role, root, 3.0).loudDb;
            double lowest = base;
            std::vector<float> original;
            for (auto* p : proc.AudioProcessor::getParameters())
                original.push_back (p->getValue());
            for (juce::uint32 k = 1; k <= 20; ++k)
            {
                augury.castOmen (1.0f, 0u, 7000u + 37u * static_cast<juce::uint32> (index) + k);
                proc.prepareToPlay (48000.0, 256);
                const double level = packaudit::analyse (proc, role, root, 3.0).loudDb;
                lowest = std::min (lowest, level);
                nearSilent += level < base - 24.0 ? 1 : 0;
                ++casts;
                if (level < base - 24.0 && juce::SystemStats::getEnvironmentVariable ("AUGUR_OMEN_DIAG", {}).isNotEmpty())
                {
                    // DIAG: which single parameter, put back to the preset's value, brings the sound back the most.
                    const auto& all = proc.AudioProcessor::getParameters();
                    std::vector<float> now;
                    for (auto* p : all)
                        now.push_back (p->getValue());
                    std::vector<std::pair<double, int>> gains;
                    for (size_t i = 0; i < all.size(); ++i)
                    {
                        if (std::abs (now[i] - original[i]) < 1.0e-6f)
                            continue;
                        all[i]->setValueNotifyingHost (original[i]);
                        proc.prepareToPlay (48000.0, 256);
                        gains.push_back ({ packaudit::analyse (proc, role, root, 1.5).loudDb - level, static_cast<int> (i) });
                        all[i]->setValueNotifyingHost (now[i]);
                    }
                    std::sort (gains.rbegin(), gains.rend());
                    std::printf ("      cast %u: %.1f dB;", k, level);
                    for (size_t g = 0; g < 4 && g < gains.size(); ++g)
                    {
                        auto* rp = dynamic_cast<juce::RangedAudioParameter*> (all[static_cast<size_t> (gains[g].second)]);
                        std::printf ("  %s %s->%s (+%.0f)", rp->getParameterID().toRawUTF8(),
                                     rp->getText (original[static_cast<size_t> (gains[g].second)], 12).toRawUTF8(),
                                     rp->getCurrentValueAsText().toRawUTF8(), gains[g].first);
                    }
                    std::printf ("\n");
                }
            }
            if (base - lowest > worstDrop)
            {
                worstDrop = base - lowest;
                worstName = presets.getFactoryName (index);
            }
            std::printf ("    %-24s level %6.1f dB, lowest after 20 OMEN %6.1f dB\n", presets.getFactoryName (index).toRawUTF8(), base, lowest);
        }
        std::printf ("    worst drop %.1f dB (%s); %d of %d omens more than 24 dB down\n", worstDrop, worstName.toRawUTF8(), nearSilent, casts);
        expect (nearSilent == 0, "20 OMEN in a row at 100 % never leave a sound close to silent");
    }

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

    // A user preset carries its AUGURY scene (corners, names, the bird); loading a preset without a scene leaves the
    // corners alone (sounds are captured from one preset after another). The file is temporary.
    {
        std::unique_ptr<juce::AudioProcessor> ownerA (createPluginFilter()), ownerB (createPluginFilter());
        auto& pa = *dynamic_cast<Augur5Processor*> (ownerA.get());
        auto& pb = *dynamic_cast<Augur5Processor*> (ownerB.get());
        pa.getPresets().loadFactory (pa.getPresets().findFactory ("Warm Horizon"));
        pa.getAugury().capture (0, "Warm Horizon");
        pa.getPresets().loadFactory (pa.getPresets().findFactory ("Brass Section"));
        pa.getAugury().capture (3, "Brass Section");
        pa.getAugury().beginMorph();
        pa.getAugury().setPosition ({ 0.3f, 0.6f });
        pa.getAugury().endMorph();
        const auto blend = snapshotOf (pa);
        // (a preset file stores plain values as text: equal to well within a step of any control)
        const auto near = [] (const std::vector<float>& x, const std::vector<float>& y) {
            if (x.size() != y.size())
                return false;
            for (size_t i = 0; i < x.size(); ++i)
                if (std::abs (x[i] - y[i]) > 1.0e-5f)
                    return false;
            return true;
        };
        const juce::String name ("zz AUGURY scene check");
        const bool saved = pa.getPresets().saveUser (name);
        const auto file = augur5::PresetManager::getUserFolder().getChildFile (name + ".augur5");
        pb.getPresets().loadUser (file);
        const auto& sb = pb.getAugury();
        expect (saved && sb.isFilled (0) && sb.isFilled (3) && ! sb.isFilled (1) && ! sb.isFilled (2) && sb.getName (3) == "Brass Section",
                "a user preset carries its AUGURY corners and their names");
        expect (near (snapshotOf (pb), blend) && sb.getPosition() == juce::Point<float> (0.3f, 0.6f), "and plays the sound it was saved with, the bird where it was");
        pb.getAugury().beginMorph();
        pb.getAugury().setPosition ({ 1.0f, 1.0f });
        pb.getAugury().endMorph();
        pa.getPresets().loadFactory (pa.getPresets().findFactory ("Brass Section"));
        expect (near (snapshotOf (pb), snapshotOf (pa)), "its corner D is the captured sound");
        pb.getPresets().loadFactory (pb.getPresets().findFactory ("Sync Lead"));
        expect (sb.filledCount() == 2, "a sound without a scene leaves the corners as they were");
        file.deleteFile();
    }

    std::printf ("%s\n", failures == 0 ? "AUGURY: all checks passed" : "AUGURY: FAILURES");
    return failures == 0 ? 0 : 1;
}

// --logo dir: writes the AUGUR-5 name (type only) on white, at 4x.
int exportLogo (const juce::File& dir)
{
    dir.createDirectory();
    const auto write = [] (const juce::Image& image, const juce::File& file) {
        file.deleteFile();
        juce::FileOutputStream out (file);
        juce::PNGImageFormat png;
        const bool ok = out.openedOk() && png.writeImageToStream (image, out);
        std::printf ("%s %dx%d -> %s\n", ok ? "wrote" : "FAILED", image.getWidth(), image.getHeight(), file.getFullPathName().toRawUTF8());
        return ok;
    };
    bool ok = true;

    // Full logo: measure once, then draw at 4x.
    float width = 0.0f;
    {
        juce::Image scratch (juce::Image::ARGB, 8, 8, true);
        juce::Graphics g (scratch);
        width = augur5::ui::drawAugurLogo (g, 0.0f, 0.0f);
    }
    constexpr float margin = 28.0f, height = 70.0f, zoom = 4.0f;
    // (Each Graphics is closed before its image is written: GPU-backed images are only flushed then.)
    {
        juce::Image image (juce::Image::ARGB, juce::roundToInt ((width + 2.0f * margin) * zoom), juce::roundToInt ((height + 2.0f * margin) * zoom), true);
        {
            juce::Graphics g (image);
            g.fillAll (juce::Colours::white);
            g.addTransform (juce::AffineTransform::scale (zoom));
            augur5::ui::drawAugurLogo (g, margin, margin);
        }
        ok = write (image, dir.getChildFile ("AUGUR-5 logo.png")) && ok;
    }
    // (The maker's logos and the app icon come from the TONAL LAB kit: docs/brand/tonal_lab.)
    return ok ? 0 : 1;
}

// --params: every parameter as CSV (id;kind;min;max;default;choices) so pack generators clamp to the real ranges and
// never write an id the instrument does not have.
int dumpParams()
{
    std::unique_ptr<juce::AudioProcessor> base (createPluginFilter());
    std::printf ("id;kind;min;max;default;choices\n");
    for (auto* p : base->getParameters())
    {
        auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p);
        if (rp == nullptr)
            continue;
        const auto range = rp->getNormalisableRange();
        juce::String kind = "float", choices;
        if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (rp))
        {
            kind = "choice";
            choices = c->choices.joinIntoString ("|");
        }
        else if (dynamic_cast<juce::AudioParameterBool*> (rp) != nullptr)
            kind = "bool";
        else if (dynamic_cast<juce::AudioParameterInt*> (rp) != nullptr)
            kind = "int";
        std::printf ("%s;%s;%g;%g;%g;%s\n", rp->getParameterID().toRawUTF8(), kind.toRawUTF8(), range.start, range.end,
                     rp->convertFrom0to1 (rp->getDefaultValue()), choices.toRawUTF8());
    }
    return 0;
}

// --pack <folder> [--dry] [--wav <dir>]: every .augur5 below the folder, played as its role (META element written by the
// pack generator), fresh engine per preset. CSV on stdout. --dry also renders every preset with its effects off (fpdry),
// so the generator judges whether two presets differ on the synth itself, not on the effects. AUGUR_AUDIT_REVERSE=1 plays
// the folder in reverse order: comparing both runs proves a preset sounds the same whatever was played before it.
int packAudit (const juce::File& folder, const juce::File& wavDir, bool dryFingerprint)
{
    constexpr double sr = 48000.0;
    constexpr int block = 256;
    std::unique_ptr<juce::AudioProcessor> base (createPluginFilter());
    auto& proc = *dynamic_cast<Augur5Processor*> (base.get());
    proc.setPlayConfigDetails (0, 2, sr, block);
    proc.prepareToPlay (sr, block);
    std::printf ("path;role;peak;loud;level;centroid;low;high;dc;width;tail;attack;finite;fp;fpdry\n");
    auto files = folder.findChildFiles (juce::File::findFiles, true, "*.augur5");
    files.sort();
    if (juce::SystemStats::getEnvironmentVariable ("AUGUR_AUDIT_REVERSE", {}).isNotEmpty())
        std::reverse (files.begin(), files.end());
    const auto join = [] (const std::vector<float>& v) {
        juce::String out;
        for (const auto x : v)
            out << (out.isEmpty() ? "" : "|") << juce::String (x, 2);
        return out;
    };
    for (const auto& f : files)
    {
        auto xml = juce::XmlDocument::parse (f);
        if (xml == nullptr)
            continue;
        juce::String role = "pad";
        int root = 60;
        if (auto* meta = xml->getChildByName ("META"))
        {
            role = meta->getStringAttribute ("role", role);
            root = meta->getIntAttribute ("note", root);
        }
        proc.getPresets().loadUser (f);
        proc.prepareToPlay (sr, block); // fresh state: no tails from the previous sound
        const double seconds = role == "pad" || role == "drone" || role == "fx" ? 8.0 : 6.0;
        const auto rel = f.getRelativePathFrom (folder);
        const auto wav = wavDir == juce::File() ? juce::File() : wavDir.getChildFile (rel).withFileExtension ("wav");
        const auto m = packaudit::analyse (proc, role, root, seconds, wav);
        const float level = proc.getParameters().getRawParameterValue (augur5::params::amp_level)->load();
        juce::String dry;
        if (dryFingerprint)
        {
            proc.getPresets().loadUser (f);
            for (const char* id : { augur5::params::fuzz_on, augur5::params::delay_on, augur5::params::fx_drive_on, augur5::params::fx_chorus_on,
                                    augur5::params::fx_phaser_on, augur5::params::fx_flanger_on, augur5::params::fx_echo_on,
                                    augur5::params::fx_reverb_on, augur5::params::fx_comp_on })
                if (auto* p = proc.getParameters().getParameter (id))
                    p->setValueNotifyingHost (0.0f);
            proc.prepareToPlay (sr, block);
            dry = join (packaudit::analyse (proc, role, root, seconds).fingerprint);
        }
        std::printf ("%s;%s;%.2f;%.2f;%.2f;%.0f;%.4f;%.4f;%.5f;%.3f;%.2f;%.3f;%d;%s;%s\n", rel.replaceCharacter ('\\', '/').toRawUTF8(),
                     role.toRawUTF8(), m.peakDb, m.loudDb, level, m.centroid, m.low, m.high, m.dc, m.width, m.tail, m.attack, m.finite ? 1 : 0,
                     join (m.fingerprint).toRawUTF8(), dry.toRawUTF8());
        std::fflush (stdout);
    }
    return 0;
}

// --stereo-report <folder | factory>: the bass phase and the spectrum above it, per preset, for an A/B of a change
// that must keep the lows in phase and leave the rest alone. Each preset is played as its role on a fresh engine and
// the whole output (notes and tail) is measured from one FFT per channel. CSV on stdout:
//   bassdb           the 20..120 Hz band, dB re the whole output
//   corr, mono, bal  that band's L/R correlation, the loss of a mono sum (dB) and the L/R balance (dB)
//   loud             the loudest 400 ms (the packs' level)
//   L, R             per-channel level in the third-octave bands 160 Hz .. 16 kHz (dB), '|' separated
int stereoReport (const juce::String& what)
{
    constexpr double sr = 48000.0;
    constexpr int block = 256;
    std::unique_ptr<juce::AudioProcessor> base (createPluginFilter());
    auto& proc = *dynamic_cast<Augur5Processor*> (base.get());
    proc.setPlayConfigDetails (0, 2, sr, block);
    proc.prepareToPlay (sr, block);
    auto& presets = proc.getPresets();

    struct Item
    {
        juce::String name, role;
        int root = 60;
        int factory = -1;
        juce::File file;
    };
    std::vector<Item> items;
    const auto folder = juce::File::getCurrentWorkingDirectory().getChildFile (what);
    if (what == "factory")
    {
        for (int i = 0; i < presets.getNumFactoryPresets(); ++i)
        {
            const auto category = presets.getFactoryCategory (i), name = presets.getFactoryName (i);
            if (category == "Drums" || name == "Init")
                continue;
            const auto role = factoryRole (category, name);
            items.push_back ({ category + "/" + name, role, role == "bass" ? 36 : 60, i, {} });
        }
    }
    else
    {
        auto files = folder.findChildFiles (juce::File::findFiles, true, "*.augur5");
        files.sort();
        for (const auto& f : files)
        {
            auto xml = juce::XmlDocument::parse (f);
            if (xml == nullptr)
                continue;
            Item it { f.getRelativePathFrom (folder).replaceCharacter ('\\', '/'), "pad", 60, -1, f };
            if (auto* meta = xml->getChildByName ("META"))
            {
                it.role = meta->getStringAttribute ("role", it.role);
                it.root = meta->getIntAttribute ("note", it.root);
            }
            else
            {
                // Expansion packs without META: the category from the file's prefix, as their browser shows it.
                const auto prefix = f.getFileName().substring (0, 2);
                const char* roles[][2] { { "BA", "bass" }, { "LD", "lead" }, { "PD", "pad" }, { "PL", "pluck" }, { "CH", "keys" },
                                         { "KY", "keys" }, { "AR", "arp" }, { "FX", "fx" }, { "DR", "bass" } };
                for (const auto& r : roles)
                    if (prefix == r[0])
                        it.role = r[1];
                it.root = it.role == "bass" ? 36 : 60;
            }
            items.push_back (it);
        }
    }

    constexpr double bandCentres[] { 160, 200, 250, 315, 400, 500, 630, 800, 1000, 1250, 1600, 2000, 2500, 3150, 4000, 5000, 6300, 8000,
                                     10000, 12500, 16000 };
    std::printf ("path;role;loud;bassdb;corr;mono;bal;L;R\n");
    for (const auto& it : items)
    {
        if (it.factory >= 0)
            presets.loadFactory (it.factory);
        else
            presets.loadUser (it.file);
        proc.prepareToPlay (sr, block); // fresh state: no tails from the previous sound
        const double seconds = it.role == "pad" || it.role == "drone" || it.role == "fx" ? 8.0 : 6.0;
        std::vector<float> L, R;
        packaudit::renderRole (proc, it.role, it.root, seconds, L, R);

        double loudest = 0.0;
        const size_t win = static_cast<size_t> (0.4 * sr), hop = win / 4;
        for (size_t s = 0; s + win <= L.size(); s += hop)
        {
            double acc = 0.0;
            for (size_t i = s; i < s + win; ++i)
                acc += 0.5 * (static_cast<double> (L[i]) * L[i] + static_cast<double> (R[i]) * R[i]);
            loudest = std::max (loudest, acc / static_cast<double> (win));
        }

        size_t n = 1;
        while (n < L.size())
            n <<= 1;
        std::vector<std::complex<double>> a (n), b (n);
        for (size_t i = 0; i < L.size(); ++i)
        {
            a[i] = L[i];
            b[i] = R[i];
        }
        packaudit::fft (a);
        packaudit::fft (b);
        double ll = 0.0, rr = 0.0, lr = 0.0, mm = 0.0, total = 0.0;
        std::vector<double> bandL (std::size (bandCentres), 0.0), bandR (bandL.size(), 0.0);
        for (size_t k = 1; k < n / 2; ++k)
        {
            const double f = static_cast<double> (k) * sr / static_cast<double> (n);
            const double pl = std::norm (a[k]), pr = std::norm (b[k]);
            total += 0.5 * (pl + pr);
            if (f >= 20.0 && f <= 120.0)
            {
                ll += pl;
                rr += pr;
                lr += (a[k] * std::conj (b[k])).real();
                mm += 0.25 * std::norm (a[k] + b[k]);
            }
            for (size_t j = 0; j < bandL.size(); ++j)
                if (f >= bandCentres[j] * 0.8909 && f < bandCentres[j] * 1.1225)
                {
                    bandL[j] += pl;
                    bandR[j] += pr;
                }
        }
        const auto db = [] (double x) { return 10.0 * std::log10 (x + 1.0e-30); };
        juce::String sl, srr;
        for (size_t j = 0; j < bandL.size(); ++j)
        {
            sl << (j ? "|" : "") << juce::String (db (bandL[j]), 2);
            srr << (j ? "|" : "") << juce::String (db (bandR[j]), 2);
        }
        std::printf ("%s;%s;%.2f;%.2f;%.4f;%.3f;%.2f;%s;%s\n", it.name.toRawUTF8(), it.role.toRawUTF8(), db (loudest), db (0.5 * (ll + rr)) - db (total),
                     lr / std::sqrt (ll * rr + 1.0e-30), db (mm) - db (0.5 * (ll + rr)), db (ll) - db (rr), sl.toRawUTF8(), srr.toRawUTF8());
        std::fflush (stdout);
    }
    return 0;
}

// --factory-wav <dir> [category]: every factory sound (or one category) played as its role in a track (the pack
// phrases), written as <dir>/<CATEGORY>/<name>.wav for tools/compare.
// --factory-loudness: the same measurement (loudest 400 ms, as the packs) as a report for tools/level_presets.py.
// Drums are left out: one-shots keep their own short-term targets (default audit).
int factoryWav (const juce::File& dir, const juce::String& onlyCategory)
{
    constexpr double sr = 48000.0;
    constexpr int block = 256;
    std::unique_ptr<juce::AudioProcessor> base (createPluginFilter());
    auto& proc = *dynamic_cast<Augur5Processor*> (base.get());
    proc.setPlayConfigDetails (0, 2, sr, block);
    proc.prepareToPlay (sr, block);
    auto& presets = proc.getPresets();
    std::printf ("%-24s %-12s %9s %9s %s\n", "preset", "category", "peak dB", "loud dB", "role");
    for (int i = 0; i < presets.getNumFactoryPresets(); ++i)
    {
        const auto category = presets.getFactoryCategory (i);
        const auto name = presets.getFactoryName (i);
        if ((onlyCategory.isNotEmpty() && ! category.equalsIgnoreCase (onlyCategory)) || category == "Drums" || name == "Init")
            continue;
        const auto role = factoryRole (category, name);
        presets.loadFactory (i);
        proc.prepareToPlay (sr, block); // fresh state: no tails from the previous sound
        juce::File wav;
        if (dir != juce::File())
        {
            wav = dir.getChildFile (category.toUpperCase()).getChildFile (juce::File::createLegalFileName (name) + ".wav");
            wav.getParentDirectory().createDirectory();
        }
        const auto m = packaudit::analyse (proc, role, role == "bass" ? 36 : 60, role == "pad" || role == "fx" ? 8.0 : 6.0, wav);
        std::printf ("%-24s %-12s %9.1f %9.1f %s\n", name.toRawUTF8(), category.toRawUTF8(), m.peakDb, m.loudDb, role.toRawUTF8());
        std::fflush (stdout);
    }
    return 0;
}

// --chord-headroom <folder> [--check]: a polysynth sound must not clip when it is played as chords. Every pack preset
// meant for chords is played as the factory audit plays them (four notes for pads, keys, brass and stabs; a dyad for
// plucks and arps) and, where the peak passes -1 dBFS, its amp_level comes down by just that much (up to three passes).
// Leads, basses, sequences, drones, FX and drums are single-note sounds: their pack levelling already covers them.
// --check only reports. Prints the worst first.
int chordHeadroom (const juce::File& folder, bool adjust)
{
    constexpr double sr = 48000.0, ceiling = -1.0;
    std::vector<std::pair<double, juce::String>> rows;
    int lowered = 0;
    for (const auto& f : folder.findChildFiles (juce::File::findFiles, true, "*.augur5"))
    {
        auto xml = juce::XmlDocument::parse (f);
        if (xml == nullptr)
            continue;
        // The role: the pack generator's META, or an expansion pack's category.
        juce::String role = xml->getStringAttribute ("category").toLowerCase();
        if (auto* meta = xml->getChildByName ("META"))
            role = meta->getStringAttribute ("role", role);
        const bool fourNotes = role == "pad" || role == "keys" || role == "brass" || role == "stab";
        const bool dyad = role == "pluck" || role == "arp";
        if (! fourNotes && ! dyad)
            continue;
        Measured m;
        for (int pass = 0; pass < 3; ++pass)
        {
            std::unique_ptr<juce::AudioProcessor> base (createPluginFilter());
            auto& proc = *dynamic_cast<Augur5Processor*> (base.get());
            proc.setPlayConfigDetails (0, 2, sr, 256);
            proc.prepareToPlay (sr, 256);
            proc.getPresets().loadUser (f);
            m = measure (proc, fourNotes ? "Pad" : "Pluck");
            if (! adjust || m.peakDb <= ceiling || ! m.finite)
                break;
            auto* params = xml->getChildByName ("PARAMS");
            if (params == nullptr)
                break;
            juce::XmlElement* level = nullptr;
            for (auto* e : params->getChildIterator())
                if (e->getStringAttribute ("id") == "amp_level")
                    level = e;
            if (level == nullptr)
            {
                level = params->createNewChildElement ("P");
                level->setAttribute ("id", "amp_level");
                level->setAttribute ("value", -6.0);
            }
            const double current = level->getDoubleAttribute ("value", -6.0);
            level->setAttribute ("value", juce::jlimit (-40.0, 6.0, std::floor ((current - (m.peakDb - ceiling) - 0.05) * 10.0) / 10.0));
            xml->writeTo (f);
            if (pass == 0)
                ++lowered;
        }
        rows.push_back ({ m.peakDb, f.getRelativePathFrom (folder) });
    }
    std::sort (rows.begin(), rows.end(), [] (const auto& a, const auto& b) { return a.first > b.first; });
    int over = 0;
    for (const auto& [peak, path] : rows)
        over += peak > ceiling ? 1 : 0;
    std::printf ("%d chord presets, %d lowered, %d still above %.0f dBFS as chords\n", static_cast<int> (rows.size()), lowered, over, ceiling);
    for (size_t i = 0; i < rows.size() && i < 10; ++i)
        std::printf ("%6.1f  %s\n", rows[i].first, rows[i].second.toRawUTF8());
    return over == 0 ? 0 : 1;
}

// --check-preset-clicks [out.wav]: a chord is held while factory pads are loaded, half a second apart, with the audio
// running on its own real-time thread exactly as a VST3 host runs it (the processor's callback lock; silence while
// suspended). Six pads, each loaded four times: the three reloads of the same pad (A -> A) can only differ by the
// change mechanism itself; the switches to a new pad (A -> B) are reported too.
// A click: in 1 ms windows, the peak second difference over the signal's own rms there (so a fade does not hide or fake
// one), against the median of that ratio over the 80 ms around it.
int checkPresetClicks (const juce::File& wavOut)
{
    constexpr double sr = 48000.0;
    constexpr int block = 256, changes = 24;
    std::unique_ptr<juce::AudioProcessor> base (createPluginFilter());
    auto& proc = *dynamic_cast<Augur5Processor*> (base.get());
    proc.setPlayConfigDetails (0, 2, sr, block);
    proc.prepareToPlay (sr, block);
    auto& presets = proc.getPresets();
    std::vector<int> pads;
    for (int i = 0; i < presets.getNumFactoryPresets(); ++i)
        if (presets.getFactoryCategory (i) == "Pad")
            pads.push_back (i);
    presets.loadFactory (pads[0]);

    const auto total = static_cast<size_t> ((changes + 2) * 0.5 * sr);
    std::vector<float> out (total, 0.0f);
    std::atomic<size_t> written { 0 };
    std::atomic<bool> running { true };
    std::thread audio ([&] {
        juce::AudioBuffer<float> buffer (2, block);
        auto deadline = std::chrono::steady_clock::now();
        bool first = true;
        while (running.load() && written.load() + block <= total)
        {
            juce::MidiBuffer midi;
            if (first)
                for (int n : { 48, 55, 60, 64 })
                    midi.addEvent (juce::MidiMessage::noteOn (1, n, static_cast<juce::uint8> (100)), 0);
            first = false;
            {
                const juce::ScopedLock lock (proc.getCallbackLock()); // as the VST3 wrapper does
                if (proc.isSuspended())
                    buffer.clear();
                else
                    proc.processBlock (buffer, midi);
            }
            const size_t at = written.load();
            for (int i = 0; i < block; ++i)
                out[at + static_cast<size_t> (i)] = buffer.getSample (0, i);
            written.store (at + block);
            deadline += std::chrono::microseconds (static_cast<long long> (block * 1.0e6 / sr));
            std::this_thread::sleep_until (deadline);
        }
    });

    std::vector<size_t> changeAt;
    std::vector<bool> reload;
    for (int c = 0; c < changes; ++c)
    {
        juce::MessageManager::getInstance()->runDispatchLoopUntil (500);
        changeAt.push_back (written.load());
        reload.push_back (c % 4 != 3);
        presets.loadFactory (pads[static_cast<size_t> ((c + 1) / 4) % pads.size()]);
    }
    juce::MessageManager::getInstance()->runDispatchLoopUntil (600);
    running.store (false);
    audio.join();
    const size_t n = written.load();
    int sounding = 0; // the held chord must survive every change (a warm-up must not silence a held voice)
    for (int v = 0; v < 8; ++v)
        sounding += proc.getVoiceLevel (v) > 1.0e-4f ? 1 : 0;

    // Per 1 ms window: peak second difference over the rms of the 6 ms around it.
    constexpr size_t win = 48;
    const size_t windows = n / win;
    std::vector<double> peak (windows, 0.0), power (windows, 0.0);
    for (size_t i = 2; i < n; ++i)
    {
        const size_t k = std::min (windows - 1, i / win);
        peak[k] = std::max (peak[k], std::abs (static_cast<double> (out[i]) - 2.0 * out[i - 1] + out[i - 2]));
        power[k] += static_cast<double> (out[i]) * out[i] / win;
    }
    double median = 0.0;
    {
        auto p = power;
        std::nth_element (p.begin(), p.begin() + static_cast<long> (p.size() / 2), p.end());
        median = p[p.size() / 2];
    }
    std::vector<double> ratio (windows, -1.0);
    for (size_t k = 3; k + 3 < windows; ++k)
    {
        double acc = 0.0;
        for (size_t j = k - 3; j <= k + 3; ++j)
            acc += power[j] / 7.0;
        if (acc > 1.0e-4 * median) // not in the silence of a fade
            ratio[k] = peak[k] / std::sqrt (acc);
    }
    int clicksReload = 0, clicksSwitch = 0, elsewhere = 0;
    double worstReload = 0.0, worstSwitch = 0.0;
    for (size_t k = 40; k + 40 < windows; ++k)
    {
        if (ratio[k] < 0.0)
            continue;
        std::vector<double> around;
        for (size_t j = k - 40; j <= k + 40; ++j)
            if ((j + 2 < k || j > k + 2) && ratio[j] >= 0.0)
                around.push_back (ratio[j]);
        if (around.size() < 20)
            continue;
        std::nth_element (around.begin(), around.begin() + static_cast<long> (around.size() / 2), around.end());
        const double score = ratio[k] / std::max (around[around.size() / 2], 1.0e-9);
        if (score < 6.0)
            continue;
        const size_t s = k * win;
        int change = -1;
        for (size_t c = 0; c < changeAt.size(); ++c)
            if (s + static_cast<size_t> (0.01 * sr) >= changeAt[c] && s <= changeAt[c] + static_cast<size_t> (0.15 * sr))
                change = static_cast<int> (c);
        if (change < 0)
            ++elsewhere;
        else if (reload[static_cast<size_t> (change)])
        {
            ++clicksReload;
            worstReload = std::max (worstReload, score);
        }
        else
        {
            ++clicksSwitch;
            worstSwitch = std::max (worstSwitch, score);
        }
    }
    if (wavOut != juce::File())
    {
        wavOut.deleteFile();
        juce::FileOutputStream os (wavOut);
        const auto bytes = static_cast<int> (n * sizeof (float));
        os.write ("RIFF", 4); os.writeInt (36 + bytes); os.write ("WAVEfmt ", 8); os.writeInt (16); os.writeShort (3); os.writeShort (1);
        os.writeInt (48000); os.writeInt (48000 * 4); os.writeShort (4); os.writeShort (32); os.write ("data", 4); os.writeInt (bytes);
        for (size_t i = 0; i < n; ++i)
            os.writeFloat (out[i]);
    }
    const int reloads = static_cast<int> (std::count (reload.begin(), reload.end(), true));
    std::printf ("preset changes: %d (%d reloads of the same pad, %d switches; chord held, real-time audio thread)\n", changes, reloads,
                 changes - reloads);
    std::printf ("clicks at a reload: %d (worst %.1fx)   at a switch: %d (worst %.1fx)   elsewhere: %d\n", clicksReload, worstReload,
                 clicksSwitch, worstSwitch, elsewhere);
    std::printf ("held notes still sounding after the changes: %d of 4\n", sounding);
    return clicksReload == 0 && clicksSwitch == 0 && sounding == 4 ? 0 : 1;
}

// --check-migration: a session and a user preset saved before 1.3 (no pb_down / lfo_div / lfo2_div) must load with the
// values that keep their sound: BEND DOWN = their single bend range, each synced LFO on the division its RATE knob chose.
int checkMigration()
{
    constexpr double sr = 48000.0;
    int failures = 0;
    const auto expect = [&failures] (bool ok, const juce::String& what) {
        std::printf ("%-78s %s\n", what.toRawUTF8(), ok ? "ok" : "FAIL");
        failures += ok ? 0 : 1;
    };
    const auto natural = [] (Augur5Processor& p, const char* id) { return p.getParameters().getRawParameterValue (id)->load(); };
    const int div1 = augur::legacyLfoDivisionFor (8.0f), div2 = augur::legacyLfoDivisionFor (0.3f);

    // A 1.2 session: today's state with the 1.3 settings taken out and the old ones set.
    {
        std::unique_ptr<juce::AudioProcessor> base (createPluginFilter());
        auto& proc = *dynamic_cast<Augur5Processor*> (base.get());
        proc.setPlayConfigDetails (0, 2, sr, 256);
        proc.prepareToPlay (sr, 256);
        auto tree = proc.getParameters().copyState();
        for (const char* id : { augur5::params::pb_down, augur5::params::lfo_div, augur5::params::lfo2_div })
            tree.removeChild (tree.getChildWithProperty ("id", id), nullptr);
        const auto set = [&tree] (const char* id, float v) { tree.getChildWithProperty ("id", id).setProperty ("value", v, nullptr); };
        set (augur5::params::pb_range, 12.0f);
        set (augur5::params::lfo_sync, 1.0f);
        set (augur5::params::lfo_rate, 8.0f);
        set (augur5::params::lfo2_sync, 1.0f);
        set (augur5::params::lfo2_rate, 0.3f);
        juce::MemoryBlock old;
        if (auto xml = tree.createXml())
            juce::AudioProcessor::copyXmlToBinary (*xml, old);

        std::unique_ptr<juce::AudioProcessor> other (createPluginFilter());
        auto& loaded = *dynamic_cast<Augur5Processor*> (other.get());
        loaded.setPlayConfigDetails (0, 2, sr, 256);
        loaded.prepareToPlay (sr, 256);
        loaded.setStateInformation (old.getData(), static_cast<int> (old.getSize()));
        expect (natural (loaded, augur5::params::pb_range) == 12.0f && natural (loaded, augur5::params::pb_down) == 12.0f,
                "1.2 session: bend range 12 -> BEND UP 12 and BEND DOWN 12");
        expect (static_cast<int> (natural (loaded, augur5::params::lfo_div)) == div1, "1.2 session: synced LFO keeps the division of RATE 8 Hz");
        expect (static_cast<int> (natural (loaded, augur5::params::lfo2_div)) == div2, "1.2 session: synced LFO 2 keeps the division of RATE 0.3 Hz");
    }

    // A 1.2 user preset file.
    {
        const auto file = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("augur_migration_check.augur5");
        file.replaceWithText ("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<AUGUR5_PRESET formatVersion=\"1\" name=\"Old\">\n  <PARAMS>\n"
                              "    <P id=\"pb_range\" value=\"7\"/>\n    <P id=\"lfo_sync\" value=\"1\"/>\n    <P id=\"lfo_rate\" value=\"8\"/>\n"
                              "  </PARAMS>\n</AUGUR5_PRESET>\n");
        std::unique_ptr<juce::AudioProcessor> base (createPluginFilter());
        auto& proc = *dynamic_cast<Augur5Processor*> (base.get());
        proc.setPlayConfigDetails (0, 2, sr, 256);
        proc.prepareToPlay (sr, 256);
        proc.getPresets().loadUser (file);
        file.deleteFile();
        expect (natural (proc, augur5::params::pb_down) == 7.0f, "1.2 preset: bend range 7 -> BEND DOWN 7");
        expect (static_cast<int> (natural (proc, augur5::params::lfo_div)) == div1, "1.2 preset: synced LFO keeps the division of RATE 8 Hz");
    }
    std::printf ("%s\n", failures == 0 ? "migration: all checks passed" : "migration: FAILURES");
    return failures == 0 ? 0 : 1;
}

// A preset's playing role and note (the META the pack tools write), "pad" on A3 when it has none.
std::pair<juce::String, int> roleOfPreset (const juce::XmlElement& preset)
{
    if (const auto* meta = preset.getChildByName ("META"))
        return { meta->getStringAttribute ("role", "pad"), meta->getIntAttribute ("note", 57) };
    return { "pad", 57 };
}

// Sounds that hold their notes in a flight (the others loop their phrase).
bool holdsNotes (const juce::String& role)
{
    return role == "pad" || role == "drone" || role == "fx" || role == "texture" || role == "arp";
}

// A held sound's level once it has settled: the role's notes held 12 s, measured over the last 4 s (its reverb full,
// its envelopes at rest), and the peak of the whole take.
packaudit::Metrics settledLevel (Augur5Processor& proc, const juce::String& role, int note)
{
    constexpr double sr = 48000.0;
    constexpr int block = 256, total = static_cast<int> (12.0 * sr), from = static_cast<int> (8.0 * sr);
    proc.prepareToPlay (sr, block);
    juce::AudioBuffer<float> buffer (2, block);
    double energy = 0.0, peak = 0.0;
    packaudit::Metrics m;
    for (int pos = 0; pos < total; pos += block)
    {
        juce::MidiBuffer midi;
        if (pos == 0)
            for (const auto& e : packaudit::patternFor (role, note))
                if (e.on)
                    midi.addEvent (juce::MidiMessage::noteOn (1, e.note, static_cast<juce::uint8> (100)), 0);
        buffer.clear();
        proc.processBlock (buffer, midi);
        for (int i = 0; i < block; ++i)
            for (int ch = 0; ch < 2; ++ch)
            {
                const double x = buffer.getSample (ch, i);
                m.finite = m.finite && std::isfinite (x);
                peak = std::max (peak, std::abs (x));
                if (pos + i >= from)
                    energy += x * x;
            }
    }
    juce::MidiBuffer off;
    off.addEvent (juce::MidiMessage::allNotesOff (1), 0);
    buffer.clear();
    proc.processBlock (buffer, off);
    m.loudDb = 10.0 * std::log10 (energy / (2.0 * (total - from)) + 1.0e-30);
    m.peakDb = 20.0 * std::log10 (peak + 1.0e-30);
    return m;
}

// --augury-scene <out.augur5> <A> <B> <C> <D>: an AUGURY scene preset made of four presets. A is the sound the preset
// plays (its file is kept as it is, with the scene added); B, C and D are captured into their corners level-matched to
// A (AMP LEVEL; a phrase of the preset's role, or a held sound once settled), so a flight moves the sound, not the volume.
int auguryScene (const juce::File& out, const std::array<juce::File, 4>& files)
{
    constexpr double sr = 48000.0;
    auto xml = juce::XmlDocument::parse (files[0]);
    if (xml == nullptr || ! xml->hasTagName ("AUGUR5_PRESET"))
        return 2;
    const auto [role, note] = roleOfPreset (*xml);
    std::unique_ptr<juce::AudioProcessor> base (createPluginFilter());
    auto& proc = *dynamic_cast<Augur5Processor*> (base.get());
    proc.setPlayConfigDetails (0, 2, sr, 256);
    auto& augury = proc.getAugury();
    auto* level = proc.getParameters().getParameter (augur5::params::amp_level);
    const auto measure = [&] {
        if (holdsNotes (role))
            return settledLevel (proc, role, note);
        proc.prepareToPlay (sr, 256);
        return packaudit::analyse (proc, role, note, 3.0);
    };
    const auto nudge = [level] (double db) {
        const float plain = juce::jlimit (-60.0f, 6.0f, level->convertFrom0to1 (level->getValue()) + static_cast<float> (db));
        level->setValueNotifyingHost (level->convertTo0to1 (plain));
    };
    double target = 0.0;
    for (int c = 0; c < augur5::AuguryModel::numCorners; ++c)
    {
        auto cornerXml = juce::XmlDocument::parse (files[static_cast<size_t> (c)]);
        if (cornerXml == nullptr)
            return 2;
        proc.getPresets().loadUser (files[static_cast<size_t> (c)]);
        auto m = measure();
        if (c == 0)
            target = m.loudDb;
        for (int pass = 0; c > 0 && pass < 4 && std::abs (target - m.loudDb) > 0.1; ++pass)
        {
            nudge (target - m.loudDb);
            m = measure();
        }
        // ...and its peaks below -1.5 dBFS (the packs keep chords below -1 dBFS; a flight blends the corners).
        if (c > 0 && m.peakDb > -1.5)
        {
            nudge (-1.5 - m.peakDb);
            m = measure();
        }
        const double loud = m.loudDb;
        const auto name = cornerXml->getStringAttribute ("name", files[static_cast<size_t> (c)].getFileNameWithoutExtension());
        augury.capture (c, name);
        std::printf ("  %c  %-28s level %6.1f dB  peak %5.1f dBFS  (AMP LEVEL %+.1f dB)\n", "ABCD"[c], name.toRawUTF8(), loud, m.peakDb,
                     level->convertFrom0to1 (level->getValue()));
    }
    augury.recall (0);
    while (auto* old = xml->getChildByName ("AUGURY"))
        xml->removeChildElement (old, true);
    xml->addChildElement (augury.toTree().createXml().release());
    out.getParentDirectory().createDirectory();
    return xml->writeTo (out) ? 0 : 1;
}

// --augury-flight <scene.augur5> <out.wav>: the scene played while the bird flies around the pad, as a user would
// fly it: 2 s on A, to B, 2 s on B, to D, on D, to C, on C, to the centre, on the centre (2 s per move), then the
// release. Pads, drones, textures, effects and arpeggios hold their notes; basses, leads, plucks, keys and brass
// loop their phrase. Prints, on each stop, the level against sound A held just as long (the bird left on A).
int auguryFlight (const juce::File& scene, const juce::File& wavFile)
{
    constexpr double sr = 48000.0;
    constexpr int block = 256;
    auto xml = juce::XmlDocument::parse (scene);
    if (xml == nullptr)
        return 2;
    const auto [role, note] = roleOfPreset (*xml);
    std::unique_ptr<juce::AudioProcessor> base (createPluginFilter());
    auto& proc = *dynamic_cast<Augur5Processor*> (base.get());
    proc.setPlayConfigDetails (0, 2, sr, block);
    proc.prepareToPlay (sr, block);
    proc.getPresets().loadUser (scene);
    auto& augury = proc.getAugury();
    if (augury.filledCount() < 2)
        return 3;
    proc.prepareToPlay (sr, block);

    struct Key { double t; float x, y; };
    const Key path[] { { 0, 0, 0 }, { 2, 0, 0 }, { 4, 1, 0 }, { 6, 1, 0 }, { 8, 1, 1 }, { 10, 1, 1 }, { 12, 0, 1 }, { 14, 0, 1 }, { 16, 0.5f, 0.5f }, { 18, 0.5f, 0.5f } };
    const auto birdAt = [&path] (double t) {
        for (size_t k = 1; k < std::size (path); ++k)
            if (t <= path[k].t)
            {
                const auto& a = path[k - 1];
                const auto& b = path[k];
                const float u = static_cast<float> ((t - a.t) / (b.t - a.t));
                return juce::Point<float> (a.x + u * (b.x - a.x), a.y + u * (b.y - a.y));
            }
        return juce::Point<float> (path[std::size (path) - 1].x, path[std::size (path) - 1].y);
    };
    constexpr double playEnd = 18.0, total = 22.0;
    const bool hold = holdsNotes (role);
    const auto phrase = packaudit::patternFor (role, note);
    double phraseEnd = 0.0;
    for (const auto& e : phrase)
        phraseEnd = std::max (phraseEnd, e.time);
    const double beat = 60.0 / 122.0, period = beat * std::ceil (phraseEnd / beat + 1.0e-6);

    // One take: the bird flies (or stays on A, the reference: a long note changes by itself as its reverb builds up and
    // its envelopes move, and the flight is measured against it, time for time). Returns the 100 ms window levels.
    const auto take = [&] (bool fly, std::vector<float>& L, std::vector<float>& R) {
        std::vector<double> windowDb;
        double acc = 0.0;
        int accN = 0;
        juce::AudioBuffer<float> buffer (2, block);
        augury.beginMorph();
        augury.setPosition ({ 0.0f, 0.0f });
        proc.prepareToPlay (sr, block);
        L.clear();
        R.clear();
        for (int pos = 0; pos < static_cast<int> (total * sr); pos += block)
        {
            const double t = pos / sr;
            if (fly && t <= playEnd)
                augury.setPosition (birdAt (t));
            juce::MidiBuffer midi;
            const auto add = [&] (double when, int n, bool on) {
                const int s = static_cast<int> (std::llround (when * sr));
                if (s >= pos && s < pos + block)
                    midi.addEvent (on ? juce::MidiMessage::noteOn (1, n, static_cast<juce::uint8> (100)) : juce::MidiMessage::noteOff (1, n), s - pos);
            };
            if (hold)
            {
                for (const auto& e : phrase)
                    if (e.on)
                    {
                        add (0.0, e.note, true);
                        add (playEnd, e.note, false);
                    }
            }
            else
            {
                for (double start = 0.0; start < playEnd - 0.5; start += period)
                    for (const auto& e : phrase)
                        if (! e.on || start + e.time < playEnd)
                            add (std::min (start + e.time, playEnd), e.note, e.on);
            }
            buffer.clear();
            proc.processBlock (buffer, midi);
            for (int i = 0; i < block; ++i)
            {
                L.push_back (buffer.getSample (0, i));
                R.push_back (buffer.getSample (1, i));
                acc += static_cast<double> (L.back()) * L.back() + static_cast<double> (R.back()) * R.back();
                if (++accN == 4800)
                {
                    windowDb.push_back (10.0 * std::log10 (acc / (2.0 * accN) + 1.0e-30));
                    acc = 0.0;
                    accN = 0;
                }
            }
        }
        augury.endMorph();
        return windowDb;
    };
    std::vector<float> L, R, stillL, stillR;
    const auto still = take (false, stillL, stillR);
    const auto flight = take (true, L, R);
    packaudit::writeWav (wavFile, L, R, sr);

    double peak = 0.0;
    bool finite = true;
    for (size_t i = 0; i < L.size(); ++i)
    {
        finite = finite && std::isfinite (L[i]) && std::isfinite (R[i]);
        peak = std::max ({ peak, static_cast<double> (std::abs (L[i])), static_cast<double> (std::abs (R[i])) });
    }
    const auto meanDb = [] (const std::vector<double>& windowDb, double from, double to) {
        double e = 0.0;
        int n = 0;
        for (size_t w = static_cast<size_t> (from * 10); w < static_cast<size_t> (to * 10) && w < windowDb.size(); ++w, ++n)
            e += std::pow (10.0, windowDb[w] / 10.0);
        return 10.0 * std::log10 (e / std::max (1, n) + 1.0e-30);
    };
    // On each stop: the flight's level, and how far it is from sound A at the same moment of the note.
    const double stops[][2] { { 4.5, 6.0 }, { 8.5, 10.0 }, { 12.5, 14.0 }, { 16.5, 18.0 } };
    const char* names[] { "B", "D", "C", "centre" };
    double worst = 0.0;
    std::printf ("  flight vs sound A held as long:");
    for (size_t s = 0; s < std::size (stops); ++s)
    {
        const double d = meanDb (flight, stops[s][0], stops[s][1]) - meanDb (still, stops[s][0], stops[s][1]);
        worst = std::max (worst, std::abs (d));
        std::printf ("  %s %+.1f", names[s], d);
    }
    std::printf (" dB   peak %.1f dBFS%s\n", 20.0 * std::log10 (peak + 1.0e-30), finite ? "" : "   NON-FINITE");
    return finite && peak < 1.0 && worst < 3.0 ? 0 : 1; // no clipping; no stop more than 3 dB from sound A
}

// --check-augury-gui: the AUGURY tab driven as a user drives it (its buttons, and the mouse on the morph pad of the real
// editor), and what that does to the parameters, to the sound and to UNDO.
int checkAuguryGui()
{
    constexpr double sr = 48000.0;
    int failures = 0;
    const auto expect = [&failures] (bool ok, const juce::String& what) {
        std::printf ("%-78s %s\n", what.toRawUTF8(), ok ? "ok" : "FAIL");
        failures += ok ? 0 : 1;
    };
    // Lets the APVTS move parameter changes into its state tree (with the undo manager), as the message loop does in a host.
    const auto pump = [] { juce::MessageManager::getInstance()->runDispatchLoopUntil (80); };

    std::unique_ptr<juce::AudioProcessor> base (createPluginFilter());
    auto& proc = *dynamic_cast<Augur5Processor*> (base.get());
    proc.setPlayConfigDetails (0, 2, sr, 256);
    proc.prepareToPlay (sr, 256);
    auto* editor = dynamic_cast<Augur5Editor*> (proc.createEditorIfNeeded());
    if (editor == nullptr)
        return 2;
    editor->setScale (1.0f);
    editor->showTab (5);

    // The pad and the buttons, found in the editor like a user finds them on screen.
    juce::Component* pad = nullptr;
    std::vector<juce::Button*> capture, recall, clear;
    juce::Button* omen = nullptr;
    std::function<void (juce::Component&)> walk = [&] (juce::Component& c) {
        if (c.getComponentID() == "augury-pad")
            pad = &c;
        if (auto* b = dynamic_cast<juce::Button*> (&c))
        {
            const auto t = b->getButtonText();
            if (t == "CAPTURE") capture.push_back (b);
            if (t == "RECALL") recall.push_back (b);
            if (t == "CLEAR") clear.push_back (b);
            if (t.startsWith ("CAST THE OMEN")) omen = b;
        }
        for (auto* child : c.getChildren())
            walk (*child);
    };
    walk (*editor);
    // (no window here, so "on screen" = visible all the way up to the editor, with a real size)
    const auto visibleInEditor = [editor] (juce::Component* c) {
        for (; c != nullptr && c != editor; c = c->getParentComponent())
            if (! c->isVisible())
                return false;
        return c == editor;
    };
    expect (pad != nullptr && visibleInEditor (pad) && pad->getWidth() > 400 && pad->getHeight() > 400,
            "the morph pad is on screen in the AUGURY tab");
    expect (capture.size() == 4 && recall.size() == 4 && clear.size() == 4 && omen != nullptr, "4 corners with CAPTURE / RECALL / CLEAR, and CAST THE OMEN");
    if (failures > 0)
        return 1;

    auto& presets = proc.getPresets();
    const auto values = [&proc] {
        std::vector<float> v;
        for (auto* p : proc.AudioProcessor::getParameters())
            v.push_back (p->getValue());
        return v;
    };
    const auto cutoff = [&proc] { return proc.getParameters().getParameter (augur5::params::flt_cutoff)->getValue(); };
    const auto measure = [&proc] {
        proc.prepareToPlay (sr, 256); // fresh state: every measurement starts from the same point
        return packaudit::analyse (proc, "pad", 57, 3.0);
    };
    const auto distance = [] (const packaudit::Metrics& a, const packaudit::Metrics& b) {
        double acc = 0.0;
        for (size_t i = 0; i < a.fingerprint.size() && i < b.fingerprint.size(); ++i)
            acc += (a.fingerprint[i] - b.fingerprint[i]) * (a.fingerprint[i] - b.fingerprint[i]);
        return std::sqrt (acc / static_cast<double> (std::max<size_t> (1, a.fingerprint.size())));
    };

    // Two contrasting sounds into corners A and D with the CAPTURE buttons.
    presets.loadFactory (presets.findFactory ("Warm Horizon"));
    pump();
    const auto valuesA = values();
    const float cutA = cutoff();
    const auto soundA = measure();
    capture[0]->onClick();
    presets.loadFactory (presets.findFactory ("Brass Section")); // POLY like A (a flight keeps the voice structure as played)
    pump();
    const auto valuesD = values();
    const float cutD = cutoff();
    const auto soundD = measure();
    capture[3]->onClick();
    expect (recall[0]->isEnabled() && recall[3]->isEnabled() && ! recall[1]->isEnabled() && ! recall[2]->isEnabled(),
            "CAPTURE filled corners A and D (their RECALL buttons are enabled)");

    // Fly the bird from A to D across the pad, then back to the middle, and let go.
    const auto field = pad->getLocalBounds().toFloat().reduced (44.0f);
    const auto at = [field] (float x, float y) { return juce::Point<float> (field.getX() + x * field.getWidth(), field.getY() + y * field.getHeight()); };
    auto source = juce::Desktop::getInstance().getMainMouseSource();
    const auto event = [&] (juce::Point<float> p, juce::Point<float> down, bool dragged) {
        const auto now = juce::Time::getCurrentTime();
        return juce::MouseEvent (source, p, {}, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, pad, pad, now, down, now, 1, dragged);
    };
    const auto start = at (0.0f, 0.0f);
    editor->mouseDown (event (start, start, false)); // the editor makes every gesture one undo step
    pad->mouseDown (event (start, start, false));
    expect (values() == valuesA, "bird on corner A: every parameter is exactly sound A");
    std::vector<float> cuts;
    std::vector<packaudit::Metrics> sounds;
    for (const float t : { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f })
    {
        pad->mouseDrag (event (at (t, t), start, true));
        cuts.push_back (cutoff());
        sounds.push_back (measure());
    }
    expect (values() == valuesD, "bird on corner D: every parameter is exactly sound D");
    bool monotonic = true;
    for (size_t i = 1; i < cuts.size(); ++i)
        monotonic = monotonic && (cutD >= cutA ? cuts[i] >= cuts[i - 1] : cuts[i] <= cuts[i - 1]);
    expect (monotonic && cutA != cutD, "on the way from A to D the cutoff moves steadily from A's to D's");
    expect (std::abs (cuts[2] - 0.5f * (cutA + cutD)) < 1.0e-4f, "in the middle the cutoff is the mean of the corners (normalised)");
    expect (distance (sounds[0], soundA) < 1.0e-3 && std::abs (sounds[0].loudDb - soundA.loudDb) < 0.01, "what plays at corner A is sound A");
    expect (distance (sounds[4], soundD) < 1.0e-3 && std::abs (sounds[4].loudDb - soundD.loudDb) < 0.01, "what plays at corner D is sound D");
    const double dMidA = distance (sounds[2], soundA), dMidD = distance (sounds[2], soundD);
    expect (dMidA > 0.5 && dMidD > 0.5, "the middle sounds different from both corners (a blend, not a switch)");
    std::printf ("    timbre distance  A..D %.2f   middle..A %.2f   middle..D %.2f   level A %.1f / middle %.1f / D %.1f dB\n",
                 distance (soundA, soundD), dMidA, dMidD, soundA.loudDb, sounds[2].loudDb, soundD.loudDb);
    for (size_t i = 0; i < sounds.size(); ++i)
        std::printf ("    t = %.2f   cutoff %.3f   brightness %5.0f Hz   level %6.1f dB\n", 0.25 * static_cast<double> (i), cuts[i],
                     sounds[i].centroid, sounds[i].loudDb);
    expect (std::all_of (sounds.begin(), sounds.end(), [] (const packaudit::Metrics& m) { return m.finite && m.peakDb < 0.0; }),
            "every point of the flight is finite and below 0 dBFS");
    pad->mouseDrag (event (at (0.5f, 0.5f), start, true));
    const auto middle = values();
    pad->mouseUp (event (at (0.5f, 0.5f), start, true));
    pump();

    // UNDO: the whole flight is one step back, to the sound that was playing before it.
    proc.getUndoManager().undo();
    pump();
    expect (values() == valuesD, "UNDO takes the whole flight back in one step");
    proc.getUndoManager().redo();
    pump();
    expect (values() == middle, "REDO brings the blend back");

    // A chord held while the bird flies (2 s) from A (a POLY pad) to C (a UNISON lead), then 1 s on C: the chord must
    // keep sounding all the way. Measured in 50 ms windows against its level before the flight.
    {
        presets.loadFactory (presets.findFactory ("Savanna Horn Lead"));
        pump();
        capture[2]->onClick();
        recall[0]->onClick(); // sound A, POLY
        const auto c0 = at (0.0f, 0.0f);
        pad->mouseDown (event (c0, c0, false));
        proc.prepareToPlay (sr, 256);
        constexpr int block = 256, window = 2400;
        juce::AudioBuffer<float> buffer (2, block);
        std::vector<double> level;
        double acc = 0.0;
        int accN = 0;
        float lastU = 0.0f;
        for (int pos = 0; pos < static_cast<int> (4.0 * sr); pos += block)
        {
            const double t = pos / sr;
            if (const float u = static_cast<float> (juce::jlimit (0.0, 1.0, (t - 1.0) / 2.0)); u != lastU)
            {
                pad->mouseDrag (event (at (0.0f, u), c0, true));
                lastU = u;
            }
            juce::MidiBuffer midi;
            if (pos == 0)
                for (const int n : { 57, 60, 64, 67 })
                    midi.addEvent (juce::MidiMessage::noteOn (1, n, static_cast<juce::uint8> (100)), 0);
            buffer.clear();
            proc.processBlock (buffer, midi);
            for (int i = 0; i < block; ++i)
            {
                acc += static_cast<double> (buffer.getSample (0, i)) * buffer.getSample (0, i) + static_cast<double> (buffer.getSample (1, i)) * buffer.getSample (1, i);
                if (++accN == window)
                {
                    level.push_back (10.0 * std::log10 (acc / (2.0 * window) + 1.0e-30));
                    acc = 0.0;
                    accN = 0;
                }
            }
        }
        pad->mouseUp (event (at (0.0f, 1.0f), c0, true));
        juce::MidiBuffer off;
        off.addEvent (juce::MidiMessage::allNotesOff (1), 0);
        buffer.clear();
        proc.processBlock (buffer, off);
        pump();
        // windows: 20 per second; before = 0.5..1 s, the flight = 1..3 s, on C = 3..4 s
        double before = 0.0;
        for (size_t w = 10; w < 20; ++w)
            before += level[w] / 10.0;
        double lowest = 1.0e9;
        for (size_t w = 20; w < level.size(); ++w)
            lowest = std::min (lowest, level[w]);
        std::printf ("    held chord, level every 0.25 s (dB):");
        for (size_t w = 0; w < level.size(); w += 5)
            std::printf (" %.0f", level[w]);
        std::printf ("\n    before the flight %.1f dB, lowest during the flight and on C %.1f dB\n", before, lowest);
        expect (lowest > before - 20.0, "a chord held during a flight from a POLY corner to a UNISON one keeps sounding");
        clear[2]->onClick();
    }

    // RECALL, OMEN, CLEAR.
    recall[0]->onClick();
    expect (values() == valuesA && proc.getAugury().getPosition() == juce::Point<float> (0.0f, 0.0f), "RECALL A plays sound A and puts the bird there");
    const auto beforeOmen = values();
    omen->onClick();
    expect (values() != beforeOmen && proc.getAugury().getLastSeed() > 0, "CAST THE OMEN changes the sound and numbers the omen");
    clear[3]->onClick();
    expect (! proc.getAugury().isFilled (3) && ! recall[3]->isEnabled(), "CLEAR D empties the corner (RECALL D disabled)");
    const auto beforeLone = values();
    pad->mouseDown (event (at (1.0f, 1.0f), at (1.0f, 1.0f), false));
    pad->mouseDrag (event (at (0.7f, 0.3f), at (1.0f, 1.0f), true));
    pad->mouseUp (event (at (0.7f, 0.3f), at (1.0f, 1.0f), true));
    expect (values() == beforeLone, "with a single corner left, flying the bird changes nothing");

    // A preset with an AUGURY scene, loaded while the tab is open: the tab shows its corners (no reopening).
    {
        presets.loadFactory (presets.findFactory ("Brass Section"));
        pump();
        capture[1]->onClick();
        const juce::String name ("zz AUGURY tab check");
        const bool saved = presets.saveUser (name);
        const auto file = augur5::PresetManager::getUserFolder().getChildFile (name + ".augur5");
        for (auto* b : clear)
            b->onClick();
        const bool emptied = ! recall[0]->isEnabled() && ! recall[1]->isEnabled();
        presets.loadUser (file);
        pump(); // the editor's timer polls the tab
        expect (saved && emptied && recall[0]->isEnabled() && recall[1]->isEnabled() && ! recall[2]->isEnabled() && ! recall[3]->isEnabled(),
                "loading a preset with a scene shows its corners in the open tab");
        file.deleteFile();
    }

    base->editorBeingDeleted (editor);
    delete editor;
    std::printf ("%s\n", failures == 0 ? "AUGURY (GUI): all checks passed" : "AUGURY (GUI): FAILURES");
    return failures == 0 ? 0 : 1;
}

// --check-skins: the SETTINGS skin buttons clicked as a user clicks them, with two editors open (two instances in a host):
// both windows rebuild in the new skin and keep their tab and their open panel, the choice is remembered, and coming back
// to LIGHT paints exactly what LIGHT painted before (no colour left behind). The user's saved skin is put back afterwards.
int checkSkins()
{
    int failures = 0;
    const auto expect = [&failures] (bool ok, const juce::String& what) {
        std::printf ("%-78s %s\n", what.toRawUTF8(), ok ? "ok" : "FAIL");
        failures += ok ? 0 : 1;
    };
    const auto pump = [] { juce::MessageManager::getInstance()->runDispatchLoopUntil (150); };
    const auto settings = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                              .getChildFile ("TONAL LAB")
                              .getChildFile ("AUGUR-5")
                              .getChildFile ("settings.xml");
    const bool hadSettings = settings.existsAsFile();
    juce::MemoryBlock saved;
    if (hadSettings)
        settings.loadFileAsData (saved);

    Augur5Editor::previewSkin (0);
    std::unique_ptr<juce::AudioProcessor> procA (createPluginFilter()), procB (createPluginFilter());
    for (auto* p : { procA.get(), procB.get() })
    {
        p->setPlayConfigDetails (0, 2, 48000.0, 256);
        p->prepareToPlay (48000.0, 256);
    }
    auto* a = dynamic_cast<Augur5Editor*> (procA->createEditorIfNeeded());
    auto* b = dynamic_cast<Augur5Editor*> (procB->createEditorIfNeeded());
    if (a == nullptr || b == nullptr)
        return 2;
    for (auto* e : { a, b })
    {
        e->setScale (1.0f);
        e->setScaleFactor (1.0f);
    }
    a->showTab (2);     // ARP, with SETTINGS open on top
    a->showOverlay (1);
    b->showTab (4);     // FX

    const auto shot = [] (juce::Component& c) { return c.createComponentSnapshot (c.getLocalBounds(), true, 1.0f); };
    // Pixels that differ, outside the horizon (it is animated: its sky and ridges move with time). A colour left behind
    // by a dark skin changes a pixel by far more than 12/255; less is anti-aliasing (Direct2D's caches can round an
    // edge pixel of a curve differently once IMPERIAL's mask has been drawn: seen once, 7/255).
    const auto differs = [] (juce::Colour p, juce::Colour q) {
        return std::abs (p.getRed() - q.getRed()) > 12 || std::abs (p.getGreen() - q.getGreen()) > 12
               || std::abs (p.getBlue() - q.getBlue()) > 12 || std::abs (p.getAlpha() - q.getAlpha()) > 12;
    };
    const auto differing = [&differs] (const juce::Image& x, const juce::Image& y) {
        if (x.getBounds() != y.getBounds())
            return -1;
        const juce::Rectangle<int> horizon (388, 778, 760, 186);
        int n = 0;
        for (int py = 0; py < x.getHeight(); ++py)
            for (int px = 0; px < x.getWidth(); ++px)
                if (! horizon.contains (px, py) && differs (x.getPixelAt (px, py), y.getPixelAt (px, py)))
                    ++n;
        return n;
    };
    // The skin buttons, found in the open SETTINGS panel as a user finds them on screen.
    // (no window here, so "on screen" = visible all the way up to the editor)
    const auto skinButton = [] (juce::Component& root, const juce::String& text) {
        const auto visibleIn = [&root] (juce::Component* c) {
            for (; c != nullptr && c != &root; c = c->getParentComponent())
                if (! c->isVisible())
                    return false;
            return c == &root;
        };
        juce::Button* found = nullptr;
        std::function<void (juce::Component&)> walk = [&] (juce::Component& c) {
            if (auto* btn = dynamic_cast<juce::Button*> (&c); btn != nullptr && btn->getButtonText() == text && visibleIn (btn))
                found = btn;
            for (auto* child : c.getChildren())
                walk (*child);
        };
        walk (root);
        return found;
    };

    pump(); // the editors' first poll (the FX chain lights the units that are on)
    const auto lightA = shot (*a), lightB = shot (*b);
    const char* names[] { "LIGHT", "NEON", "IMPERIAL" };
    for (const int s : { 1, 2, 0 })
    {
        const juce::String name (names[s]);
        auto* button = skinButton (*a, name);
        expect (button != nullptr, "SETTINGS shows the " + name + " button");
        if (button == nullptr)
            break;
        button->onClick();
        pump();
        expect (static_cast<int> (augur5::ui::currentSkin()) == s, name + ": the skin is applied");
        expect (static_cast<int> (augur5::ui::loadSavedSkin()) == s, name + ": the choice is remembered (settings.xml)");
        expect (skinButton (*a, name) != nullptr && skinButton (*a, name)->getToggleState(),
                name + ": the rebuilt window still has SETTINGS open, with " + name + " lit");
        const auto imgA = shot (*a), imgB = shot (*b);
        const int dA = differing (imgA, lightA), dB = differing (imgB, lightB);
        if (s == 0)
        {
            expect (dA == 0 && dB == 0, "back to LIGHT: both windows paint exactly as before (" + juce::String (dA) + " / "
                                            + juce::String (dB) + " pixels differ)");
            for (auto [before, after] : { std::pair { &lightA, &imgA }, std::pair { &lightB, &imgB } })
            {
                juce::Rectangle<int> where;
                for (int py = 0; py < after->getHeight(); ++py)
                    for (int px = 0; px < after->getWidth(); ++px)
                        if (! juce::Rectangle<int> (388, 778, 760, 186).contains (px, py) && differs (before->getPixelAt (px, py), after->getPixelAt (px, py)))
                            where = where.isEmpty() ? juce::Rectangle<int> (px, py, 1, 1) : where.getUnion ({ px, py, 1, 1 });
                if (! where.isEmpty())
                    std::printf ("    differing area x %d..%d  y %d..%d  (first pixel %s before, %s after)\n", where.getX(), where.getRight(),
                                 where.getY(), where.getBottom(), before->getPixelAt (where.getX(), where.getY()).toDisplayString (true).toRawUTF8(),
                                 after->getPixelAt (where.getX(), where.getY()).toDisplayString (true).toRawUTF8());
            }
        }
        else
            expect (dA > 100000 && dB > 100000, name + ": both open windows repainted (" + juce::String (dA) + " / " + juce::String (dB)
                                                     + " pixels changed)");
    }

    for (auto [proc, editor] : { std::pair { procA.get(), a }, std::pair { procB.get(), b } })
    {
        proc->editorBeingDeleted (editor);
        delete editor;
    }
    if (hadSettings)
        settings.replaceWithData (saved.getData(), saved.getSize());
    else
        settings.deleteFile();
    std::printf ("%s\n", failures == 0 ? "skins: all checks passed" : "skins: FAILURES");
    return failures == 0 ? 0 : 1;
}

// --paint-bench [scale]: how long the editor takes to paint in each skin, at a window scale (default 1.0): the whole
// window, and the small repaints the 30 Hz timer makes all the time (a knob, the wheels and LEDs, the horizon).
int paintBench (float scale)
{
    const juce::Rectangle<int> regions[] { { 0, 0, 1536, 1024 }, { 620, 170, 60, 60 }, { 70, 790, 300, 140 }, { 388, 778, 760, 186 } };
    const char* regionNames[] { "whole window", "one knob", "wheels and LEDs", "horizon" };
    for (int skin = 0; skin < augur5::ui::numSkins; ++skin)
    {
        Augur5Editor::previewSkin (skin);
        std::unique_ptr<juce::AudioProcessor> base (createPluginFilter());
        base->setPlayConfigDetails (0, 2, 48000.0, 256);
        base->prepareToPlay (48000.0, 256);
        auto* editor = dynamic_cast<Augur5Editor*> (base->createEditorIfNeeded());
        editor->setScaleFactor (1.0f);
        editor->setScale (scale);
        juce::Image image (juce::Image::ARGB, editor->getWidth(), editor->getHeight(), true);
        std::printf ("%-9s", augur5::ui::skinName (static_cast<augur5::ui::Skin> (skin)));
        {
            const auto start = juce::Time::getHighResolutionTicks();
            juce::Graphics g (image);
            editor->paintEntireComponent (g, true);
            std::printf ("   first paint %6.1f ms", juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - start) * 1000.0);
        }
        for (size_t r = 0; r < std::size (regions); ++r)
        {
            const auto area = (regions[r].toFloat() * scale).getSmallestIntegerContainer();
            const int runs = r == 0 ? 10 : 60;
            const auto start = juce::Time::getHighResolutionTicks();
            for (int i = 0; i < runs; ++i)
            {
                juce::Graphics g (image);
                g.reduceClipRegion (area);
                editor->paintEntireComponent (g, true);
            }
            const double ms = juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - start) * 1000.0 / runs;
            std::printf ("   %s %6.2f ms", regionNames[r], ms);
        }
        std::printf ("\n");
        base->editorBeingDeleted (editor);
        delete editor;
    }
    return 0;
}

int main (int argc, char** argv)
{
    if (argc >= 2 && std::strcmp (argv[1], "--params") == 0)
        return dumpParams();
    if (argc >= 3 && std::strcmp (argv[1], "--pack") == 0)
    {
        juce::File wavDir;
        bool dry = false;
        for (int a = 3; a < argc; ++a)
        {
            if (std::strcmp (argv[a], "--dry") == 0)
                dry = true;
            else if (std::strcmp (argv[a], "--wav") == 0 && a + 1 < argc)
                wavDir = juce::File::getCurrentWorkingDirectory().getChildFile (juce::String::fromUTF8 (argv[++a]));
        }
        return packAudit (juce::File::getCurrentWorkingDirectory().getChildFile (juce::String::fromUTF8 (argv[2])), wavDir, dry);
    }
    if (argc >= 3 && std::strcmp (argv[1], "--stereo-report") == 0)
        return stereoReport (juce::String::fromUTF8 (argv[2]));
    if (argc >= 3 && std::strcmp (argv[1], "--logo") == 0)
        return exportLogo (juce::File::getCurrentWorkingDirectory().getChildFile (juce::String::fromUTF8 (argv[2])));
    if (argc >= 2 && std::strcmp (argv[1], "--check-augury") == 0)
        return checkAugury();
    juce::ScopedJuceInitialiser_GUI juceInit;
    if (argc >= 3 && std::strcmp (argv[1], "--chord-headroom") == 0)
        return chordHeadroom (juce::File::getCurrentWorkingDirectory().getChildFile (juce::String::fromUTF8 (argv[2])),
                              ! (argc >= 4 && std::strcmp (argv[3], "--check") == 0));
    if (argc >= 2 && std::strcmp (argv[1], "--check-preset-clicks") == 0)
        return checkPresetClicks (argc >= 3 ? juce::File::getCurrentWorkingDirectory().getChildFile (juce::String::fromUTF8 (argv[2])) : juce::File());
    if (argc >= 2 && std::strcmp (argv[1], "--check-migration") == 0)
        return checkMigration();
    if (argc >= 2 && std::strcmp (argv[1], "--factory-loudness") == 0)
        return factoryWav ({}, argc >= 3 ? juce::String::fromUTF8 (argv[2]) : juce::String());
    if (argc >= 3 && std::strcmp (argv[1], "--factory-wav") == 0)
        return factoryWav (juce::File::getCurrentWorkingDirectory().getChildFile (juce::String::fromUTF8 (argv[2])),
                           argc >= 4 ? juce::String::fromUTF8 (argv[3]) : juce::String());
    if (argc >= 7 && std::strcmp (argv[1], "--augury-scene") == 0)
        return auguryScene (juce::File::getCurrentWorkingDirectory().getChildFile (argv[2]),
                            { juce::File::getCurrentWorkingDirectory().getChildFile (argv[3]), juce::File::getCurrentWorkingDirectory().getChildFile (argv[4]),
                              juce::File::getCurrentWorkingDirectory().getChildFile (argv[5]), juce::File::getCurrentWorkingDirectory().getChildFile (argv[6]) });
    if (argc >= 4 && std::strcmp (argv[1], "--augury-flight") == 0)
        return auguryFlight (juce::File::getCurrentWorkingDirectory().getChildFile (argv[2]), juce::File::getCurrentWorkingDirectory().getChildFile (argv[3]));
    if (argc >= 2 && std::strcmp (argv[1], "--check-augury-gui") == 0)
        return checkAuguryGui();
    if (argc >= 2 && std::strcmp (argv[1], "--check-skins") == 0)
        return checkSkins();
    if (argc >= 2 && std::strcmp (argv[1], "--paint-bench") == 0)
        return paintBench (argc >= 3 ? static_cast<float> (std::atof (argv[2])) : 1.0f);
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
        return levelPack (juce::File::getCurrentWorkingDirectory().getChildFile (juce::String::fromUTF8 (argv[2])),
                          argc >= 4 && std::strcmp (argv[3], "--pack-scale") == 0);
    if (argc >= 3 && std::strcmp (argv[1], "--snapshot") == 0)
        return snapshot (juce::String::fromUTF8 (argv[2]), argc >= 4 ? static_cast<float> (std::atof (argv[3])) : 1.0f, argc >= 5 ? std::atoi (argv[4]) : 0,
                         argc >= 6 ? std::atoi (argv[5]) : -1);
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
