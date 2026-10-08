#pragma once

// Pack analysis for augur_preset_audit --pack (the TONAL LAB atlas pipeline, as in MANTIS-37 and PYTHIA 32): plays a
// user preset the way its role is used in a track and measures it. One CSV line per preset; tools/pack/
// make_sun_atlas.py reads it to level, to quality-gate and to tell sounds apart.

#include "PluginProcessor.h"

#include <juce_audio_formats/juce_audio_formats.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <memory>
#include <vector>

namespace packaudit
{

struct Metrics
{
    double peakDb = -240, loudDb = -240, centroid = 0, low = 0, high = 0, dc = 0, width = 0, tail = 0, attack = 0;
    bool finite = true;
    // Timbre fingerprint (loudness independent), used to find presets that sound alike:
    //   [0..31]  spectral shape: mean dB in 32 log bands 60 Hz..16 kHz, minus their mean
    //   [32..47] articulation: RMS envelope over 16 slices of the phrase, dB re its maximum
    //   [48..63] brightness over time: centroid (log2 Hz) over the same 16 slices, minus its mean
    //   [64] spectral flux (movement), [65] stereo width
    std::vector<float> fingerprint;
};

inline void writeWav (const juce::File& file, const std::vector<float>& L, const std::vector<float>& R, double sr)
{
    file.getParentDirectory().createDirectory();
    file.deleteFile();
    juce::AudioBuffer<float> b (2, static_cast<int> (L.size()));
    b.copyFrom (0, 0, L.data(), static_cast<int> (L.size()));
    b.copyFrom (1, 0, R.data(), static_cast<int> (R.size()));
    juce::WavAudioFormat wav;
    if (auto stream = std::unique_ptr<juce::OutputStream> (file.createOutputStream()))
        if (auto writer = std::unique_ptr<juce::AudioFormatWriter> (wav.createWriterFor (stream.get(), sr, 2, 24, {}, 0)))
        {
            stream.release(); // owned by the writer now
            writer->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
        }
}

inline void fft (std::vector<std::complex<double>>& a)
{
    const size_t n = a.size();
    for (size_t i = 1, j = 0; i < n; ++i)
    {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1)
            j ^= bit;
        j ^= bit;
        if (i < j)
            std::swap (a[i], a[j]);
    }
    for (size_t len = 2; len <= n; len <<= 1)
    {
        const double ang = -2.0 * 3.14159265358979323846 / static_cast<double> (len);
        const std::complex<double> wl (std::cos (ang), std::sin (ang));
        for (size_t i = 0; i < n; i += len)
        {
            std::complex<double> w (1.0);
            for (size_t j = 0; j < len / 2; ++j)
            {
                const auto u = a[i + j], v = a[i + j + len / 2] * w;
                a[i + j] = u + v;
                a[i + j + len / 2] = u - v;
                w *= wl;
            }
        }
    }
}

struct Event
{
    double time;
    int note;
    bool on;
};

// How each role is played, at 122 BPM (a sixteenth = 0.123 s). A polysynth: pads, keys and brass are chords.
inline std::vector<Event> patternFor (const juce::String& role, int root)
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
    if (role == "bass")
    {
        // Rolling sixteenths with the kick gap (-xxx -xxx ...), two bars.
        for (int i = 0; i < 32; ++i)
            if (i % 4 != 0)
                note (i * s16, root + ((i % 16 == 14) ? 3 : 0), s16 * 0.6);
    }
    else if (role == "lead")
    {
        note (0.0, root, 0.9);
        note (0.9, root + 3, 0.6);
        note (1.5, root + 7, 1.2);
    }
    else if (role == "pluck")
    {
        const int line[] = { 0, 7, 3, 10, 0, 7, 12, 3 };
        for (int i = 0; i < 8; ++i)
            note (i * 3.0 * s16, root + line[i], s16 * 1.2);
    }
    else if (role == "keys")
    {
        // Four chord stabs: i - VI - VII - i (minor), half a second apart.
        chord (0.0, root, { 0, 3, 7, 10 }, 0.42);
        chord (0.5, root, { -4, 0, 3, 7 }, 0.42);
        chord (1.0, root, { -2, 2, 5, 9 }, 0.42);
        chord (1.5, root, { 0, 3, 7, 12 }, 0.7);
    }
    else if (role == "brass")
    {
        chord (0.0, root, { 0, 7, 12, 16 }, 0.9);
        chord (1.0, root, { -2, 5, 10, 14 }, 1.2);
    }
    else if (role == "arp")
    {
        chord (0.0, root, { 0, 3, 7 }, 3.0);
    }
    else if (role == "seq" || role == "drone" || role == "fx")
    {
        note (0.0, root, 3.0);
    }
    else // pad
    {
        chord (0.0, root, { 0, 3, 7, 10 }, 3.0);
    }
    return ev;
}

inline std::vector<float> fingerprint (const std::vector<float>& L, const std::vector<float>& R, size_t offSample, double sr, double width);

