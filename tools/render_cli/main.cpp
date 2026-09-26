// augur_render: renders the AUGUR-5 engine to WAV without a DAW, or benchmarks it.
//
//   augur_render [--notes 60,64,67] [--velocity 0.8] [--hold 1.5] [--release 1.0] [--sr 48000]
//                [--set name=value ...] [--out out.wav]
//   augur_render --bench 10 [--voices 5] [--sr 48000] [--set ...]
//
// --set names: osc1_semi osc1_fine osc1_pw osc1_saw osc1_pulse osc1_sync osc2_semi osc2_fine osc2_pw
//   osc2_saw osc2_tri osc2_pulse osc2_lofreq osc2_kbd osc_model mix_osc1 mix_osc2 mix_noise mix_drive
//   cutoff reso env_amt flt_model keytrack fenv_a fenv_d fenv_s fenv_r aenv_a aenv_d aenv_s aenv_r
//   pm_on pm_fenv pm_osc2 pm_freqa pm_pwa pm_filter age detune spread unison glide level
//   chorus delay reverb

#include "Engine/SynthEngine.h"
#include "Util/Denormals.h"
#include "WavWriter.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace
{
using Setter = std::function<void (augur::SynthParams&, float)>;

const std::map<std::string, Setter>& setters()
{
    using P = augur::SynthParams;
    static const std::map<std::string, Setter> m {
        { "osc1_semi", [] (P& p, float v) { p.osc1Semi = static_cast<int> (v); } },
        { "osc1_fine", [] (P& p, float v) { p.osc1Fine = v; } },
        { "osc1_pw", [] (P& p, float v) { p.osc1Pw = v; } },
        { "osc1_saw", [] (P& p, float v) { p.osc1Saw = v > 0.5f; } },
        { "osc1_pulse", [] (P& p, float v) { p.osc1Pulse = v > 0.5f; } },
        { "osc1_sync", [] (P& p, float v) { p.osc1Sync = v > 0.5f; } },
        { "osc2_semi", [] (P& p, float v) { p.osc2Semi = static_cast<int> (v); } },
        { "osc2_fine", [] (P& p, float v) { p.osc2Fine = v; } },
        { "osc2_pw", [] (P& p, float v) { p.osc2Pw = v; } },
        { "osc2_saw", [] (P& p, float v) { p.osc2Saw = v > 0.5f; } },
        { "osc2_tri", [] (P& p, float v) { p.osc2Tri = v > 0.5f; } },
        { "osc2_pulse", [] (P& p, float v) { p.osc2Pulse = v > 0.5f; } },
        { "osc2_lofreq", [] (P& p, float v) { p.osc2LoFreq = v > 0.5f; } },
        { "osc2_kbd", [] (P& p, float v) { p.osc2Kbd = v > 0.5f; } },
        { "osc_model", [] (P& p, float v) { p.oscModel = static_cast<int> (v); } },
        { "mix_osc1", [] (P& p, float v) { p.mixOsc1 = v; } },
        { "mix_osc2", [] (P& p, float v) { p.mixOsc2 = v; } },
        { "mix_noise", [] (P& p, float v) { p.mixNoise = v; } },
        { "mix_drive", [] (P& p, float v) { p.mixDrive = v; } },
        { "cutoff", [] (P& p, float v) { p.cutoffHz = v; } },
        { "reso", [] (P& p, float v) { p.resonance = v; } },
        { "env_amt", [] (P& p, float v) { p.envAmount = v; } },
        { "flt_model", [] (P& p, float v) { p.filterModel = static_cast<int> (v); } },
        { "fuzz_on", [] (P& p, float v) { p.fuzzOn = v > 0.5f; } },
        { "fuzz_sustain", [] (P& p, float v) { p.fuzzSustain = v; } },
        { "phaser_on", [] (P& p, float v) { p.phaserOn = v > 0.5f; } },
        { "reverb_type", [] (P& p, float v) { p.reverbType = static_cast<int> (v); } },
        { "chorus_mode", [] (P& p, float v) { p.chorusMode = static_cast<int> (v); } },
        { "flt_slope", [] (P& p, float v) { p.filterSlope = static_cast<int> (v); } },
        { "flt_mode", [] (P& p, float v) { p.filterMode = static_cast<int> (v); } },
        { "hpf_cutoff", [] (P& p, float v) { p.hpfHz = v; } },
        { "arp_on", [] (P& p, float v) { p.arpOn = v > 0.5f; } },
        { "arp_mode", [] (P& p, float v) { p.arpMode = static_cast<int> (v); } },
        { "arp_rate", [] (P& p, float v) { p.arpRate = static_cast<int> (v); } },
        { "arp_oct", [] (P& p, float v) { p.arpOctaves = static_cast<int> (v); } },
        { "keytrack", [] (P& p, float v) { p.keytrack = static_cast<int> (v); } },
        { "fenv_a", [] (P& p, float v) { p.fenvA = v; } },
        { "fenv_d", [] (P& p, float v) { p.fenvD = v; } },
        { "fenv_s", [] (P& p, float v) { p.fenvS = v; } },
        { "fenv_r", [] (P& p, float v) { p.fenvR = v; } },
        { "aenv_a", [] (P& p, float v) { p.aenvA = v; } },
        { "aenv_d", [] (P& p, float v) { p.aenvD = v; } },
        { "aenv_s", [] (P& p, float v) { p.aenvS = v; } },
        { "aenv_r", [] (P& p, float v) { p.aenvR = v; } },
        { "pm_on", [] (P& p, float v) { p.pmOn = v > 0.5f; } },
        { "pm_fenv", [] (P& p, float v) { p.pmFilterEnv = v; } },
        { "pm_osc2", [] (P& p, float v) { p.pmOsc2 = v; } },
        { "pm_freqa", [] (P& p, float v) { p.pmFreqA = v > 0.5f; } },
        { "pm_pwa", [] (P& p, float v) { p.pmPwA = v > 0.5f; } },
        { "pm_filter", [] (P& p, float v) { p.pmFilter = v > 0.5f; } },
        { "age", [] (P& p, float v) { p.analogAge = v; } },
        { "detune", [] (P& p, float v) { p.voiceDetune = v; } },
        { "spread", [] (P& p, float v) { p.voiceSpread = v; } },
        { "unison", [] (P& p, float v) { p.unison = v > 0.5f; } },
        { "glide", [] (P& p, float v) { p.glide = v; } },
        { "level", [] (P& p, float v) { p.levelDb = v; } },
        { "chorus", [] (P& p, float v) { p.chorusOn = v > 0.5f; } },
        { "delay", [] (P& p, float v) { p.delayOn = v > 0.5f; } },
        { "reverb", [] (P& p, float v) { p.reverbOn = v > 0.5f; } },
    };
    return m;
}

std::vector<int> parseNotes (const std::string& s)
{
    std::vector<int> notes;
    std::stringstream ss (s);
    std::string item;
    while (std::getline (ss, item, ','))
        notes.push_back (std::atoi (item.c_str()));
    return notes;
}

int usage()
{
    std::fprintf (stderr, "usage: augur_render [--notes 60,64,67] [--velocity V] [--hold S] [--release S] [--sr HZ]\n"
                          "                    [--set name=value]... [--out FILE] | --bench SECONDS [--voices N]\n");
    return 1;
}
} // namespace

