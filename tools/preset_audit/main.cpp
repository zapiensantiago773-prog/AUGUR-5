// augur_preset_audit: loads the real plugin processor (as a host would), plays every factory preset
// with notes that suit its category and reports its level. Fails on silence, non-finite output or
// clipping, so the factory library can be checked after every change.

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

// --snapshot out.png [scale]: renders the plugin editor (design canvas) to a PNG, for layout review.
int snapshot (const juce::String& path, float scale)
{
    std::unique_ptr<juce::AudioProcessor> base (createPluginFilter());
    base->setPlayConfigDetails (0, 2, 48000.0, 256);
    base->prepareToPlay (48000.0, 256);
    auto* editor = base->createEditorIfNeeded();
    editor->setScaleFactor (1.0f);
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

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    if (argc >= 3 && std::strcmp (argv[1], "--snapshot") == 0)
        return snapshot (juce::String::fromUTF8 (argv[2]), argc >= 4 ? static_cast<float> (std::atof (argv[3])) : 1.0f);
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
