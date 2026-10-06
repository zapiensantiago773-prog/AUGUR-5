#include "LegacyEffects.h"

#include <cmath>
#include <map>

namespace augur5
{

namespace
{
const char* const legacyIds[] { "chorus_on", "chorus_rate", "chorus_depth", "chorus_mix", "chorus_mode",
                                "phaser_on", "phaser_rate", "phaser_depth", "phaser_fb", "phaser_mix",
                                "echo_on", "echo_mode", "echo_rate", "echo_intensity", "echo_bass", "echo_treble",
                                "echo_wow", "echo_input", "echo_volume", "echo_reverb",
                                "reverb_on", "reverb_size", "reverb_decay", "reverb_mix", "reverb_type" };

// 1.0's effects were sends (dry stays, wet added at `ratio`); the rack crossfades with equal power
// (dry cos, wet sin). The same wet/dry ratio is tan (pi/2 * mix).
float crossfadeFor (float ratio)
{
    return static_cast<float> (2.0 / 3.14159265358979 * std::atan (std::max (0.0f, ratio)));
}
} // namespace

bool isLegacyEffectId (const juce::String& id)
{
    for (auto* l : legacyIds)
        if (id == l)
            return true;
    return false;
}

void convertLegacyEffects (Settings& settings)
{
    std::map<juce::String, float> old;
    Settings kept;
    for (auto& s : settings)
    {
        if (isLegacyEffectId (s.first))
            old[s.first] = s.second;
        else
            kept.push_back (s);
    }
    if (old.empty())
        return;

    const auto has = [&old] (const char* prefix) {
        for (auto& [k, v] : old)
            if (k.startsWith (prefix))
                return true;
        return false;
    };
    const auto get = [&old] (const char* id, float def) {
        const auto it = old.find (id);
        return it != old.end() ? it->second : def;
    };
    const auto set = [&kept] (const char* id, float v) { kept.push_back ({ id, v }); };

    if (has ("chorus_"))
    {
        // 1.0 modes: 0 FREE (RATE / DEPTH knobs), 1 I, 2 II, 3 I+II. The rack's Juno modes are the same circuits;
        // FREE becomes Juno I with its rate and sweep scaled to the old knobs.
        const int mode = juce::roundToInt (get ("chorus_mode", 0.0f));
        set ("fx_chorus_on", get ("chorus_on", 0.0f));
        set ("fx_chorus_mode", static_cast<float> (mode == 0 ? 0 : mode - 1));
        if (mode == 0)
        {
            set ("fx_chorus_rate", juce::jlimit (0.25f, 4.0f, get ("chorus_rate", 0.6f) / 0.513f));
            set ("fx_chorus_depth", juce::jlimit (0.0f, 2.0f, (0.0005f + 0.0045f * get ("chorus_depth", 0.5f)) / 0.001845f));
        }
        const float mix = get ("chorus_mix", 0.5f);
        set ("fx_chorus_mix", crossfadeFor (mix / std::max (0.1f, 1.0f - 0.5f * mix)));
        set ("fx_chorus_hiss", 0.05f);
    }

    if (has ("phaser_"))
    {
        // 1.0: six all-pass stages swept between ~128 Hz and 4 kHz, positive feedback, same dry / shifted mix.
        set ("fx_phaser_on", get ("phaser_on", 0.0f));
        set ("fx_phaser_stages", 1.0f); // 6
        set ("fx_phaser_rate", juce::jlimit (0.02f, 10.0f, get ("phaser_rate", 0.3f)));
        set ("fx_phaser_depth", get ("phaser_depth", 0.7f));
        set ("fx_phaser_center", 700.0f);
        set ("fx_phaser_feedback", 0.95f * get ("phaser_fb", 0.4f));
        set ("fx_phaser_lfo", 1.0f); // triangle
        set ("fx_phaser_mix", get ("phaser_mix", 0.5f));
    }

    if (has ("echo_"))
    {
        // Same 12-position selector. REPEAT RATE 0..1 was 250 .. 55 ms on head 1; BASS / TREBLE +-1 were about +-7 dB.
        set ("fx_echo_on", get ("echo_on", 0.0f));
        set ("fx_echo_mode", get ("echo_mode", 3.0f));
        set ("fx_echo_time", static_cast<float> (250.0 * std::pow (0.055 / 0.250, juce::jlimit (0.0, 1.0, static_cast<double> (get ("echo_rate", 0.5f))))));
        set ("fx_echo_intensity", get ("echo_intensity", 0.45f));
        set ("fx_echo_bass", 7.0f * get ("echo_bass", 0.0f));
        set ("fx_echo_treble", 7.0f * get ("echo_treble", 0.0f));
        set ("fx_echo_wow", get ("echo_wow", 0.4f));
        set ("fx_echo_flutter", 0.8f * get ("echo_wow", 0.4f));
        set ("fx_echo_sat", get ("echo_input", 0.5f));
        set ("fx_echo_age", 0.15f);
        set ("fx_echo_spring", get ("echo_reverb", 0.35f));
        set ("fx_echo_mix", crossfadeFor (get ("echo_volume", 0.5f)));
    }

    if (has ("reverb_"))
    {
        // 1.0 types: 0 HALL, 1 PLATE, 2 SPRING -> rack: 2 HALL, 0 PLATE, 4 SPRING. SIZE 0..1 -> 0.5 .. 1.5.
        static constexpr float typeMap[3] { 2.0f, 0.0f, 4.0f };
        set ("fx_reverb_on", get ("reverb_on", 0.0f));
        set ("fx_reverb_type", typeMap[juce::jlimit (0, 2, juce::roundToInt (get ("reverb_type", 0.0f)))]);
        set ("fx_reverb_size", 0.5f + get ("reverb_size", 0.5f));
        set ("fx_reverb_decay", juce::jlimit (0.2f, 30.0f, get ("reverb_decay", 2.5f)));
        set ("fx_reverb_mix", crossfadeFor (1.2f * get ("reverb_mix", 0.25f)));
    }

    settings = std::move (kept);
}

} // namespace augur5
