// augur_preset_audit: loads the real plugin processor (as a host would), plays every factory preset
// with notes that suit its category and reports its level. Fails on silence, non-finite output or
// clipping, so the factory library can be checked after every change.

#include "PluginProcessor.h"

#include <cmath>
#include <cstdio>
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

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    constexpr double sr = 48000.0;
    constexpr int block = 256;
    constexpr int window = 2400; // 50 ms short-term loudness window

    std::unique_ptr<juce::AudioProcessor> first (createPluginFilter());
    const int numPresets = dynamic_cast<Augur5Processor*> (first.get())->getPresets().getNumFactoryPresets();
    first.reset();

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
    return failures == 0 ? 0 : 1;
}