int main (int argc, char** argv)
{
    augur::ScopedFlushDenormals ftz;

    std::vector<int> notes { 57 };
    float velocity = 0.8f;
    double hold = 1.5, release = 1.0, benchSeconds = 0.0;
    std::uint32_t sampleRate = 48000;
    int quality = 1; // 0 ECO, 1 GREAT, 2 DIVINE
    int benchVoices = 5;
    std::string out = "render.wav";
    augur::SynthParams params;

    for (int i = 1; i < argc; ++i)
    {
        const std::string key = argv[i];
        if (i + 1 >= argc)
            return usage();
        const std::string value = argv[++i];

        if (key == "--notes")          notes = parseNotes (value);
        else if (key == "--velocity")  velocity = static_cast<float> (std::atof (value.c_str()));
        else if (key == "--hold")      hold = std::atof (value.c_str());
        else if (key == "--release")   release = std::atof (value.c_str());
        else if (key == "--sr")        sampleRate = static_cast<std::uint32_t> (std::atoi (value.c_str()));
        else if (key == "--quality")   quality = std::atoi (value.c_str());
        else if (key == "--out")       out = value;
        else if (key == "--bench")     benchSeconds = std::atof (value.c_str());
        else if (key == "--voices")    benchVoices = std::atoi (value.c_str());
        else if (key == "--set")
        {
            const auto eq = value.find ('=');
            const auto it = eq == std::string::npos ? setters().end() : setters().find (value.substr (0, eq));
            if (it == setters().end())
            {
                std::fprintf (stderr, "unknown --set %s\n", value.c_str());
                return 1;
            }
            it->second (params, static_cast<float> (std::atof (value.substr (eq + 1).c_str())));
        }
        else
            return usage();
    }

    auto engine = std::make_unique<augur::SynthEngine>();
    engine->prepare (sampleRate, augur::SynthEngine::defaultUnitSeed, augur::SynthEngine::oversamplingFor (quality, sampleRate));
    constexpr int block = 256;
    std::vector<float> l (block), r (block);

    if (benchSeconds > 0.0)
    {
        params.voiceCount = std::max (benchVoices, 1);
        engine->setParams (params);
        for (int v = 0; v < benchVoices; ++v)
            engine->noteOn (48 + v * 4, 0.8f);

        const auto total = static_cast<long long> (benchSeconds * sampleRate);
        const auto t0 = std::chrono::steady_clock::now();
        for (long long done = 0; done < total; done += block)
            engine->process (l.data(), r.data(), block);
        const double elapsed = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        std::printf ("%d voices @ %u Hz (oversampling x%d): %.3f s CPU for %.1f s audio = %.2f%% of one core\n",
                     benchVoices, sampleRate, engine->getOversampling(), elapsed, benchSeconds, 100.0 * elapsed / benchSeconds);
        return 0;
    }

    engine->setParams (params);
    const auto holdSamples = static_cast<long long> (hold * sampleRate);
    const auto totalSamples = holdSamples + static_cast<long long> (release * sampleRate);
    std::vector<float> interleaved;
    interleaved.reserve (static_cast<size_t> (totalSamples * 2));

    for (int n : notes)
        engine->noteOn (n, velocity);
    bool released = false;
    for (long long pos = 0; pos < totalSamples;)
    {
        if (! released && pos >= holdSamples)
        {
            for (int n : notes)
                engine->noteOff (n);
            released = true;
        }
        long long len = std::min<long long> (block, totalSamples - pos);
        if (! released)
            len = std::min (len, holdSamples - pos);
        engine->process (l.data(), r.data(), static_cast<int> (len));
        for (long long k = 0; k < len; ++k)
        {
            interleaved.push_back (l[static_cast<size_t> (k)]);
            interleaved.push_back (r[static_cast<size_t> (k)]);
        }
        pos += len;
    }

    if (! augur::tools::writeWavFloat (out, interleaved, 2, sampleRate))
    {
        std::fprintf (stderr, "could not write %s\n", out.c_str());
        return 1;
    }
    std::printf ("wrote %s (%lld frames @ %u Hz)\n", out.c_str(), totalSamples, sampleRate);
    return 0;
}
