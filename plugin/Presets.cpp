#include "Presets.h"
#include "Parameters.h"

#include <algorithm>
#include <string_view>

namespace augur5
{

namespace
{
struct Setting
{
    const char* id;
    float value; // real units (Hz, s, %, st, choice index, 0/1)
};

struct FactoryPreset
{
    const char* name;
    std::vector<Setting> settings;       // applied on top of the defaults (= Init)
    const char* category = "Classic";
};

std::vector<FactoryPreset> buildBank();

const std::vector<FactoryPreset>& factoryBank()
{
    static const std::vector<FactoryPreset> bank = buildBank();
    return bank;
}

std::vector<FactoryPreset> buildBank()
{
    std::vector<FactoryPreset> bank {
        { "Init", {}, "Init" },

        { "Warm Horizon", {
            { "mix_osc1", 0.8f }, { "mix_osc2", 0.72f }, { "mix_noise", 0.05f }, { "mix_drive", 0.3f },
            { "osc2_fine", 7.0f }, { "flt_cutoff", 1200.0f }, { "flt_reso", 0.3f }, { "flt_env_amt", 0.3f },
            { "fenv_a", 0.01f }, { "fenv_d", 1.2f }, { "fenv_s", 0.5f }, { "fenv_r", 1.5f },
            { "aenv_a", 0.02f }, { "aenv_d", 1.0f }, { "aenv_s", 0.75f }, { "aenv_r", 1.2f },
            { "amp_velocity", 0.4f }, { "flt_velocity", 0.25f },
            { "mm3_amt", 0.08f }, { "lfo_rate", 0.4f }, { "lfo_amount", 0.6f },
            { "voice_detune", 0.3f }, { "voice_spread", 0.4f }, { "analog_age", 0.4f },
            { "chorus_on", 1.0f }, { "chorus_mix", 0.3f }, { "reverb_on", 1.0f }, { "reverb_mix", 0.25f }, { "amp_level", -5.1f } }, "Pad" },

        { "Brass Section", {
            { "mix_osc1", 0.9f }, { "mix_osc2", 0.8f }, { "osc2_fine", -6.0f }, { "flt_cutoff", 500.0f },
            { "flt_reso", 0.1f }, { "flt_env_amt", 0.55f }, { "fenv_a", 0.06f }, { "fenv_d", 0.5f }, { "fenv_s", 0.45f },
            { "fenv_r", 0.3f }, { "aenv_a", 0.03f }, { "aenv_d", 0.4f }, { "aenv_s", 0.9f }, { "aenv_r", 0.3f },
            { "flt_velocity", 0.3f }, { "analog_age", 0.45f }, { "amp_level", -10.8f } }, "Stab" },

        { "Sync Lead", {
            { "osc1_sync", 1.0f }, { "osc1_freq", 12.0f }, { "mix_osc1", 1.0f }, { "mix_osc2", 0.0f },
            { "pm_on", 1.0f }, { "pm_fenv_amt", 0.35f }, { "pm_dst_freqa", 1.0f },
            { "flt_cutoff", 3500.0f }, { "flt_reso", 0.2f }, { "flt_env_amt", 0.2f },
            { "fenv_a", 0.001f }, { "fenv_d", 0.9f }, { "fenv_s", 0.2f }, { "voice_count", 1.0f }, { "glide", 0.06f },
            { "legato", 1.0f }, { "mm4_src", 3.0f }, { "mm4_dst", 0.0f }, { "mm4_amt", 0.1f }, { "lfo_rate", 5.5f },
            { "delay_on", 1.0f }, { "delay_mix", 0.2f }, { "amp_level", 0.6f } }, "Lead" },

        { "Poly Strings", {
            { "osc1_pulse", 1.0f }, { "osc1_saw", 0.0f }, { "osc1_pw", 30.0f }, { "osc2_fine", 9.0f },
            { "mix_osc1", 0.8f }, { "mix_osc2", 0.8f }, { "flt_cutoff", 2200.0f }, { "flt_env_amt", 0.1f },
            { "aenv_a", 0.45f }, { "aenv_s", 0.85f }, { "aenv_r", 1.4f }, { "fenv_a", 0.5f },
            { "mm1_src", 3.0f }, { "mm1_dst", 2.0f }, { "mm1_amt", 0.35f }, { "lfo_rate", 0.7f },
            { "voice_spread", 0.7f }, { "chorus_on", 1.0f }, { "chorus_mix", 0.5f }, { "reverb_on", 1.0f }, { "reverb_mix", 0.3f }, { "amp_level", -9.2f } }, "Pad" },

        { "Pluck Keys", {
            { "mix_osc2", 0.5f }, { "osc2_freq", 12.0f }, { "flt_cutoff", 300.0f }, { "flt_reso", 0.25f },
            { "flt_env_amt", 0.6f }, { "fenv_a", 0.001f }, { "fenv_d", 0.35f }, { "fenv_s", 0.0f }, { "fenv_r", 0.3f },
            { "aenv_a", 0.001f }, { "aenv_d", 1.1f }, { "aenv_s", 0.0f }, { "aenv_r", 0.4f }, { "flt_velocity", 0.5f },
            { "delay_on", 1.0f }, { "delay_time", 0.33f }, { "delay_mix", 0.18f }, { "amp_level", 0.3f } }, "Pluck" },

        { "Fat Bass", {
            { "osc2_freq", -12.0f }, { "osc2_pulse", 1.0f }, { "osc2_saw", 0.0f }, { "mix_osc2", 0.9f }, { "mix_drive", 0.55f },
            { "flt_cutoff", 180.0f }, { "flt_reso", 0.3f }, { "flt_env_amt", 0.45f }, { "fenv_a", 0.001f },
            { "fenv_d", 0.25f }, { "fenv_s", 0.1f }, { "aenv_a", 0.001f }, { "aenv_d", 0.5f }, { "aenv_s", 0.7f },
            { "aenv_r", 0.08f }, { "voice_count", 1.0f }, { "flt_model", 1.0f }, { "flt_keytrack", 1.0f }, { "amp_level", -7.3f } }, "Bass" },

        { "Poly-Mod Bell", {
            { "osc2_freq", 19.0f }, { "osc1_saw", 0.0f }, { "osc1_pulse", 1.0f }, { "osc1_pw", 50.0f },
            { "mix_osc2", 0.0f }, { "pm_on", 1.0f }, { "pm_osc2_amt", 0.35f }, { "pm_fenv_amt", 0.15f },
            { "pm_dst_freqa", 1.0f }, { "flt_cutoff", 5000.0f }, { "flt_env_amt", 0.2f },
            { "aenv_a", 0.001f }, { "aenv_d", 2.5f }, { "aenv_s", 0.0f }, { "aenv_r", 2.0f },
            { "fenv_d", 1.5f }, { "fenv_s", 0.0f }, { "reverb_on", 1.0f }, { "reverb_mix", 0.35f }, { "reverb_decay", 4.0f }, { "amp_level", -11.0f } }, "Keys" },

        { "Unison Monster", {
            { "unison", 1.0f }, { "voice_count", 6.0f }, { "voice_detune", 0.35f }, { "voice_spread", 0.9f },
            { "mix_osc2", 0.9f }, { "osc2_freq", -12.0f }, { "mix_drive", 0.5f }, { "flt_cutoff", 1400.0f },
            { "flt_reso", 0.35f }, { "flt_env_amt", 0.4f }, { "fenv_d", 0.8f }, { "fenv_s", 0.3f }, { "glide", 0.05f }, { "amp_level", -10.2f } }, "Lead" },

        { "Dream Pad", {
            { "osc1_pulse", 1.0f }, { "osc1_pw", 40.0f }, { "osc2_tri", 1.0f }, { "osc2_saw", 0.0f }, { "osc2_fine", 12.0f },
            { "mix_osc2", 0.7f }, { "mix_noise", 0.08f }, { "flt_cutoff", 900.0f }, { "flt_reso", 0.35f },
            { "flt_env_amt", 0.25f }, { "fenv_a", 2.5f }, { "fenv_d", 3.0f }, { "fenv_s", 0.4f }, { "fenv_r", 3.0f },
            { "aenv_a", 1.8f }, { "aenv_d", 2.0f }, { "aenv_s", 0.8f }, { "aenv_r", 3.5f },
            { "mm1_src", 3.0f }, { "mm1_dst", 4.0f }, { "mm1_amt", 0.25f }, { "lfo_rate", 0.15f }, { "lfo_amount", 0.8f },
            { "mm2_src", 3.0f }, { "mm2_dst", 2.0f }, { "mm2_amt", 0.3f },
            { "voice_spread", 0.8f }, { "chorus_on", 1.0f }, { "chorus_mix", 0.45f },
            { "reverb_on", 1.0f }, { "reverb_mix", 0.45f }, { "reverb_size", 0.85f }, { "reverb_decay", 7.0f }, { "amp_level", -8.9f } }, "Pad" },

        { "Vintage Keys", {
            { "osc1_saw", 0.0f }, { "osc1_pulse", 1.0f }, { "osc1_pw", 22.0f }, { "osc2_pulse", 1.0f }, { "osc2_saw", 0.0f },
            { "osc2_pw", 45.0f }, { "osc2_freq", 12.0f }, { "mix_osc2", 0.4f }, { "flt_cutoff", 1600.0f },
            { "flt_env_amt", 0.3f }, { "fenv_d", 0.7f }, { "fenv_s", 0.15f }, { "aenv_a", 0.002f }, { "aenv_d", 1.6f },
            { "aenv_s", 0.35f }, { "aenv_r", 0.35f }, { "analog_age", 0.85f }, { "flt_model", 1.0f }, { "osc_model", 1.0f },
            { "chorus_on", 1.0f }, { "chorus_mix", 0.35f }, { "amp_level", -13.4f } }, "Keys" },

        { "Resonant Sweep", {
            { "mix_osc2", 0.7f }, { "osc2_fine", 11.0f }, { "flt_cutoff", 150.0f }, { "flt_reso", 0.72f },
            { "flt_env_amt", 0.7f }, { "fenv_a", 1.8f }, { "fenv_d", 3.5f }, { "fenv_s", 0.2f }, { "fenv_r", 2.5f },
            { "aenv_a", 0.4f }, { "aenv_s", 0.9f }, { "aenv_r", 2.5f }, { "delay_on", 1.0f }, { "delay_mix", 0.25f },
            { "delay_fb", 0.5f }, { "reverb_on", 1.0f }, { "reverb_mix", 0.3f }, { "amp_level", -3.0f } }, "Atmos & FX" },

        // ------------------------------------------------------------------------------------------------
        // Melodic / progressive library. Delays are set for ~123 BPM (3/16 = 0.366 s, 1/4 = 0.488 s,
        // 1/8 = 0.244 s). Mod wheel opens the filter on most sounds (matrix slot 4), for live expression.
        // ------------------------------------------------------------------------------------------------

        // BASS
        { "Nocturne Drive Bass", {
            { "osc2_freq", -12.0f }, { "osc2_pulse", 1.0f }, { "osc2_saw", 0.0f }, { "osc2_pw", 42.0f },
            { "mix_osc1", 0.85f }, { "mix_osc2", 0.8f }, { "mix_drive", 0.45f },
            { "flt_cutoff", 220.0f }, { "flt_reso", 0.35f }, { "flt_env_amt", 0.45f }, { "flt_model", 1.0f },
            { "flt_keytrack", 1.0f }, { "flt_velocity", 0.3f },
            { "fenv_a", 0.001f }, { "fenv_d", 0.28f }, { "fenv_s", 0.1f }, { "fenv_r", 0.15f },
            { "aenv_a", 0.001f }, { "aenv_d", 0.4f }, { "aenv_s", 0.75f }, { "aenv_r", 0.12f },
            { "voice_count", 1.0f }, { "legato", 1.0f }, { "glide", 0.03f }, { "amp_level", -5.3f },
            { "mm4_dst", 4.0f }, { "mm4_amt", 0.35f } }, "Bass" },

        { "Rolling Sub Pulse", {
            { "osc1_saw", 0.0f }, { "osc1_pulse", 1.0f }, { "osc1_pw", 40.0f }, { "osc2_freq", -12.0f },
            { "osc2_saw", 0.0f }, { "osc2_tri", 1.0f }, { "mix_osc2", 0.8f }, { "mix_drive", 0.3f },
            { "flt_cutoff", 160.0f }, { "flt_reso", 0.2f }, { "flt_env_amt", 0.35f }, { "flt_keytrack", 1.0f },
            { "fenv_a", 0.001f }, { "fenv_d", 0.18f }, { "fenv_s", 0.0f }, { "fenv_r", 0.1f },
            { "aenv_a", 0.001f }, { "aenv_d", 0.3f }, { "aenv_s", 0.6f }, { "aenv_r", 0.08f },
            { "voice_count", 1.0f }, { "amp_level", -2.4f }, { "mm4_dst", 4.0f }, { "mm4_amt", 0.3f } }, "Bass" },

        { "Hypnotic Bassline", {
            { "osc2_freq", -12.0f }, { "osc2_pulse", 1.0f }, { "osc2_saw", 0.0f }, { "osc2_pw", 30.0f },
            { "mix_osc2", 0.7f }, { "mix_drive", 0.35f }, { "flt_cutoff", 280.0f }, { "flt_reso", 0.45f },
            { "flt_env_amt", 0.5f }, { "flt_velocity", 0.35f }, { "flt_keytrack", 1.0f },
            { "fenv_a", 0.001f }, { "fenv_d", 0.2f }, { "fenv_s", 0.1f }, { "fenv_r", 0.12f },
            { "aenv_a", 0.001f }, { "aenv_d", 0.5f }, { "aenv_s", 0.8f }, { "aenv_r", 0.1f },
            { "voice_count", 1.0f }, { "amp_level", 0.0f }, { "mm4_dst", 4.0f }, { "mm4_amt", 0.35f } }, "Bass" },

        { "Acid Horizon", {
            { "mix_osc2", 0.0f }, { "mix_drive", 0.5f }, { "flt_cutoff", 300.0f }, { "flt_reso", 0.72f },
            { "flt_env_amt", 0.6f }, { "flt_velocity", 0.45f }, { "flt_keytrack", 1.0f },
            { "fenv_a", 0.001f }, { "fenv_d", 0.22f }, { "fenv_s", 0.05f }, { "fenv_r", 0.15f },
            { "aenv_a", 0.001f }, { "aenv_d", 0.4f }, { "aenv_s", 0.7f }, { "aenv_r", 0.1f },
            { "voice_count", 1.0f }, { "legato", 1.0f }, { "glide", 0.06f },
            { "delay_on", 1.0f }, { "delay_time", 0.366f }, { "delay_fb", 0.3f }, { "delay_mix", 0.15f },
            { "mm4_dst", 4.0f }, { "mm4_amt", 0.4f }, { "amp_level", 2.8f } }, "Bass" },

        // LEAD
        { "Orbit Sync Lead", {
            { "osc1_sync", 1.0f }, { "osc1_freq", 7.0f }, { "mix_osc1", 1.0f }, { "mix_osc2", 0.25f },
            { "pm_on", 1.0f }, { "pm_fenv_amt", 0.25f }, { "pm_dst_freqa", 1.0f },
            { "flt_cutoff", 2800.0f }, { "flt_reso", 0.25f }, { "flt_env_amt", 0.25f },
            { "fenv_a", 0.002f }, { "fenv_d", 1.1f }, { "fenv_s", 0.3f }, { "fenv_r", 0.6f },
            { "aenv_a", 0.004f }, { "aenv_s", 0.9f }, { "aenv_r", 0.5f },
            { "voice_count", 1.0f }, { "legato", 1.0f }, { "glide", 0.08f },
            { "mm1_src", 3.0f }, { "mm1_dst", 0.0f }, { "mm1_amt", 0.1f }, { "lfo_rate", 5.2f }, { "lfo_delay", 0.45f },
            { "delay_on", 1.0f }, { "delay_time", 0.366f }, { "delay_fb", 0.45f }, { "delay_mix", 0.25f },
            { "reverb_on", 1.0f }, { "reverb_decay", 4.0f }, { "reverb_mix", 0.3f }, { "amp_level", -0.5f } }, "Lead" },

        { "Glass Mono Lead", {
            { "osc1_saw", 0.0f }, { "osc1_pulse", 1.0f }, { "osc1_pw", 30.0f }, { "osc2_freq", 12.0f }, { "osc2_fine", 6.0f },
            { "mix_osc2", 0.5f }, { "flt_cutoff", 3200.0f }, { "flt_reso", 0.2f }, { "flt_env_amt", 0.15f },
            { "fenv_d", 0.8f }, { "fenv_s", 0.5f }, { "aenv_a", 0.01f }, { "aenv_s", 0.9f }, { "aenv_r", 0.7f },
            { "voice_count", 1.0f }, { "glide", 0.12f }, { "legato", 1.0f },
            { "mm1_src", 3.0f }, { "mm1_dst", 0.0f }, { "mm1_amt", 0.1f }, { "lfo_rate", 5.5f }, { "lfo_delay", 0.4f },
            { "chorus_on", 1.0f }, { "chorus_mix", 0.3f },
            { "delay_on", 1.0f }, { "delay_time", 0.488f }, { "delay_fb", 0.4f }, { "delay_mix", 0.22f },
            { "reverb_on", 1.0f }, { "reverb_decay", 5.0f }, { "reverb_mix", 0.3f }, { "amp_level", -2.8f } }, "Lead" },

        { "Silk Portamento", {
            { "unison", 1.0f }, { "voice_count", 4.0f }, { "voice_detune", 0.2f }, { "voice_spread", 0.7f },
            { "mix_osc2", 0.7f }, { "osc2_fine", 5.0f }, { "flt_cutoff", 1800.0f }, { "flt_reso", 0.2f },
            { "flt_env_amt", 0.2f }, { "fenv_d", 1.5f }, { "fenv_s", 0.5f },
            { "aenv_a", 0.02f }, { "aenv_s", 0.9f }, { "aenv_r", 0.8f }, { "glide", 0.15f }, { "legato", 1.0f },
            { "chorus_on", 1.0f }, { "chorus_mix", 0.35f },
            { "delay_on", 1.0f }, { "delay_time", 0.366f }, { "delay_fb", 0.35f }, { "delay_mix", 0.2f },
            { "reverb_on", 1.0f }, { "reverb_decay", 5.0f }, { "reverb_mix", 0.3f },
            { "mm4_dst", 4.0f }, { "mm4_amt", 0.3f }, { "amp_level", -8.7f } }, "Lead" },

        // PAD
        { "Afterglow Pad", {
            { "osc2_fine", 9.0f }, { "mix_osc2", 0.8f }, { "flt_cutoff", 900.0f }, { "flt_reso", 0.25f },
            { "flt_env_amt", 0.35f }, { "fenv_a", 2.5f }, { "fenv_d", 3.0f }, { "fenv_s", 0.45f }, { "fenv_r", 3.0f },
            { "aenv_a", 1.5f }, { "aenv_d", 2.0f }, { "aenv_s", 0.85f }, { "aenv_r", 4.0f },
            { "mm3_amt", 0.2f }, { "lfo_rate", 0.12f }, { "lfo_amount", 0.8f },
            { "voice_spread", 0.8f }, { "voice_detune", 0.4f }, { "analog_age", 0.5f },
            { "chorus_on", 1.0f }, { "chorus_mix", 0.45f },
            { "reverb_on", 1.0f }, { "reverb_size", 0.85f }, { "reverb_decay", 8.0f }, { "reverb_mix", 0.45f },
            { "mm4_dst", 4.0f }, { "mm4_amt", 0.3f }, { "amp_level", -6.4f } }, "Pad" },

        { "Nightfall Strings", {
            { "osc1_saw", 0.0f }, { "osc1_pulse", 1.0f }, { "osc1_pw", 35.0f }, { "osc2_freq", 12.0f }, { "osc2_fine", 4.0f },
            { "mix_osc2", 0.55f }, { "flt_cutoff", 2200.0f }, { "flt_reso", 0.1f }, { "flt_env_amt", 0.1f },
            { "aenv_a", 0.8f }, { "aenv_s", 0.9f }, { "aenv_r", 2.5f }, { "fenv_a", 0.9f },
            { "mm1_src", 3.0f }, { "mm1_dst", 2.0f }, { "mm1_amt", 0.3f }, { "lfo_rate", 0.8f },
            { "voice_spread", 0.8f }, { "chorus_on", 1.0f }, { "chorus_mix", 0.5f },
            { "reverb_on", 1.0f }, { "reverb_size", 0.7f }, { "reverb_decay", 5.0f }, { "reverb_mix", 0.35f }, { "amp_level", -10.0f } }, "Pad" },

        { "Horizon Swell", {
            { "mix_osc2", 0.6f }, { "osc2_fine", -7.0f }, { "mix_noise", 0.1f }, { "flt_cutoff", 400.0f },
            { "flt_reso", 0.3f }, { "flt_env_amt", 0.6f }, { "fenv_a", 4.0f }, { "fenv_d", 4.0f }, { "fenv_s", 0.6f },
            { "fenv_r", 4.0f }, { "aenv_a", 3.0f }, { "aenv_s", 1.0f }, { "aenv_r", 5.0f },
            { "voice_spread", 0.9f }, { "analog_age", 0.6f }, { "chorus_on", 1.0f }, { "chorus_mix", 0.4f },
            { "reverb_on", 1.0f }, { "reverb_size", 0.9f }, { "reverb_decay", 10.0f }, { "reverb_mix", 0.5f }, { "amp_level", -1.5f } }, "Pad" },

        { "Distant Choir", {
            { "osc1_saw", 0.0f }, { "osc1_pulse", 1.0f }, { "osc1_pw", 50.0f }, { "osc2_saw", 0.0f }, { "osc2_tri", 1.0f },
            { "osc2_freq", 12.0f }, { "mix_osc2", 0.6f }, { "flt_cutoff", 1500.0f }, { "flt_reso", 0.15f },
            { "aenv_a", 1.2f }, { "aenv_s", 0.9f }, { "aenv_r", 3.5f },
            { "mm1_src", 3.0f }, { "mm1_dst", 2.0f }, { "mm1_amt", 0.35f }, { "lfo_rate", 0.3f },
            { "voice_spread", 0.7f }, { "chorus_on", 1.0f }, { "chorus_mix", 0.6f },
            { "reverb_on", 1.0f }, { "reverb_size", 0.8f }, { "reverb_decay", 7.0f }, { "reverb_mix", 0.5f }, { "amp_level", -9.1f } }, "Pad" },

        // PLUCK
        { "Afterlight Pluck", {
            { "osc1_pulse", 1.0f }, { "osc1_pw", 45.0f }, { "mix_osc2", 0.5f }, { "osc2_fine", 7.0f },
            { "flt_cutoff", 350.0f }, { "flt_reso", 0.35f }, { "flt_env_amt", 0.65f }, { "flt_velocity", 0.4f },
            { "fenv_a", 0.001f }, { "fenv_d", 0.32f }, { "fenv_s", 0.0f }, { "fenv_r", 0.3f },
            { "aenv_a", 0.001f }, { "aenv_d", 0.9f }, { "aenv_s", 0.0f }, { "aenv_r", 0.5f },
            { "delay_on", 1.0f }, { "delay_time", 0.366f }, { "delay_fb", 0.4f }, { "delay_mix", 0.28f },
            { "reverb_on", 1.0f }, { "reverb_decay", 3.5f }, { "reverb_mix", 0.3f },
            { "mm4_dst", 4.0f }, { "mm4_amt", 0.35f }, { "amp_level", -6.5f } }, "Pluck" },

        { "Crystal Keys", {
            { "osc2_freq", 19.0f }, { "mix_osc2", 0.25f }, { "pm_on", 1.0f }, { "pm_osc2_amt", 0.15f },
            { "pm_dst_freqa", 1.0f }, { "flt_cutoff", 2500.0f }, { "flt_env_amt", 0.3f },
            { "fenv_a", 0.001f }, { "fenv_d", 0.4f }, { "fenv_s", 0.1f },
            { "aenv_a", 0.001f }, { "aenv_d", 1.2f }, { "aenv_s", 0.0f }, { "aenv_r", 0.8f },
            { "delay_on", 1.0f }, { "delay_time", 0.244f }, { "delay_fb", 0.35f }, { "delay_mix", 0.22f },
            { "reverb_on", 1.0f }, { "reverb_decay", 4.0f }, { "reverb_mix", 0.35f }, { "amp_level", -4.9f } }, "Keys" },

        { "Arp Mirage", {
            { "osc1_saw", 0.0f }, { "osc1_pulse", 1.0f }, { "osc1_pw", 25.0f }, { "mix_osc2", 0.4f }, { "osc2_freq", 12.0f },
            { "flt_cutoff", 600.0f }, { "flt_reso", 0.45f }, { "flt_env_amt", 0.5f }, { "flt_velocity", 0.3f },
            { "fenv_a", 0.001f }, { "fenv_d", 0.18f }, { "fenv_s", 0.05f }, { "fenv_r", 0.2f },
            { "aenv_a", 0.001f }, { "aenv_d", 0.35f }, { "aenv_s", 0.1f }, { "aenv_r", 0.25f },
            { "delay_on", 1.0f }, { "delay_time", 0.366f }, { "delay_fb", 0.5f }, { "delay_mix", 0.3f },
            { "reverb_on", 1.0f }, { "reverb_decay", 3.0f }, { "reverb_mix", 0.25f },
            { "mm4_dst", 4.0f }, { "mm4_amt", 0.35f }, { "amp_level", 2.3f } }, "Pluck" },

        { "Pulse Echo", {
            { "osc2_freq", 12.0f }, { "mix_osc2", 0.6f }, { "osc1_sync", 1.0f }, { "osc1_freq", 5.0f },
            { "flt_cutoff", 800.0f }, { "flt_reso", 0.3f }, { "flt_env_amt", 0.55f },
            { "fenv_a", 0.001f }, { "fenv_d", 0.25f }, { "fenv_s", 0.0f }, { "aenv_a", 0.001f }, { "aenv_d", 0.5f },
            { "aenv_s", 0.0f }, { "aenv_r", 0.3f },
            { "delay_on", 1.0f }, { "delay_time", 0.366f }, { "delay_fb", 0.55f }, { "delay_mix", 0.3f },
            { "reverb_on", 1.0f }, { "reverb_decay", 4.0f }, { "reverb_mix", 0.3f }, { "amp_level", 0.1f } }, "Pluck" },

        // STAB
        { "Midnight Stab", {
            { "osc2_freq", 7.0f }, { "osc2_fine", 4.0f }, { "mix_osc2", 0.7f },
            { "flt_cutoff", 700.0f }, { "flt_reso", 0.4f }, { "flt_env_amt", 0.55f }, { "flt_velocity", 0.3f },
            { "fenv_a", 0.001f }, { "fenv_d", 0.35f }, { "fenv_s", 0.1f }, { "fenv_r", 0.4f },
            { "aenv_a", 0.001f }, { "aenv_d", 0.6f }, { "aenv_s", 0.2f }, { "aenv_r", 0.4f },
            { "delay_on", 1.0f }, { "delay_time", 0.366f }, { "delay_fb", 0.35f }, { "delay_mix", 0.2f },
            { "reverb_on", 1.0f }, { "reverb_decay", 4.5f }, { "reverb_mix", 0.35f }, { "amp_level", -0.3f } }, "Stab" },

        { "Warehouse Chord", {
            { "osc1_pulse", 1.0f }, { "osc1_pw", 45.0f }, { "osc2_freq", 12.0f }, { "mix_osc2", 0.6f }, { "mix_drive", 0.4f },
            { "flt_cutoff", 1100.0f }, { "flt_reso", 0.3f }, { "flt_env_amt", 0.45f },
            { "fenv_a", 0.001f }, { "fenv_d", 0.5f }, { "fenv_s", 0.2f }, { "fenv_r", 0.5f },
            { "aenv_a", 0.001f }, { "aenv_d", 0.8f }, { "aenv_s", 0.3f }, { "aenv_r", 0.6f },
            { "chorus_on", 1.0f }, { "chorus_mix", 0.3f }, { "reverb_on", 1.0f }, { "reverb_mix", 0.3f }, { "amp_level", -14.6f } }, "Stab" },

        // ARP
        { "Sequence Rain", {
            { "osc1_saw", 0.0f }, { "osc1_pulse", 1.0f }, { "osc1_pw", 35.0f }, { "mix_osc2", 0.0f },
            { "flt_cutoff", 900.0f }, { "flt_reso", 0.4f }, { "flt_env_amt", 0.4f },
            { "fenv_a", 0.001f }, { "fenv_d", 0.15f }, { "fenv_s", 0.0f },
            { "aenv_a", 0.001f }, { "aenv_d", 0.25f }, { "aenv_s", 0.0f }, { "aenv_r", 0.2f },
            { "mm3_amt", 0.15f }, { "lfo_wave", 3.0f }, { "lfo_rate", 4.0f }, { "lfo_amount", 0.7f },
            { "voice_spread", 0.6f },
            { "delay_on", 1.0f }, { "delay_time", 0.366f }, { "delay_fb", 0.55f }, { "delay_mix", 0.3f },
            { "reverb_on", 1.0f }, { "reverb_decay", 5.0f }, { "reverb_mix", 0.35f }, { "amp_level", 2.3f } }, "Arp" },

        { "Resonant Cascade", {
            { "mix_osc2", 0.4f }, { "osc2_fine", 8.0f }, { "flt_cutoff", 250.0f }, { "flt_reso", 0.78f },
            { "flt_env_amt", 0.7f }, { "flt_velocity", 0.5f },
            { "fenv_a", 0.001f }, { "fenv_d", 0.45f }, { "fenv_s", 0.0f }, { "fenv_r", 0.3f },
            { "aenv_a", 0.001f }, { "aenv_d", 0.6f }, { "aenv_s", 0.0f }, { "aenv_r", 0.35f },
            { "delay_on", 1.0f }, { "delay_time", 0.244f }, { "delay_fb", 0.45f }, { "delay_mix", 0.25f },
            { "reverb_on", 1.0f }, { "reverb_decay", 3.5f }, { "reverb_mix", 0.3f },
            { "mm4_dst", 4.0f }, { "mm4_amt", 0.35f }, { "amp_level", 5.4f } }, "Arp" },

        // ATMOS
        { "Deep Space Drone", {
            { "osc2_freq", -12.0f }, { "osc2_fine", 5.0f }, { "mix_osc2", 0.8f }, { "flt_cutoff", 300.0f },
            { "flt_reso", 0.6f }, { "flt_env_amt", 0.0f }, { "mm3_amt", 0.35f }, { "lfo_wave", 3.0f },
            { "lfo_rate", 0.2f }, { "lfo_amount", 0.9f },
            { "aenv_a", 4.0f }, { "aenv_s", 1.0f }, { "aenv_r", 8.0f }, { "analog_age", 0.7f }, { "voice_spread", 0.9f },
            { "chorus_on", 1.0f }, { "chorus_mix", 0.35f },
            { "reverb_on", 1.0f }, { "reverb_size", 1.0f }, { "reverb_decay", 14.0f }, { "reverb_mix", 0.6f }, { "amp_level", 6.0f } }, "Atmos & FX" },

        { "Riser Tide", {
            { "mix_noise", 0.6f }, { "mix_osc2", 0.3f }, { "flt_cutoff", 200.0f }, { "flt_reso", 0.5f },
            { "flt_env_amt", 0.9f }, { "fenv_a", 8.0f }, { "fenv_s", 1.0f }, { "fenv_r", 3.0f },
            { "aenv_a", 6.0f }, { "aenv_s", 1.0f }, { "aenv_r", 3.0f },
            { "mm3_amt", 0.1f }, { "lfo_rate", 6.0f },
            { "reverb_on", 1.0f }, { "reverb_size", 0.9f }, { "reverb_decay", 9.0f }, { "reverb_mix", 0.5f }, { "amp_level", 6.0f } }, "Atmos & FX" },

        { "Poly-Mod Ghost", {
            { "osc2_freq", 5.0f }, { "mix_osc2", 0.2f }, { "pm_on", 1.0f }, { "pm_osc2_amt", 0.3f },
            { "pm_dst_freqa", 1.0f }, { "pm_dst_filter", 1.0f }, { "flt_cutoff", 1400.0f }, { "flt_reso", 0.3f },
            { "flt_env_amt", 0.3f }, { "fenv_a", 1.5f }, { "fenv_d", 2.5f }, { "fenv_s", 0.3f },
            { "aenv_a", 1.0f }, { "aenv_s", 0.8f }, { "aenv_r", 3.0f },
            { "reverb_on", 1.0f }, { "reverb_size", 0.85f }, { "reverb_decay", 7.0f }, { "reverb_mix", 0.45f }, { "amp_level", 0.2f } }, "Atmos & FX" },

        // ---- more BASS ----
        { "Offbeat Pluck Bass", {
            { "osc2_freq", -12.0f }, { "mix_osc2", 0.6f }, { "flt_cutoff", 240.0f }, { "flt_reso", 0.3f },
            { "flt_env_amt", 0.55f }, { "flt_velocity", 0.3f }, { "flt_keytrack", 1.0f },
            { "fenv_a", 0.001f }, { "fenv_d", 0.14f }, { "fenv_s", 0.0f }, { "fenv_r", 0.1f },
            { "aenv_a", 0.001f }, { "aenv_d", 0.25f }, { "aenv_s", 0.0f }, { "aenv_r", 0.08f },
            { "voice_count", 1.0f }, { "amp_level", 6.0f }, { "mm4_dst", 4.0f }, { "mm4_amt", 0.35f }, { "mix_osc1", 1.0f }, { "mix_drive", 0.3f } }, "Bass" },

        { "Deep Reese Tide", {
            { "unison", 1.0f }, { "voice_count", 3.0f }, { "voice_detune", 0.3f }, { "voice_spread", 0.4f },
            { "osc2_fine", 12.0f }, { "mix_osc2", 0.9f }, { "mix_drive", 0.4f },
            { "flt_cutoff", 380.0f }, { "flt_reso", 0.2f }, { "flt_env_amt", 0.2f }, { "flt_keytrack", 1.0f },
            { "fenv_d", 0.8f }, { "fenv_s", 0.4f }, { "aenv_a", 0.005f }, { "aenv_s", 0.9f }, { "aenv_r", 0.2f },
            { "mm3_amt", 0.08f }, { "lfo_rate", 0.25f }, { "amp_level", -9.3f },
            { "mm4_dst", 4.0f }, { "mm4_amt", 0.35f } }, "Bass" },

        { "Sync Growl Bass", {
            { "osc1_sync", 1.0f }, { "osc1_freq", 9.0f }, { "osc2_freq", -12.0f }, { "mix_osc2", 0.5f }, { "mix_drive", 0.55f },
            { "pm_on", 1.0f }, { "pm_fenv_amt", 0.2f }, { "pm_dst_freqa", 1.0f },
            { "flt_cutoff", 600.0f }, { "flt_reso", 0.3f }, { "flt_env_amt", 0.4f }, { "flt_keytrack", 1.0f },
            { "fenv_a", 0.001f }, { "fenv_d", 0.3f }, { "fenv_s", 0.15f },
            { "aenv_a", 0.001f }, { "aenv_s", 0.8f }, { "aenv_r", 0.1f },
            { "voice_count", 1.0f }, { "legato", 1.0f }, { "glide", 0.04f }, { "amp_level", -1.9f },
            { "mm4_dst", 4.0f }, { "mm4_amt", 0.35f } }, "Bass" },

        // ---- more LEAD ----
        { "Solar Flare Lead", {
            { "osc1_pulse", 1.0f }, { "osc1_pw", 40.0f }, { "osc2_fine", 8.0f }, { "mix_osc2", 0.8f }, { "mix_drive", 0.3f },
            { "flt_cutoff", 2000.0f }, { "flt_reso", 0.3f }, { "flt_env_amt", 0.3f },
            { "fenv_a", 0.02f }, { "fenv_d", 0.9f }, { "fenv_s", 0.45f }, { "aenv_a", 0.01f }, { "aenv_s", 0.9f }, { "aenv_r", 0.6f },
            { "voice_count", 1.0f }, { "glide", 0.07f }, { "legato", 1.0f },
            { "mm1_src", 3.0f }, { "mm1_dst", 0.0f }, { "mm1_amt", 0.1f }, { "lfo_rate", 5.0f }, { "lfo_delay", 0.5f },
            { "delay_on", 1.0f }, { "delay_time", 0.366f }, { "delay_fb", 0.4f }, { "delay_mix", 0.25f },
            { "reverb_on", 1.0f }, { "reverb_decay", 4.5f }, { "reverb_mix", 0.3f }, { "amp_level", -6.1f } }, "Lead" },

        { "Hollow Pulse Lead", {
            { "osc1_saw", 0.0f }, { "osc1_pulse", 1.0f }, { "osc1_pw", 15.0f }, { "mix_osc2", 0.0f },
            { "flt_cutoff", 2400.0f }, { "flt_reso", 0.15f }, { "flt_env_amt", 0.2f },
            { "fenv_d", 0.6f }, { "fenv_s", 0.5f }, { "aenv_a", 0.005f }, { "aenv_s", 0.9f }, { "aenv_r", 0.5f },
            { "mm1_src", 3.0f }, { "mm1_dst", 2.0f }, { "mm1_amt", 0.25f }, { "lfo_rate", 0.6f },
            { "voice_count", 1.0f }, { "glide", 0.05f }, { "legato", 1.0f },
            { "delay_on", 1.0f }, { "delay_time", 0.488f }, { "delay_fb", 0.35f }, { "delay_mix", 0.2f },
            { "reverb_on", 1.0f }, { "reverb_mix", 0.25f }, { "amp_level", -1.4f } }, "Lead" },

        { "Crying Resonance", {
            { "mix_osc2", 0.4f }, { "osc2_freq", 12.0f }, { "flt_cutoff", 900.0f }, { "flt_reso", 0.7f },
            { "flt_env_amt", 0.35f }, { "flt_keytrack", 2.0f }, { "fenv_a", 0.15f }, { "fenv_d", 1.2f }, { "fenv_s", 0.4f },
            { "aenv_a", 0.02f }, { "aenv_s", 0.9f }, { "aenv_r", 0.8f },
            { "voice_count", 1.0f }, { "glide", 0.1f }, { "legato", 1.0f },
            { "mm1_src", 3.0f }, { "mm1_dst", 0.0f }, { "mm1_amt", 0.12f }, { "lfo_rate", 5.8f }, { "lfo_delay", 0.6f },
            { "reverb_on", 1.0f }, { "reverb_decay", 6.0f }, { "reverb_mix", 0.35f },
            { "mm4_dst", 4.0f }, { "mm4_amt", 0.4f }, { "amp_level", 3.8f } }, "Lead" },

        // ---- more PAD ----
        { "Ember Pad", {
            { "osc1_pulse", 1.0f }, { "osc1_pw", 30.0f }, { "osc2_fine", -8.0f }, { "mix_osc2", 0.8f },
            { "flt_cutoff", 700.0f }, { "flt_reso", 0.35f }, { "flt_env_amt", 0.3f },
            { "fenv_a", 1.8f }, { "fenv_d", 2.5f }, { "fenv_s", 0.5f }, { "fenv_r", 3.0f },
            { "aenv_a", 1.2f }, { "aenv_s", 0.85f }, { "aenv_r", 3.5f },
            { "mm1_src", 3.0f }, { "mm1_dst", 2.0f }, { "mm1_amt", 0.25f }, { "lfo_rate", 0.4f },
            { "voice_spread", 0.8f }, { "analog_age", 0.55f },
            { "chorus_on", 1.0f }, { "chorus_mix", 0.4f },
            { "reverb_on", 1.0f }, { "reverb_size", 0.8f }, { "reverb_decay", 7.0f }, { "reverb_mix", 0.4f }, { "amp_level", -8.0f } }, "Pad" },

        // ---- more PLUCK ----
        { "Marimba Glow", {
            { "osc1_saw", 0.0f }, { "osc1_pulse", 1.0f }, { "osc1_pw", 50.0f }, { "osc2_saw", 0.0f }, { "osc2_tri", 1.0f },
            { "osc2_freq", 12.0f }, { "mix_osc2", 0.7f }, { "flt_cutoff", 1200.0f }, { "flt_env_amt", 0.35f },
            { "fenv_a", 0.001f }, { "fenv_d", 0.12f }, { "fenv_s", 0.0f },
            { "aenv_a", 0.001f }, { "aenv_d", 0.45f }, { "aenv_s", 0.0f }, { "aenv_r", 0.35f },
            { "delay_on", 1.0f }, { "delay_time", 0.366f }, { "delay_fb", 0.3f }, { "delay_mix", 0.2f },
            { "reverb_on", 1.0f }, { "reverb_mix", 0.3f }, { "amp_level", -4.2f } }, "Pluck" },

        { "Tidal Pluck", {
            { "osc2_fine", 10.0f }, { "mix_osc2", 0.7f }, { "flt_cutoff", 450.0f }, { "flt_reso", 0.55f },
            { "flt_env_amt", 0.55f }, { "flt_velocity", 0.4f },
            { "fenv_a", 0.001f }, { "fenv_d", 0.4f }, { "fenv_s", 0.0f }, { "fenv_r", 0.4f },
            { "aenv_a", 0.001f }, { "aenv_d", 1.4f }, { "aenv_s", 0.0f }, { "aenv_r", 0.8f },
            { "voice_spread", 0.7f }, { "chorus_on", 1.0f }, { "chorus_mix", 0.3f },
            { "delay_on", 1.0f }, { "delay_time", 0.488f }, { "delay_fb", 0.45f }, { "delay_mix", 0.3f },
            { "reverb_on", 1.0f }, { "reverb_size", 0.8f }, { "reverb_decay", 6.0f }, { "reverb_mix", 0.35f }, { "amp_level", 3.5f } }, "Pluck" },


        // ---- signature PLUCKS: fast RC filter snap, velocity brightness, a poly-mod pitch blip on the attack ----
        { "Velvet Snap Pluck", {
            { "osc2_fine", 5.0f }, { "mix_osc2", 0.75f }, { "flt_cutoff", 260.0f }, { "flt_reso", 0.3f },
            { "flt_env_amt", 0.7f }, { "flt_keytrack", 1.0f }, { "flt_velocity", 0.45f },
            { "pm_on", 1.0f }, { "pm_fenv_amt", 0.04f }, { "pm_dst_freqa", 1.0f },
            { "fenv_a", 0.001f }, { "fenv_d", 0.22f }, { "fenv_s", 0.0f }, { "fenv_r", 0.25f },
            { "aenv_a", 0.001f }, { "aenv_d", 0.7f }, { "aenv_s", 0.0f }, { "aenv_r", 0.45f },
            { "amp_velocity", 0.6f }, { "analog_age", 0.35f }, { "voice_spread", 0.5f },
            { "chorus_on", 1.0f }, { "chorus_mix", 0.2f },
            { "delay_on", 1.0f }, { "delay_time", 0.366f }, { "delay_fb", 0.38f }, { "delay_mix", 0.22f },
            { "reverb_on", 1.0f }, { "reverb_size", 0.7f }, { "reverb_decay", 4.0f }, { "reverb_mix", 0.28f },
            { "mm4_dst", 4.0f }, { "mm4_amt", 0.35f }, { "amp_level", -0.1f } }, "Pluck" },

        { "Silver Thread Pluck", {
            { "osc1_saw", 0.0f }, { "osc1_pulse", 1.0f }, { "osc1_pw", 18.0f }, { "osc2_saw", 0.0f }, { "osc2_tri", 1.0f },
            { "osc2_freq", 12.0f }, { "mix_osc2", 0.45f }, { "flt_cutoff", 700.0f }, { "flt_reso", 0.2f },
            { "flt_env_amt", 0.5f }, { "flt_keytrack", 2.0f }, { "flt_velocity", 0.4f },
            { "pm_on", 1.0f }, { "pm_fenv_amt", 0.03f }, { "pm_dst_freqa", 1.0f },
            { "fenv_a", 0.001f }, { "fenv_d", 0.12f }, { "fenv_s", 0.0f }, { "fenv_r", 0.15f },
            { "aenv_a", 0.001f }, { "aenv_d", 0.55f }, { "aenv_s", 0.0f }, { "aenv_r", 0.35f },
            { "amp_velocity", 0.55f }, { "voice_spread", 0.6f },
            { "delay_on", 1.0f }, { "delay_time", 0.244f }, { "delay_fb", 0.3f }, { "delay_mix", 0.2f },
            { "reverb_on", 1.0f }, { "reverb_size", 0.6f }, { "reverb_decay", 3.0f }, { "reverb_mix", 0.3f },
            { "mm4_dst", 4.0f }, { "mm4_amt", 0.3f }, { "amp_level", -2.1f } }, "Pluck" },

        { "Deep Water Pluck", {
            { "osc2_saw", 0.0f }, { "osc2_pulse", 1.0f }, { "osc2_pw", 40.0f }, { "osc2_freq", -12.0f }, { "mix_osc2", 0.6f },
            { "flt_cutoff", 180.0f }, { "flt_reso", 0.5f }, { "flt_env_amt", 0.6f }, { "flt_keytrack", 2.0f },
            { "flt_velocity", 0.4f }, { "mix_drive", 0.25f },
            { "fenv_a", 0.001f }, { "fenv_d", 0.35f }, { "fenv_s", 0.0f }, { "fenv_r", 0.35f },
            { "aenv_a", 0.001f }, { "aenv_d", 1.1f }, { "aenv_s", 0.0f }, { "aenv_r", 0.6f },
            { "amp_velocity", 0.5f }, { "analog_age", 0.4f },
            { "delay_on", 1.0f }, { "delay_time", 0.488f }, { "delay_fb", 0.45f }, { "delay_mix", 0.25f },
            { "reverb_on", 1.0f }, { "reverb_size", 0.85f }, { "reverb_decay", 6.0f }, { "reverb_mix", 0.35f },
            { "mm4_dst", 4.0f }, { "mm4_amt", 0.35f }, { "amp_level", -0.7f } }, "Pluck" },

        // ---- KEYS ----
        { "Electric Dusk", {
            { "osc1_saw", 0.0f }, { "osc1_pulse", 1.0f }, { "osc1_pw", 20.0f }, { "osc2_saw", 0.0f }, { "osc2_tri", 1.0f },
            { "osc2_freq", 12.0f }, { "mix_osc2", 0.5f }, { "flt_cutoff", 1400.0f }, { "flt_env_amt", 0.3f },
            { "flt_velocity", 0.4f }, { "fenv_a", 0.001f }, { "fenv_d", 0.6f }, { "fenv_s", 0.15f },
            { "aenv_a", 0.002f }, { "aenv_d", 2.0f }, { "aenv_s", 0.25f }, { "aenv_r", 0.5f },
            { "chorus_on", 1.0f }, { "chorus_mix", 0.4f }, { "reverb_on", 1.0f }, { "reverb_mix", 0.25f }, { "amp_level", -8.1f } }, "Keys" },

        { "Soft Tine", {
            { "osc2_freq", 24.0f }, { "osc1_saw", 0.0f }, { "osc1_pulse", 1.0f }, { "osc1_pw", 50.0f }, { "mix_osc2", 0.0f },
            { "pm_on", 1.0f }, { "pm_osc2_amt", 0.08f }, { "pm_fenv_amt", 0.05f }, { "pm_dst_freqa", 1.0f },
            { "flt_cutoff", 1800.0f }, { "flt_env_amt", 0.25f }, { "fenv_a", 0.001f }, { "fenv_d", 0.5f }, { "fenv_s", 0.1f },
            { "aenv_a", 0.001f }, { "aenv_d", 2.2f }, { "aenv_s", 0.15f }, { "aenv_r", 0.6f },
            { "chorus_on", 1.0f }, { "chorus_mix", 0.35f }, { "reverb_on", 1.0f }, { "reverb_mix", 0.25f }, { "amp_level", -9.2f } }, "Keys" },

        { "Organ of Light", {
            { "osc1_saw", 0.0f }, { "osc1_pulse", 1.0f }, { "osc1_pw", 50.0f }, { "osc2_saw", 0.0f }, { "osc2_pulse", 1.0f },
            { "osc2_pw", 50.0f }, { "osc2_freq", 12.0f }, { "mix_osc2", 0.6f }, { "flt_cutoff", 3000.0f },
            { "flt_env_amt", 0.0f }, { "aenv_a", 0.003f }, { "aenv_s", 1.0f }, { "aenv_r", 0.12f },
            { "mm1_src", 3.0f }, { "mm1_dst", 0.0f }, { "mm1_amt", 0.06f }, { "lfo_rate", 6.2f },
            { "chorus_on", 1.0f }, { "chorus_mix", 0.55f }, { "chorus_rate", 1.2f },
            { "reverb_on", 1.0f }, { "reverb_mix", 0.25f }, { "amp_level", -13.0f } }, "Keys" },

        { "Velvet Clav", {
            { "osc1_saw", 0.0f }, { "osc1_pulse", 1.0f }, { "osc1_pw", 12.0f }, { "mix_osc2", 0.0f },
            { "flt_cutoff", 900.0f }, { "flt_reso", 0.3f }, { "flt_env_amt", 0.5f }, { "flt_velocity", 0.5f },
            { "fenv_a", 0.001f }, { "fenv_d", 0.22f }, { "fenv_s", 0.1f },
            { "aenv_a", 0.001f }, { "aenv_d", 0.7f }, { "aenv_s", 0.2f }, { "aenv_r", 0.15f },
            { "delay_on", 1.0f }, { "delay_time", 0.244f }, { "delay_fb", 0.25f }, { "delay_mix", 0.15f }, { "amp_level", -4.3f } }, "Keys" },

        // ---- more STAB ----
        { "Dub Chord", {
            { "osc2_fine", 6.0f }, { "mix_osc2", 0.7f }, { "flt_cutoff", 900.0f }, { "flt_reso", 0.25f },
            { "flt_env_amt", 0.35f }, { "fenv_a", 0.001f }, { "fenv_d", 0.25f }, { "fenv_s", 0.05f },
            { "aenv_a", 0.001f }, { "aenv_d", 0.3f }, { "aenv_s", 0.0f }, { "aenv_r", 0.2f },
            { "delay_on", 1.0f }, { "delay_time", 0.366f }, { "delay_fb", 0.65f }, { "delay_mix", 0.35f },
            { "reverb_on", 1.0f }, { "reverb_size", 0.7f }, { "reverb_decay", 5.0f }, { "reverb_mix", 0.3f }, { "amp_level", -1.1f } }, "Stab" },

        { "Rave Organ Stab", {
            { "osc1_saw", 0.0f }, { "osc1_pulse", 1.0f }, { "osc1_pw", 50.0f }, { "osc2_saw", 0.0f }, { "osc2_pulse", 1.0f },
            { "osc2_freq", 12.0f }, { "mix_osc2", 0.7f }, { "mix_drive", 0.35f }, { "flt_cutoff", 2400.0f },
            { "flt_env_amt", 0.2f }, { "fenv_d", 0.3f }, { "fenv_s", 0.3f },
            { "aenv_a", 0.001f }, { "aenv_d", 0.4f }, { "aenv_s", 0.3f }, { "aenv_r", 0.25f },
            { "reverb_on", 1.0f }, { "reverb_mix", 0.25f }, { "amp_level", -13.8f } }, "Stab" },

        { "Filtered House Stab", {
            { "mix_osc2", 0.6f }, { "osc2_freq", 12.0f }, { "flt_cutoff", 500.0f }, { "flt_reso", 0.5f },
            { "flt_env_amt", 0.4f }, { "fenv_a", 0.001f }, { "fenv_d", 0.3f }, { "fenv_s", 0.15f },
            { "aenv_a", 0.001f }, { "aenv_d", 0.5f }, { "aenv_s", 0.2f }, { "aenv_r", 0.3f },
            { "mm4_dst", 4.0f }, { "mm4_amt", 0.5f },
            { "chorus_on", 1.0f }, { "chorus_mix", 0.3f }, { "reverb_on", 1.0f }, { "reverb_mix", 0.25f }, { "amp_level", -0.9f } }, "Stab" },

        // ---- more ARP ----
        { "Pulse Runner", {
            { "osc1_saw", 0.0f }, { "osc1_pulse", 1.0f }, { "osc1_pw", 30.0f }, { "mix_osc2", 0.3f }, { "osc2_freq", -12.0f },
            { "flt_cutoff", 700.0f }, { "flt_reso", 0.35f }, { "flt_env_amt", 0.45f },
            { "fenv_a", 0.001f }, { "fenv_d", 0.12f }, { "fenv_s", 0.0f },
            { "aenv_a", 0.001f }, { "aenv_d", 0.2f }, { "aenv_s", 0.0f }, { "aenv_r", 0.12f },
            { "delay_on", 1.0f }, { "delay_time", 0.244f }, { "delay_fb", 0.35f }, { "delay_mix", 0.2f },
            { "mm4_dst", 4.0f }, { "mm4_amt", 0.4f }, { "amp_level", 3.2f } }, "Arp" },

        { "Starlight Arp", {
            { "osc2_freq", 19.0f }, { "mix_osc2", 0.3f }, { "flt_cutoff", 2200.0f }, { "flt_env_amt", 0.3f },
            { "fenv_a", 0.001f }, { "fenv_d", 0.2f }, { "fenv_s", 0.0f },
            { "aenv_a", 0.001f }, { "aenv_d", 0.3f }, { "aenv_s", 0.0f }, { "aenv_r", 0.3f },
            { "voice_spread", 0.8f },
            { "delay_on", 1.0f }, { "delay_time", 0.366f }, { "delay_fb", 0.55f }, { "delay_mix", 0.35f },
            { "reverb_on", 1.0f }, { "reverb_size", 0.85f }, { "reverb_decay", 6.0f }, { "reverb_mix", 0.4f }, { "amp_level", 2.8f } }, "Arp" },

        { "Hypno Sequence", {
            { "mix_osc2", 0.5f }, { "osc2_fine", 7.0f }, { "mix_drive", 0.3f }, { "flt_cutoff", 400.0f }, { "flt_reso", 0.6f },
            { "flt_env_amt", 0.45f }, { "fenv_a", 0.001f }, { "fenv_d", 0.18f }, { "fenv_s", 0.05f },
            { "aenv_a", 0.001f }, { "aenv_d", 0.25f }, { "aenv_s", 0.1f }, { "aenv_r", 0.15f },
            { "mm3_amt", 0.2f }, { "lfo_rate", 0.1f }, { "lfo_amount", 1.0f },
            { "delay_on", 1.0f }, { "delay_time", 0.366f }, { "delay_fb", 0.4f }, { "delay_mix", 0.22f },
            { "mm4_dst", 4.0f }, { "mm4_amt", 0.4f }, { "amp_level", 4.3f } }, "Arp" },

        { "Echo Grid", {
            { "osc1_saw", 0.0f }, { "osc1_pulse", 1.0f }, { "osc1_pw", 45.0f }, { "mix_osc2", 0.0f },
            { "flt_cutoff", 1500.0f }, { "flt_reso", 0.2f }, { "flt_env_amt", 0.3f },
            { "fenv_a", 0.001f }, { "fenv_d", 0.1f }, { "fenv_s", 0.0f },
            { "aenv_a", 0.001f }, { "aenv_d", 0.15f }, { "aenv_s", 0.0f }, { "aenv_r", 0.1f },
            { "delay_on", 1.0f }, { "delay_time", 0.366f }, { "delay_fb", 0.7f }, { "delay_mix", 0.4f },
            { "reverb_on", 1.0f }, { "reverb_mix", 0.2f }, { "amp_level", 2.7f } }, "Arp" },

        // ---- DRUMS (pitch sweeps: filter envelope -> oscillator pitch through matrix slot 1) ----
        { "Analog Kick", {
            { "osc2_saw", 0.0f }, { "osc2_tri", 1.0f }, { "osc2_freq", -24.0f }, { "mix_osc1", 0.0f }, { "mix_osc2", 1.0f },
            { "mix_noise", 0.04f }, { "mix_drive", 0.5f }, { "flt_cutoff", 700.0f }, { "flt_env_amt", 0.3f }, { "flt_keytrack", 0.0f },
            { "mm1_src", 0.0f }, { "mm1_dst", 1.0f }, { "mm1_amt", 0.8f },
            { "fenv_a", 0.001f }, { "fenv_d", 0.09f }, { "fenv_s", 0.0f }, { "fenv_r", 0.09f },
            { "aenv_a", 0.001f }, { "aenv_d", 0.45f }, { "aenv_s", 0.0f }, { "aenv_r", 0.25f },
            { "amp_velocity", 0.3f }, { "voice_count", 1.0f }, { "analog_age", 0.1f }, { "amp_level", 1.6f } }, "Drums" },

        { "Sub Kick", {
            { "osc2_saw", 0.0f }, { "osc2_tri", 1.0f }, { "osc2_freq", -24.0f }, { "mix_osc1", 0.0f }, { "mix_osc2", 1.0f },
            { "mix_drive", 0.3f }, { "flt_cutoff", 400.0f }, { "flt_env_amt", 0.2f }, { "flt_keytrack", 0.0f },
            { "mm1_src", 0.0f }, { "mm1_dst", 1.0f }, { "mm1_amt", 0.65f },
            { "fenv_a", 0.001f }, { "fenv_d", 0.14f }, { "fenv_s", 0.0f }, { "fenv_r", 0.14f },
            { "aenv_a", 0.001f }, { "aenv_d", 0.9f }, { "aenv_s", 0.0f }, { "aenv_r", 0.5f },
            { "voice_count", 1.0f }, { "analog_age", 0.1f }, { "amp_level", 2.9f } }, "Drums" },

        { "Tight Snare", {
            { "osc2_saw", 0.0f }, { "osc2_tri", 1.0f }, { "mix_osc1", 0.0f }, { "mix_osc2", 0.45f }, { "mix_noise", 0.85f },
            { "flt_cutoff", 5000.0f }, { "flt_reso", 0.2f }, { "flt_env_amt", 0.2f }, { "flt_keytrack", 0.0f },
            { "mm1_src", 0.0f }, { "mm1_dst", 1.0f }, { "mm1_amt", 0.5f },
            { "fenv_a", 0.001f }, { "fenv_d", 0.1f }, { "fenv_s", 0.0f },
            { "aenv_a", 0.001f }, { "aenv_d", 0.18f }, { "aenv_s", 0.0f }, { "aenv_r", 0.15f },
            { "voice_count", 1.0f }, { "reverb_on", 1.0f }, { "reverb_size", 0.3f }, { "reverb_decay", 1.2f }, { "reverb_mix", 0.15f }, { "amp_level", 6.0f }, { "mix_drive", 0.55f } }, "Drums" },

        { "Noise Clap", {
            { "mix_osc1", 0.0f }, { "mix_osc2", 0.0f }, { "mix_noise", 1.0f }, { "flt_cutoff", 1600.0f }, { "flt_reso", 0.5f },
            { "flt_env_amt", 0.3f }, { "flt_keytrack", 0.0f }, { "fenv_a", 0.001f }, { "fenv_d", 0.08f }, { "fenv_s", 0.0f },
            { "aenv_a", 0.001f }, { "aenv_d", 0.16f }, { "aenv_s", 0.0f }, { "aenv_r", 0.18f },
            { "voice_count", 1.0f }, { "reverb_on", 1.0f }, { "reverb_size", 0.35f }, { "reverb_decay", 1.5f }, { "reverb_mix", 0.25f }, { "amp_level", 5.5f }, { "mix_drive", 0.6f } }, "Drums" },

        { "Closed Hat", {
            { "mix_osc1", 0.0f }, { "mix_osc2", 0.0f }, { "mix_noise", 1.0f }, { "flt_cutoff", 12000.0f }, { "flt_reso", 0.35f },
            { "flt_keytrack", 0.0f }, { "aenv_a", 0.001f }, { "aenv_d", 0.06f }, { "aenv_s", 0.0f }, { "aenv_r", 0.05f },
            { "amp_velocity", 0.6f }, { "voice_count", 1.0f }, { "amp_level", 6.0f }, { "mix_drive", 0.7f } }, "Drums" },

        { "Open Hat", {
            { "mix_osc1", 0.0f }, { "mix_osc2", 0.0f }, { "mix_noise", 1.0f }, { "flt_cutoff", 10000.0f }, { "flt_reso", 0.4f },
            { "flt_keytrack", 0.0f }, { "aenv_a", 0.001f }, { "aenv_d", 0.35f }, { "aenv_s", 0.0f }, { "aenv_r", 0.3f },
            { "amp_velocity", 0.5f }, { "voice_count", 1.0f }, { "amp_level", -4.0f }, { "mix_drive", 0.5f } }, "Drums" },

        { "Analog Tom", {
            { "osc2_saw", 0.0f }, { "osc2_tri", 1.0f }, { "osc2_freq", -12.0f }, { "mix_osc1", 0.0f }, { "mix_osc2", 1.0f },
            { "mix_noise", 0.05f }, { "flt_cutoff", 1200.0f }, { "flt_keytrack", 0.0f },
            { "mm1_src", 0.0f }, { "mm1_dst", 1.0f }, { "mm1_amt", 0.55f },
            { "fenv_a", 0.001f }, { "fenv_d", 0.15f }, { "fenv_s", 0.0f },
            { "aenv_a", 0.001f }, { "aenv_d", 0.4f }, { "aenv_s", 0.0f }, { "aenv_r", 0.3f },
            { "reverb_on", 1.0f }, { "reverb_size", 0.4f }, { "reverb_mix", 0.15f }, { "amp_level", 5.8f } }, "Drums" },

        { "Rim Click", {
            { "osc1_saw", 0.0f }, { "osc1_pulse", 1.0f }, { "osc1_pw", 20.0f }, { "osc1_freq", 12.0f }, { "mix_osc2", 0.0f },
            { "mix_osc1", 1.0f }, { "mix_drive", 0.75f }, { "mix_noise", 0.35f }, { "flt_cutoff", 3000.0f }, { "flt_reso", 0.6f }, { "flt_keytrack", 0.0f },
            { "aenv_a", 0.001f }, { "aenv_d", 0.06f }, { "aenv_s", 0.0f }, { "aenv_r", 0.04f },
            { "voice_count", 1.0f }, { "amp_level", 6.0f } }, "Drums" },

        { "Metal Cowbell", {
            { "osc1_saw", 0.0f }, { "osc1_pulse", 1.0f }, { "osc1_pw", 50.0f }, { "osc1_freq", 12.0f },
            { "osc2_saw", 0.0f }, { "osc2_pulse", 1.0f }, { "osc2_pw", 50.0f }, { "osc2_freq", 19.0f }, { "osc2_fine", -20.0f },
            { "mix_osc2", 0.9f }, { "flt_cutoff", 2600.0f }, { "flt_reso", 0.3f }, { "flt_keytrack", 0.0f },
            { "aenv_a", 0.001f }, { "aenv_d", 0.25f }, { "aenv_s", 0.0f }, { "aenv_r", 0.2f },
            { "voice_count", 1.0f }, { "amp_level", -5.8f } }, "Drums" },

        // ---- more ATMOS & FX ----
        { "Sweep Down FX", {
            { "mix_osc2", 0.5f }, { "mix_noise", 0.3f }, { "flt_cutoff", 8000.0f }, { "flt_reso", 0.6f }, { "flt_env_amt", -0.8f },
            { "fenv_a", 0.001f }, { "fenv_d", 3.5f }, { "fenv_s", 0.0f },
            { "aenv_a", 0.01f }, { "aenv_d", 4.0f }, { "aenv_s", 0.0f }, { "aenv_r", 2.0f },
            { "delay_on", 1.0f }, { "delay_time", 0.488f }, { "delay_fb", 0.5f }, { "delay_mix", 0.3f },
            { "reverb_on", 1.0f }, { "reverb_size", 0.9f }, { "reverb_decay", 9.0f }, { "reverb_mix", 0.45f }, { "amp_level", -2.4f } }, "Atmos & FX" },

        { "Laser Zap", {
            { "osc2_saw", 0.0f }, { "osc2_tri", 1.0f }, { "mix_osc1", 0.0f }, { "mix_osc2", 1.0f }, { "flt_cutoff", 6000.0f },
            { "flt_reso", 0.4f }, { "mm1_src", 0.0f }, { "mm1_dst", 1.0f }, { "mm1_amt", 1.0f },
            { "fenv_a", 0.001f }, { "fenv_d", 0.25f }, { "fenv_s", 0.0f },
            { "aenv_a", 0.001f }, { "aenv_d", 0.35f }, { "aenv_s", 0.0f }, { "aenv_r", 0.3f },
            { "delay_on", 1.0f }, { "delay_time", 0.244f }, { "delay_fb", 0.5f }, { "delay_mix", 0.3f }, { "amp_level", 4.6f } }, "Atmos & FX" },

        { "Wind Tunnel", {
            { "mix_osc1", 0.0f }, { "mix_osc2", 0.0f }, { "mix_noise", 1.0f }, { "flt_cutoff", 900.0f }, { "flt_reso", 0.75f },
            { "mm3_amt", 0.4f }, { "lfo_rate", 0.15f }, { "lfo_amount", 1.0f },
            { "aenv_a", 2.0f }, { "aenv_s", 1.0f }, { "aenv_r", 4.0f }, { "voice_spread", 0.9f },
            { "reverb_on", 1.0f }, { "reverb_size", 1.0f }, { "reverb_decay", 12.0f }, { "reverb_mix", 0.5f }, { "amp_level", 6.0f } }, "Atmos & FX" },
    };

    // Order: Init, then the categories as the browser lists them.
    static constexpr const char* order[] { "Init", "Bass", "Lead", "Pad", "Pluck", "Keys", "Stab", "Arp", "Drums", "Atmos & FX" };
    const auto rank = [] (const char* c) {
        for (int i = 0; i < static_cast<int> (std::size (order)); ++i)
            if (std::string_view (order[i]) == c)
                return i;
        return static_cast<int> (std::size (order));
    };
    std::stable_sort (bank.begin(), bank.end(), [&] (const FactoryPreset& a, const FactoryPreset& b) { return rank (a.category) < rank (b.category); });
    return bank;
}

constexpr const char* presetExtension = ".augur5";
} // namespace

PresetManager::PresetManager (juce::AudioProcessorValueTreeState& s) : state (s) {}

int PresetManager::getNumFactoryPresets() const noexcept { return static_cast<int> (factoryBank().size()); }

int PresetManager::findFactory (const juce::String& name) const
{
    for (int i = 0; i < getNumFactoryPresets(); ++i)
        if (name == factoryBank()[static_cast<size_t> (i)].name)
            return i;
    return 0;
}

juce::String PresetManager::getFactoryCategory (int index) const
{
    return factoryBank()[static_cast<size_t> (juce::jlimit (0, getNumFactoryPresets() - 1, index))].category;
}

juce::String PresetManager::getFactoryName (int index) const
{
    return factoryBank()[static_cast<size_t> (juce::jlimit (0, getNumFactoryPresets() - 1, index))].name;
}

void PresetManager::setParam (const juce::String& id, float value)
{
    if (auto* p = state.getParameter (id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (value));
        p->endChangeGesture();
    }
}

void PresetManager::resetToDefaults()
{
    for (auto* p : state.processor.getParameters())
    {
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
        {
            rp->beginChangeGesture();
            rp->setValueNotifyingHost (rp->getDefaultValue());
            rp->endChangeGesture();
        }
    }
}

void PresetManager::loadFactory (int index)
{
    index = juce::jlimit (0, getNumFactoryPresets() - 1, index);
    const auto& preset = factoryBank()[static_cast<size_t> (index)];
    resetToDefaults();
    for (const auto& s : preset.settings)
        setParam (s.id, s.value);
    currentFactory = index;
    currentUserFile = juce::File();
    currentName = preset.name;
    if (onPresetLoaded)
        onPresetLoaded();
}

juce::File PresetManager::getUserFolder()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
        .getChildFile ("TONAL LAB")
        .getChildFile ("AUGUR-5")
        .getChildFile ("Presets");
}

