#pragma once

#include <juce_core/juce_core.h>

#include <algorithm>
#include <array>

namespace augur5
{

// Lock-free bridge audio thread -> GUI (single producer, single consumer). The processor pushes its output at
// the end of every block; the horizon pulls it at frame rate. When the GUI is late, samples are dropped.
class AudioTap
{
public:
    // Audio thread: wait-free.
    void push (const float* const* channels, int numChannels, int numSamples) noexcept
    {
        if (numChannels <= 0 || numSamples <= 0)
            return;
        int s1, n1, s2, n2;
        fifo.prepareToWrite (numSamples, s1, n1, s2, n2);
        const float gain = 1.0f / static_cast<float> (numChannels);
        const auto write = [&] (int start, int count, int offset) {
            for (int i = 0; i < count; ++i)
            {
                float v = 0.0f;
                for (int c = 0; c < numChannels; ++c)
                    v += channels[c][offset + i];
                data[static_cast<size_t> (start + i)] = v * gain;
            }
        };
        write (s1, n1, 0);
        write (s2, n2, n1);
        fifo.finishedWrite (n1 + n2);
    }

    // GUI thread.
    int pull (float* dest, int maxSamples) noexcept
    {
        int s1, n1, s2, n2;
        fifo.prepareToRead (maxSamples, s1, n1, s2, n2);
        std::copy_n (data.begin() + s1, n1, dest);
        std::copy_n (data.begin() + s2, n2, dest + n1);
        fifo.finishedRead (n1 + n2);
        return n1 + n2;
    }

private:
    static constexpr int size = 16384;
    juce::AbstractFifo fifo { size };
    std::array<float, static_cast<size_t> (size)> data {};
};

// Lock-free queue of short MIDI messages from the GUI (on-screen keys, wheels) to the audio thread.
class UiMidiQueue
{
public:
    // Message thread.
    void push (juce::uint8 status, juce::uint8 data1, juce::uint8 data2) noexcept
    {
        int s1, n1, s2, n2;
        fifo.prepareToWrite (1, s1, n1, s2, n2);
        if (n1 > 0)
            messages[static_cast<size_t> (s1)] = { status, data1, data2 };
        fifo.finishedWrite (n1);
    }

    // Audio thread: calls fn (const juce::uint8* bytes, int numBytes) for every queued message.
    template <typename Fn>
    void drain (Fn&& fn) noexcept
    {
        int s1, n1, s2, n2;
        fifo.prepareToRead (fifo.getNumReady(), s1, n1, s2, n2);
        for (int i = 0; i < n1; ++i)
            fn (messages[static_cast<size_t> (s1 + i)].data(), 3);
        for (int i = 0; i < n2; ++i)
            fn (messages[static_cast<size_t> (s2 + i)].data(), 3);
        fifo.finishedRead (n1 + n2);
    }

private:
    static constexpr int size = 512;
    juce::AbstractFifo fifo { size };
    std::array<std::array<juce::uint8, 3>, static_cast<size_t> (size)> messages {};
};

} // namespace augur5
