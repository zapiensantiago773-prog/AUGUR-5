// augur_render: renders the DSP core to WAV without a DAW.
// Usage: augur_render [--note 69] [--velocity 1.0] [--hold 1.0] [--release 0.5] [--sr 48000] [--out note.wav]

#include "Voice/MonoTestVoice.h"
#include "WavWriter.h"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace
{
struct Options
{
    int note = 69;
    float velocity = 1.0f;
    double holdSeconds = 1.0;
    double releaseSeconds = 0.5;
    std::uint32_t sampleRate = 48000;
    std::string out = "note.wav";
};

bool parse (int argc, char** argv, Options& o)
{
    for (int i = 1; i < argc; ++i)
    {
        const std::string key = argv[i];
        if (i + 1 >= argc)
            return false;
        const char* value = argv[++i];

        if (key == "--note")          o.note = std::atoi (value);
        else if (key == "--velocity") o.velocity = static_cast<float> (std::atof (value));
        else if (key == "--hold")     o.holdSeconds = std::atof (value);
        else if (key == "--release")  o.releaseSeconds = std::atof (value);
        else if (key == "--sr")       o.sampleRate = static_cast<std::uint32_t> (std::atoi (value));
        else if (key == "--out")      o.out = value;
        else                          return false;
    }
    return o.sampleRate > 0;
}
} // namespace

int main (int argc, char** argv)
{
    Options o;
    if (! parse (argc, argv, o))
    {
        std::fprintf (stderr, "usage: augur_render [--note N] [--velocity V] [--hold S] [--release S] [--sr HZ] [--out FILE]\n");
        return 1;
    }

    const auto holdSamples = static_cast<int> (o.holdSeconds * o.sampleRate);
    const auto releaseSamples = static_cast<int> (o.releaseSeconds * o.sampleRate);

    std::vector<float> samples (static_cast<size_t> (holdSamples + releaseSamples));

    augur::MonoTestVoice voice;
    voice.prepare (static_cast<double> (o.sampleRate));
    voice.noteOn (o.note, o.velocity);
    voice.render (samples.data(), holdSamples);
    voice.noteOff (o.note);
    voice.render (samples.data() + holdSamples, releaseSamples);

    if (! augur::tools::writeWavFloatMono (o.out, samples, o.sampleRate))
    {
        std::fprintf (stderr, "could not write %s\n", o.out.c_str());
        return 1;
    }

    std::printf ("wrote %s (%zu samples @ %u Hz)\n", o.out.c_str(), samples.size(), o.sampleRate);
    return 0;
}