juce::Array<juce::File> PresetManager::getUserPresets() const
{
    auto files = getUserFolder().findChildFiles (juce::File::findFiles, false, juce::String ("*") + presetExtension);
    files.sort();
    return files;
}

void PresetManager::loadUser (const juce::File& file)
{
    auto xml = juce::XmlDocument::parse (file);
    if (xml == nullptr || ! xml->hasTagName ("AUGUR5_PRESET"))
        return;

    resetToDefaults();
    if (auto* paramsXml = xml->getChildByName ("PARAMS"))
        for (auto* e : paramsXml->getChildIterator())
            setParam (e->getStringAttribute ("id"), static_cast<float> (e->getDoubleAttribute ("value")));

    currentUserFile = file;
    currentName = file.getFileNameWithoutExtension();
    if (onPresetLoaded)
        onPresetLoaded();
}

bool PresetManager::saveUser (const juce::String& name)
{
    const auto safe = juce::File::createLegalFileName (name.trim());
    if (safe.isEmpty())
        return false;

    juce::XmlElement root ("AUGUR5_PRESET");
    root.setAttribute ("formatVersion", formatVersion);
    root.setAttribute ("name", name);
    auto* paramsXml = root.createNewChildElement ("PARAMS");
    for (auto* p : state.processor.getParameters())
    {
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
        {
            auto* e = paramsXml->createNewChildElement ("P");
            e->setAttribute ("id", rp->getParameterID());
            e->setAttribute ("value", rp->convertFrom0to1 (rp->getValue()));
        }
    }

    const auto folder = getUserFolder();
    folder.createDirectory();
    const auto file = folder.getChildFile (safe + presetExtension);
    if (! root.writeTo (file))
        return false;
    currentUserFile = file;
    currentName = name;
    return true;
}

int PresetManager::currentFlatIndex() const
{
    if (currentUserFile.existsAsFile())
    {
        const auto users = getUserPresets();
        const int u = users.indexOf (currentUserFile);
        if (u >= 0)
            return getNumFactoryPresets() + u;
    }
    return currentFactory;
}

void PresetManager::loadFlat (int index)
{
    const auto users = getUserPresets();
    const int total = getNumFactoryPresets() + users.size();
    index = (index % total + total) % total;
    if (index < getNumFactoryPresets())
        loadFactory (index);
    else
        loadUser (users[index - getNumFactoryPresets()]);
}

void PresetManager::next() { loadFlat (currentFlatIndex() + 1); }
void PresetManager::previous() { loadFlat (currentFlatIndex() - 1); }

} // namespace augur5
