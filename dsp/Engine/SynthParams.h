#pragma once

#include <array>

namespace augur
{

enum class ModSource
{
    FilterEnv,
    AmpEnv,
    Osc2,
    Lfo,
    ModWheel,
    Velocity,
    Aftertouch,
    Noise,
    count
};

enum class ModDest
{
    Osc1Freq,
    Osc2Freq,
    Osc1Pw,
    Osc2Pw,
    FilterCutoff,
    Resonance,
    AmpLevel,
    LfoRate,
    count
};

struct ModSlot
{
    ModSource source = ModSource::FilterEnv;
    ModDest dest = ModDest::Osc1Freq;
    float amount = 0.0f; // -1..1
};

// Plain parameter snapshot in engineering units. Filled by the plugin once per block; the engine
// smooths everything that could zipper. No JUCE here.
struct SynthParams
{
    // Oscillators
    int osc1Semi = 0;
    float osc1Fine = 0.0f;  // cents
    float osc1Pw = 0.5f;    // 0.05..0.95
    bool osc1Saw = true, osc1Pulse = false, osc1Sync = false;

    int osc2Semi = 0;
    float osc2Fine = 0.0f;
    float osc2Pw = 0.5f;
    bool osc2Saw = true, osc2Tri = false, osc2Pulse = false, osc2LoFreq = false, osc2Kbd = true;

    int oscModel = 0; // 0 = CEM3340 (Rev 3), 1 = SSM2030 (Rev 1/2)

    // Mixer
    float mixOsc1 = 0.8f, mixOsc2 = 0.6f, mixNoise = 0.0f, mixDrive = 0.2f;

    // Filter
    float cutoffHz = 2500.0f, resonance = 0.2f, envAmount = 0.3f;
    int filterModel = 0; // 0 = CEM3320 (Rev 3), 1 = SSM2040 (Rev 1/2), 2 = Cascade, 3 = Multimode, 4 = Bite
    int filterSlope = 0; // 0 = 24 dB/oct, 1 = 12 dB/oct
    int filterMode = 0;  // 0 = low-pass, 1 = band-pass, 2 = high-pass
    float hpfHz = 10.0f; // post-filter high-pass; <= 10 Hz = off
    int keytrack = 2;    // 0 off, 1 half, 2 full
    float filterVelocity = 0.0f;

    // Amplifier
    float levelDb = -6.0f; // <= -60 means silence
    float ampVelocity = 0.5f;
    float aftertouchAmount = 0.0f;

    // Envelopes (seconds / 0..1)
    float fenvA = 0.005f, fenvD = 0.6f, fenvS = 0.3f, fenvR = 0.5f;
    float aenvA = 0.003f, aenvD = 0.8f, aenvS = 0.8f, aenvR = 0.4f;

    // LFO
    float lfoRate = 4.0f; // Hz, or 0..1 position when synced
    float lfoAmount = 0.5f, lfoDelay = 0.0f;
    int lfoWave = 0;
    bool lfoSync = false;

    // Poly Mod
    bool pmOn = false;
    float pmFilterEnv = 0.0f, pmOsc2 = 0.0f;
    bool pmFreqA = true, pmPwA = false, pmFilter = false;

    // Mod matrix
    std::array<ModSlot, 4> matrix {};

    // Voices / vintage
    float voiceDetune = 0.3f, voiceSpread = 0.3f, voicePan = 0.0f;
    int voiceCount = 5;
    float analogAge = 0.35f;

    // Effects
    bool chorusOn = false;
    float chorusRate = 0.6f, chorusDepth = 0.5f, chorusMix = 0.5f;
    bool delayOn = false;
    float delayTime = 0.375f, delayFeedback = 0.35f, delayMix = 0.3f;
    bool reverbOn = false;
    float reverbSize = 0.5f, reverbDecay = 2.5f, reverbMix = 0.25f;

    // Global
    float masterTuneCents = 0.0f, glide = 0.0f;
    bool unison = false, legato = false;
    int pitchBendRange = 2;

    // Arpeggiator
    bool arpOn = false;
    int arpMode = 0;   // Up, Down, Up-Down, Random, As played
    int arpOctaves = 1;
    int arpRate = 5;   // index into arpRateBeats (1/16)
    float arpGate = 0.5f, arpSwing = 0.0f;
    bool arpLatch = false;
};

inline constexpr std::array<double, 8> arpRateBeats { 1.0, 0.75, 0.5, 1.0 / 3.0, 0.375, 0.25, 1.0 / 6.0, 0.125 };
inline constexpr std::array<const char*, 8> arpRateNames { "1/4", "1/8D", "1/8", "1/8T", "1/16D", "1/16", "1/16T", "1/32" };

// Tempo-synced LFO divisions, in beats (quarter notes) per cycle, slowest first.
inline constexpr std::array<float, 16> lfoSyncBeats { 32.0f, 16.0f, 8.0f, 6.0f, 4.0f, 3.0f, 2.0f, 1.5f,
                                                      1.0f, 0.75f, 0.5f, 1.0f / 3.0f, 0.25f, 1.0f / 6.0f, 0.125f, 1.0f / 12.0f };
inline constexpr std::array<const char*, 16> lfoSyncNames { "8 bars", "4 bars", "2 bars", "3/2", "1 bar", "3/4", "1/2", "3/8",
                                                            "1/4", "3/16", "1/8", "1/8T", "1/16", "1/16T", "1/32", "1/32T" };

} // namespace augur