inline Metrics analyse (Augur5Processor& proc, const juce::String& role, int root, double seconds, const juce::File& wavFile = {})
{
    constexpr double sr = 48000.0;
    constexpr int block = 256;
    const auto events = patternFor (role, root);
    double lastOff = 0.0;
    for (const auto& e : events)
        if (! e.on)
            lastOff = std::max (lastOff, e.time);

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
                midi.addEvent (e.on ? juce::MidiMessage::noteOn (1, e.note, static_cast<juce::uint8> (100)) : juce::MidiMessage::noteOff (1, e.note),
                               s - pos);
        }
        buffer.clear();
        proc.processBlock (buffer, midi);
        const int n = std::min (block, total - pos);
        for (int i = 0; i < n; ++i)
        {
            L[static_cast<size_t> (pos + i)] = buffer.getSample (0, i);
            R[static_cast<size_t> (pos + i)] = buffer.getSample (1, i);
        }
    }

    Metrics m;
    double peak = 0.0, sumMid = 0.0, sumSide = 0.0, sumDc = 0.0;
    const auto offSample = static_cast<size_t> (lastOff * sr);
    for (size_t i = 0; i < L.size(); ++i)
    {
        if (! std::isfinite (L[i]) || ! std::isfinite (R[i]))
        {
            m.finite = false;
            return m;
        }
        peak = std::max ({ peak, static_cast<double> (std::abs (L[i])), static_cast<double> (std::abs (R[i])) });
        const double mid = 0.5 * (L[i] + R[i]), side = 0.5 * (L[i] - R[i]);
        if (i < offSample)
        {
            sumMid += mid * mid;
            sumSide += side * side;
            sumDc += mid;
        }
    }
    m.peakDb = 20.0 * std::log10 (peak + 1e-12);
    m.width = std::sqrt (sumSide / std::max (1e-20, sumMid));
    m.dc = sumDc / static_cast<double> (std::max<size_t> (1, offSample));

    // Loudness: the loudest 400 ms (momentary-like), mid + side power.
    const size_t win = static_cast<size_t> (0.4 * sr), hop = win / 4;
    double loudest = 0.0;
    for (size_t s = 0; s + win <= L.size(); s += hop)
    {
        double acc = 0.0;
        for (size_t i = s; i < s + win; ++i)
            acc += 0.5 * (static_cast<double> (L[i]) * L[i] + static_cast<double> (R[i]) * R[i]);
        loudest = std::max (loudest, acc / static_cast<double> (win));
    }
    m.loudDb = 10.0 * std::log10 (loudest + 1e-24);

    // Attack: time until the signal first reaches half its peak.
    for (size_t i = 0; i < L.size(); ++i)
        if (std::abs (L[i]) > 0.5 * peak)
        {
            m.attack = static_cast<double> (i) / sr;
            break;
        }

    // Tail: after the last note-off, time until the level stays below -60 dB re the peak.
    const double floor = peak * 0.001;
    size_t lastLoud = offSample;
    for (size_t i = offSample; i < L.size(); ++i)
        if (std::abs (L[i]) > floor || std::abs (R[i]) > floor)
            lastLoud = i;
    m.tail = static_cast<double> (lastLoud - std::min (lastLoud, offSample)) / sr;

    // Spectrum while the notes sound: averaged Hann-windowed 4096-point frames.
    constexpr size_t N = 4096;
    std::vector<double> power (N / 2, 0.0), window (N);
    for (size_t i = 0; i < N; ++i)
        window[i] = 0.5 - 0.5 * std::cos (2.0 * 3.14159265358979 * static_cast<double> (i) / (N - 1));
    for (size_t s = 0; s + N <= std::min (L.size(), offSample + N); s += N / 2)
    {
        std::vector<std::complex<double>> a (N);
        for (size_t i = 0; i < N; ++i)
            a[i] = 0.5 * (L[s + i] + R[s + i]) * window[i];
        fft (a);
        for (size_t k = 0; k < N / 2; ++k)
            power[k] += std::norm (a[k]);
    }
    double tot = 0.0, cen = 0.0, lo = 0.0, hi = 0.0;
    for (size_t k = 1; k < N / 2; ++k)
    {
        const double f = static_cast<double> (k) * sr / N;
        if (f < 20.0 || f > 20000.0)
            continue;
        tot += power[k];
        cen += power[k] * f;
        if (f >= 30.0 && f <= 120.0)
            lo += power[k];
        if (f >= 6000.0)
            hi += power[k];
    }
    m.centroid = cen / std::max (tot, 1e-30);
    m.low = lo / std::max (tot, 1e-30);
    m.high = hi / std::max (tot, 1e-30);
    m.fingerprint = fingerprint (L, R, offSample, sr, m.width);
    if (wavFile != juce::File())
        writeWav (wavFile, L, R, sr);
    return m;
}

