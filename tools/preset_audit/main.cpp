// augur_preset_audit: loads the real plugin processor (as a host would), plays every factory preset
// with notes that suit its category and reports its level. Fails on silence, non-finite output or
// clipping, so the factory library can be checked after every change.

#include "LegacyEffects.h"
#include "PackAnalysis.h"
#include "gui/Theme.h"
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
// 10..18 = the FX tab showing effect 0..8, 19 = the MOD tab with a busy test matrix.
int snapshot (const juce::String& path, float scale, int tab)
{
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

// --logo dir: writes the brand images: the full logo on white (4x), the mark alone on paper (1024 px, also the app
// icon) and the mark on dark (for dark backgrounds).
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

    // The mark alone: paper tile (app icon) and on dark.
    for (const bool dark : { false, true })
    {
        constexpr int size = 1024;
        juce::Image image (juce::Image::ARGB, size, size, true);
        {
            juce::Graphics g (image);
            const auto tile = juce::Rectangle<float> (0.0f, 0.0f, static_cast<float> (size), static_cast<float> (size)).reduced (40.0f);
            g.setColour (dark ? juce::Colour (0xff17181b) : juce::Colour (0xfffbf9f5));
            g.fillRoundedRectangle (tile, 190.0f);
            g.setColour (dark ? juce::Colour (0xff2a2b30) : augur5::ui::colours::panelBorder);
            g.drawRoundedRectangle (tile.reduced (2.0f), 188.0f, 4.0f);
            augur5::ui::drawAugurMark (g, tile.getCentre(), 330.0f, dark ? juce::Colour (0xffeeeae3) : augur5::ui::colours::ink,
                                       augur5::ui::colours::accent);
        }
        ok = write (image, dir.getChildFile (dark ? "AUGUR-5 mark (dark).png" : "AUGUR-5 mark.png")) && ok;
    }
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
    if (argc >= 3 && std::strcmp (argv[1], "--logo") == 0)
        return exportLogo (juce::File::getCurrentWorkingDirectory().getChildFile (juce::String::fromUTF8 (argv[2])));
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