inline std::vector<float> fingerprint (const std::vector<float>& L, const std::vector<float>& R, size_t offSample, double sr, double width)
{
    constexpr size_t N = 2048, hop = 1024;
    constexpr int bands = 32, slices = 16;
    const size_t end = std::min (L.size(), offSample + static_cast<size_t> (0.5 * sr)); // the notes and half a second of tail
    std::vector<double> window (N);
    for (size_t i = 0; i < N; ++i)
        window[i] = 0.5 - 0.5 * std::cos (2.0 * 3.14159265358979 * static_cast<double> (i) / (N - 1));
    std::array<size_t, bands + 1> edge {};
    for (int b = 0; b <= bands; ++b)
        edge[static_cast<size_t> (b)] = std::max<size_t> (1, static_cast<size_t> (60.0 * std::pow (16000.0 / 60.0, b / static_cast<double> (bands)) * N / sr));

    std::vector<std::array<double, bands>> frames;
    std::vector<double> frameRms, frameCen;
    for (size_t s = 0; s + N <= end; s += hop)
    {
        std::vector<std::complex<double>> a (N);
        double rms = 0.0;
        for (size_t i = 0; i < N; ++i)
        {
            const double x = 0.5 * (L[s + i] + R[s + i]);
            rms += x * x;
            a[i] = x * window[i];
        }
        fft (a);
        std::array<double, bands> e {};
        double tot = 0.0, cen = 0.0;
        for (int b = 0; b < bands; ++b)
            for (size_t k = edge[static_cast<size_t> (b)]; k < edge[static_cast<size_t> (b) + 1] && k < N / 2; ++k)
            {
                const double p = std::norm (a[k]);
                e[static_cast<size_t> (b)] += p;
                tot += p;
                cen += p * static_cast<double> (k) * sr / N;
            }
        frames.push_back (e);
        frameRms.push_back (std::sqrt (rms / N));
        frameCen.push_back (tot > 1e-20 ? std::log2 (cen / tot) : 0.0);
    }
    std::vector<float> fp (66, 0.0f);
    if (frames.empty())
        return fp;

    std::array<double, bands> sum {};
    for (const auto& f : frames)
        for (int b = 0; b < bands; ++b)
            sum[static_cast<size_t> (b)] += f[static_cast<size_t> (b)];
    double mean = 0.0;
    std::array<double, bands> db {};
    for (int b = 0; b < bands; ++b)
    {
        db[static_cast<size_t> (b)] = 10.0 * std::log10 (sum[static_cast<size_t> (b)] / static_cast<double> (frames.size()) + 1e-20);
        mean += db[static_cast<size_t> (b)] / bands;
    }
    for (int b = 0; b < bands; ++b)
        fp[static_cast<size_t> (b)] = static_cast<float> (std::max (-60.0, db[static_cast<size_t> (b)] - mean));

    const double maxRms = *std::max_element (frameRms.begin(), frameRms.end()) + 1e-20;
    std::array<double, slices> sRms {}, sCen {}, sW {};
    for (size_t i = 0; i < frames.size(); ++i)
    {
        const auto sl = std::min<size_t> (slices - 1, i * slices / frames.size());
        sRms[sl] += frameRms[i] * frameRms[i];
        sCen[sl] += frameCen[i] * frameRms[i];
        sW[sl] += frameRms[i];
    }
    const double perSlice = std::max (1.0, static_cast<double> (frames.size()) / slices);
    double cenMean = 0.0, wTot = 0.0;
    for (size_t sl = 0; sl < slices; ++sl)
    {
        cenMean += sCen[sl];
        wTot += sW[sl];
    }
    cenMean /= std::max (wTot, 1e-20);
    for (size_t sl = 0; sl < slices; ++sl)
    {
        fp[32 + sl] = static_cast<float> (std::max (-60.0, 20.0 * std::log10 (std::sqrt (sRms[sl] / perSlice) / maxRms + 1e-6)));
        fp[48 + sl] = static_cast<float> (sW[sl] > 1e-9 ? sCen[sl] / sW[sl] - cenMean : 0.0);
    }

    double floorP = 0.0;
    for (const auto& f : frames)
        for (const double e : f)
            floorP = std::max (floorP, e);
    floorP = floorP * 1.0e-6 + 1e-20;
    double flux = 0.0;
    int count = 0;
    for (size_t i = 1; i < frames.size(); ++i)
    {
        if (frameRms[i] < maxRms * 0.05 || frameRms[i - 1] < maxRms * 0.05)
            continue;
        double d = 0.0;
        for (int b = 0; b < bands; ++b)
            d += std::abs (10.0 * std::log10 ((frames[i][static_cast<size_t> (b)] + floorP) / (frames[i - 1][static_cast<size_t> (b)] + floorP)));
        flux += d / bands;
        ++count;
    }
    fp[64] = static_cast<float> (count > 0 ? flux / count : 0.0);
    fp[65] = static_cast<float> (width);
    return fp;
}

} // namespace packaudit
