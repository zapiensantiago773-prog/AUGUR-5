// Expansion factory library (included inside buildBank's initializer list).
// Values in real units, like the rest of the bank. amp_level is set by tools/level_presets.py.
//
// Choice indices used here:
//   flt_model 0 CEM3320, 1 SSM2040, 2 CASCADE, 3 MULTIMODE, 4 BITE | flt_slope 0 24 dB, 1 12 dB | flt_mode 0 LP, 1 BP, 2 HP
//   lfo2_wave 0 SINE, 1 TRI, 2 SAW UP, 3 SAW DN, 4 SQR, 5 S&H, 6 SMOOTH
//   matrix sources 0 FENV, 1 AENV, 2 OSC2, 3 LFO, 4 WHEEL, 5 VEL, 6 AT, 7 NOISE, 8 MODENV, 9 LFO2, 10 KEY, 11 RANDOM
//   matrix dests 0 OSC1 FREQ, 1 OSC2 FREQ, 2 PW1, 3 PW2, 4 CUTOFF, 5 RESO, 6 AMP, 7 LFO RATE, 8 FM, 9 RING, 10 SUB,
//                11 DRIVE, 12 OSC1 LVL, 13 OSC2 LVL, 14 NOISE LVL, 15 LFO2 RATE
//   delay_div 5 1/8, 6 1/8D, 8 1/4, 9 1/4D | arp_rate 2 1/8, 3 1/8T, 5 1/16, 6 1/16T, 7 1/32 | arp_mode 0 UP, 1 DN, 2 UPDN, 3 RND, 4 ORDER
//   chorus_mode 1 I, 2 II, 3 I+II | reverb_type 1 PLATE | voice_mode 1 DUO

        // ================= SIGNATURE: the melancholic heart of the instrument =================
        { "Melancholia Fuzz Pad", {
            { "mix_osc1", 0.8f }, { "mix_osc2", 0.75f }, { "osc2_fine", 9.0f }, { "flt_cutoff", 1400.0f }, { "flt_reso", 0.2f },
            { "flt_env_amt", 0.15f }, { "aenv_a", 1.2f }, { "aenv_d", 2.0f }, { "aenv_s", 0.85f }, { "aenv_r", 3.5f },
            { "fenv_a", 1.5f }, { "fenv_d", 3.0f }, { "fenv_s", 0.6f }, { "fenv_r", 3.0f },
            { "lfo2_wave", 6.0f }, { "lfo2_rate", 0.12f }, { "lfo2_retrig", 0.0f }, { "mm5_src", 9.0f }, { "mm5_dst", 4.0f }, { "mm5_amt", 0.12f },
            { "fuzz_on", 1.0f }, { "fuzz_sustain", 0.55f }, { "fuzz_tone", 0.35f }, { "fuzz_volume", 0.6f }, { "fuzz_mix", 0.6f },
            { "phaser_on", 1.0f }, { "phaser_rate", 0.08f }, { "phaser_depth", 0.6f }, { "phaser_fb", 0.3f }, { "phaser_mix", 0.35f },
            { "voice_spread", 0.8f }, { "analog_age", 0.5f },
            { "reverb_on", 1.0f }, { "reverb_type", 1.0f }, { "reverb_size", 0.85f }, { "reverb_decay", 7.0f }, { "reverb_mix", 0.4f }, { "amp_level", 3.5f } }, "Signature" },

        { "Grey Skies Lead", {
            { "mix_osc1", 1.0f }, { "mix_osc2", 0.5f }, { "osc2_freq", -12.0f }, { "osc2_fine", 4.0f }, { "flt_cutoff", 2200.0f },
            { "flt_reso", 0.25f }, { "flt_env_amt", 0.2f }, { "fenv_a", 0.05f }, { "fenv_d", 1.5f }, { "fenv_s", 0.6f },
            { "aenv_a", 0.03f }, { "aenv_s", 0.9f }, { "aenv_r", 0.6f }, { "voice_count", 1.0f }, { "legato", 1.0f }, { "glide", 0.09f },
            { "lfo_rate", 5.0f }, { "lfo_delay", 0.6f }, { "mm4_src", 3.0f }, { "mm4_dst", 0.0f }, { "mm4_amt", 0.08f },
            { "fuzz_on", 1.0f }, { "fuzz_sustain", 0.7f }, { "fuzz_tone", 0.45f }, { "fuzz_volume", 0.6f }, { "fuzz_mix", 0.8f },
            { "delay_on", 1.0f }, { "delay_sync", 1.0f }, { "delay_div", 6.0f }, { "delay_pingpong", 1.0f }, { "delay_fb", 0.45f }, { "delay_mix", 0.3f },
            { "reverb_on", 1.0f }, { "reverb_type", 1.0f }, { "reverb_decay", 4.0f }, { "reverb_mix", 0.3f }, { "amp_level", 1.7f } }, "Signature" },

        { "Wall of Dusk", {
            { "voice_mode", 1.0f }, { "voice_count", 10.0f }, { "voice_detune", 0.6f }, { "mix_osc1", 0.8f }, { "mix_osc2", 0.7f },
            { "osc2_freq", 7.0f }, { "flt_cutoff", 1800.0f }, { "flt_reso", 0.15f }, { "flt_env_amt", 0.1f },
            { "aenv_a", 0.8f }, { "aenv_s", 1.0f }, { "aenv_r", 4.0f },
            { "fuzz_on", 1.0f }, { "fuzz_sustain", 0.75f }, { "fuzz_tone", 0.4f }, { "fuzz_volume", 0.55f }, { "fuzz_mix", 0.45f },
            { "chorus_on", 1.0f }, { "chorus_mode", 1.0f }, { "chorus_mix", 0.5f },
            { "reverb_on", 1.0f }, { "reverb_size", 1.0f }, { "reverb_decay", 10.0f }, { "reverb_mix", 0.45f }, { "amp_level", -5.8f } }, "Signature" },

        { "Tape Memory Keys", {
            { "mix_osc1", 0.7f }, { "mix_osc2", 0.5f }, { "osc2_tri", 1.0f }, { "osc2_saw", 0.0f }, { "osc2_freq", 12.0f },
            { "flt_cutoff", 900.0f }, { "flt_reso", 0.15f }, { "flt_env_amt", 0.4f }, { "flt_velocity", 0.4f },
            { "fenv_a", 0.002f }, { "fenv_d", 0.9f }, { "fenv_s", 0.25f }, { "aenv_a", 0.002f }, { "aenv_d", 2.5f }, { "aenv_s", 0.3f }, { "aenv_r", 0.9f },
            { "analog_age", 0.85f }, { "fuzz_on", 1.0f }, { "fuzz_sustain", 0.3f }, { "fuzz_tone", 0.3f }, { "fuzz_volume", 0.6f }, { "fuzz_mix", 0.35f },
            { "chorus_on", 1.0f }, { "chorus_mode", 2.0f }, { "chorus_mix", 0.5f },
            { "delay_on", 1.0f }, { "delay_sync", 1.0f }, { "delay_div", 9.0f }, { "delay_fb", 0.35f }, { "delay_mix", 0.22f },
            { "reverb_on", 1.0f }, { "reverb_mix", 0.25f }, { "amp_level", -0.3f } }, "Signature" },

        { "Afterhours Bloom", {
            { "mix_osc1", 0.8f }, { "mix_osc2", 0.7f }, { "osc2_fine", -8.0f }, { "mix_ring", 0.12f }, { "flt_cutoff", 380.0f },
            { "flt_reso", 0.35f }, { "flt_env_amt", 0.0f }, { "menv_a", 3.0f }, { "menv_d", 4.0f }, { "menv_s", 0.7f }, { "menv_r", 4.0f },
            { "mm5_src", 8.0f }, { "mm5_dst", 4.0f }, { "mm5_amt", 0.55f }, { "aenv_a", 1.5f }, { "aenv_s", 1.0f }, { "aenv_r", 4.0f },
            { "fuzz_on", 1.0f }, { "fuzz_sustain", 0.4f }, { "fuzz_tone", 0.5f }, { "fuzz_volume", 0.55f }, { "fuzz_mix", 0.3f },
            { "voice_spread", 0.9f }, { "reverb_on", 1.0f }, { "reverb_type", 1.0f }, { "reverb_size", 0.9f }, { "reverb_decay", 8.0f }, { "reverb_mix", 0.45f }, { "amp_level", -2.1f } }, "Signature" },

        { "Lost Signal", {
            { "osc2_freq", 19.0f }, { "osc2_tri", 1.0f }, { "osc2_saw", 0.0f }, { "mix_osc1", 0.9f }, { "mix_osc2", 0.0f }, { "osc_xmod", 0.3f },
            { "menv_a", 0.001f }, { "menv_d", 1.2f }, { "menv_s", 0.1f }, { "mm5_src", 8.0f }, { "mm5_dst", 8.0f }, { "mm5_amt", 0.4f },
            { "osc1_saw", 0.0f }, { "osc1_pulse", 1.0f }, { "osc1_pw", 50.0f },
            { "flt_cutoff", 5000.0f }, { "aenv_a", 0.002f }, { "aenv_d", 3.0f }, { "aenv_s", 0.2f }, { "aenv_r", 2.5f },
            { "fuzz_on", 1.0f }, { "fuzz_sustain", 0.45f }, { "fuzz_tone", 0.6f }, { "fuzz_volume", 0.5f }, { "fuzz_mix", 0.4f },
            { "delay_on", 1.0f }, { "delay_sync", 1.0f }, { "delay_div", 6.0f }, { "delay_pingpong", 1.0f }, { "delay_fb", 0.5f }, { "delay_mix", 0.3f },
            { "reverb_on", 1.0f }, { "reverb_type", 1.0f }, { "reverb_mix", 0.35f }, { "amp_level", -3.2f } }, "Signature" },

        { "Sunset Shoegaze", {
            { "voice_mode", 1.0f }, { "voice_count", 10.0f }, { "voice_detune", 0.8f }, { "mix_osc1", 0.9f }, { "mix_osc2", 0.8f },
            { "osc2_freq", 12.0f }, { "osc2_fine", 6.0f }, { "flt_cutoff", 2500.0f }, { "aenv_a", 0.4f }, { "aenv_s", 1.0f }, { "aenv_r", 3.0f },
            { "fuzz_on", 1.0f }, { "fuzz_sustain", 0.85f }, { "fuzz_tone", 0.5f }, { "fuzz_volume", 0.55f }, { "fuzz_mix", 0.55f },
            { "phaser_on", 1.0f }, { "phaser_rate", 0.15f }, { "phaser_depth", 0.8f }, { "phaser_fb", 0.5f }, { "phaser_mix", 0.4f },
            { "reverb_on", 1.0f }, { "reverb_size", 1.0f }, { "reverb_decay", 12.0f }, { "reverb_mix", 0.5f }, { "amp_level", -0.8f } }, "Signature" },

        { "Velvet Horizon", {
            { "flt_model", 3.0f }, { "flt_mode", 1.0f }, { "flt_slope", 1.0f }, { "flt_cutoff", 800.0f }, { "flt_reso", 0.55f },
            { "mix_osc1", 0.8f }, { "mix_osc2", 0.8f }, { "osc2_fine", 7.0f }, { "mix_noise", 0.08f },
            { "lfo2_wave", 1.0f }, { "lfo2_rate", 0.07f }, { "lfo2_retrig", 0.0f }, { "mm5_src", 9.0f }, { "mm5_dst", 4.0f }, { "mm5_amt", 0.35f },
            { "aenv_a", 1.0f }, { "aenv_s", 1.0f }, { "aenv_r", 3.0f }, { "voice_spread", 0.8f },
            { "chorus_on", 1.0f }, { "chorus_mode", 1.0f }, { "chorus_mix", 0.4f },
            { "reverb_on", 1.0f }, { "reverb_size", 0.9f }, { "reverb_decay", 8.0f }, { "reverb_mix", 0.4f }, { "amp_level", -14.5f } }, "Signature" },

        { "Cathedral Pluck", {
            { "mix_osc2", 0.7f }, { "osc2_freq", 12.0f }, { "osc2_fine", 4.0f }, { "flt_cutoff", 300.0f }, { "flt_reso", 0.35f },
            { "flt_env_amt", 0.6f }, { "flt_velocity", 0.4f }, { "fenv_a", 0.001f }, { "fenv_d", 0.35f }, { "fenv_s", 0.0f },
            { "aenv_a", 0.001f }, { "aenv_d", 1.6f }, { "aenv_s", 0.0f }, { "aenv_r", 1.2f },
            { "delay_on", 1.0f }, { "delay_sync", 1.0f }, { "delay_div", 6.0f }, { "delay_pingpong", 1.0f }, { "delay_fb", 0.5f }, { "delay_mix", 0.3f },
            { "reverb_on", 1.0f }, { "reverb_type", 1.0f }, { "reverb_size", 1.0f }, { "reverb_decay", 9.0f }, { "reverb_mix", 0.45f }, { "amp_level", 1.9f } }, "Signature" },

        { "Nocturne Arp", {
            { "arp_on", 1.0f }, { "arp_mode", 2.0f }, { "arp_rate", 5.0f }, { "arp_oct", 2.0f }, { "arp_gate", 0.45f },
            { "mix_osc2", 0.6f }, { "osc2_fine", 6.0f }, { "flt_cutoff", 500.0f }, { "flt_reso", 0.4f }, { "flt_env_amt", 0.45f },
            { "fenv_a", 0.001f }, { "fenv_d", 0.25f }, { "fenv_s", 0.1f }, { "aenv_a", 0.001f }, { "aenv_d", 0.4f }, { "aenv_s", 0.3f }, { "aenv_r", 0.3f },
            { "lfo2_wave", 0.0f }, { "lfo2_rate", 0.1f }, { "lfo2_retrig", 0.0f }, { "mm5_src", 9.0f }, { "mm5_dst", 4.0f }, { "mm5_amt", 0.2f },
            { "fuzz_on", 1.0f }, { "fuzz_sustain", 0.35f }, { "fuzz_tone", 0.45f }, { "fuzz_volume", 0.6f }, { "fuzz_mix", 0.3f },
            { "delay_on", 1.0f }, { "delay_sync", 1.0f }, { "delay_div", 6.0f }, { "delay_pingpong", 1.0f }, { "delay_fb", 0.45f }, { "delay_mix", 0.3f },
            { "reverb_on", 1.0f }, { "reverb_mix", 0.3f }, { "amp_level", 3.7f } }, "Signature" },

        { "Warm Circuit Bass", {
            { "voice_count", 1.0f }, { "legato", 1.0f }, { "glide", 0.03f }, { "mix_osc1", 1.0f }, { "mix_osc2", 0.0f }, { "mix_sub", 0.7f },
            { "flt_cutoff", 280.0f }, { "flt_reso", 0.25f }, { "flt_env_amt", 0.4f }, { "fenv_a", 0.001f }, { "fenv_d", 0.35f }, { "fenv_s", 0.2f },
            { "aenv_a", 0.002f }, { "aenv_s", 0.9f }, { "aenv_r", 0.15f },
            { "fuzz_on", 1.0f }, { "fuzz_sustain", 0.45f }, { "fuzz_tone", 0.3f }, { "fuzz_volume", 0.6f }, { "fuzz_mix", 0.3f }, { "amp_level", -1.4f } }, "Signature" },

        { "Faded Polaroid", {
            { "mix_osc1", 0.7f }, { "mix_osc2", 0.6f }, { "osc1_saw", 0.0f }, { "osc1_pulse", 1.0f }, { "osc1_pw", 35.0f }, { "osc2_fine", 12.0f },
            { "flt_cutoff", 1100.0f }, { "flt_env_amt", 0.25f }, { "fenv_d", 1.5f }, { "fenv_s", 0.3f },
            { "aenv_a", 0.01f }, { "aenv_d", 2.0f }, { "aenv_s", 0.5f }, { "aenv_r", 1.5f }, { "analog_age", 0.95f },
            { "lfo_rate", 0.3f }, { "mm3_src", 3.0f }, { "mm3_dst", 2.0f }, { "mm3_amt", 0.3f },
            { "fuzz_on", 1.0f }, { "fuzz_sustain", 0.25f }, { "fuzz_tone", 0.25f }, { "fuzz_volume", 0.65f }, { "fuzz_mix", 0.25f },
            { "chorus_on", 1.0f }, { "chorus_mode", 3.0f }, { "chorus_mix", 0.4f }, { "reverb_on", 1.0f }, { "reverb_mix", 0.3f }, { "amp_level", -3.7f } }, "Signature" },

        // ================= BASS =================
        { "Sub Anchor", {
            { "voice_count", 1.0f }, { "legato", 1.0f }, { "osc1_saw", 0.0f }, { "osc1_pulse", 1.0f }, { "mix_osc1", 0.4f }, { "mix_osc2", 0.0f },
            { "mix_sub", 1.0f }, { "flt_cutoff", 180.0f }, { "flt_env_amt", 0.2f }, { "fenv_d", 0.2f }, { "fenv_s", 0.1f },
            { "aenv_a", 0.002f }, { "aenv_s", 1.0f }, { "aenv_r", 0.1f }, { "amp_level", -5.4f } }, "Bass" },

        { "Rubber Pulse Bass", {
            { "voice_count", 1.0f }, { "osc1_saw", 0.0f }, { "osc1_pulse", 1.0f }, { "osc1_pw", 20.0f }, { "mix_osc1", 1.0f }, { "mix_osc2", 0.0f },
            { "menv_a", 0.001f }, { "menv_d", 0.25f }, { "menv_s", 0.0f }, { "mm5_src", 8.0f }, { "mm5_dst", 2.0f }, { "mm5_amt", 0.6f },
            { "flt_cutoff", 350.0f }, { "flt_reso", 0.3f }, { "flt_env_amt", 0.35f }, { "fenv_d", 0.2f }, { "fenv_s", 0.0f },
            { "aenv_a", 0.001f }, { "aenv_d", 0.3f }, { "aenv_s", 0.6f }, { "aenv_r", 0.1f }, { "amp_level", -1.2f } }, "Bass" },

        { "Cascade Rolling Bass", {
            { "flt_model", 2.0f }, { "voice_count", 1.0f }, { "mix_osc2", 0.8f }, { "osc2_freq", -12.0f }, { "osc2_fine", 5.0f },
            { "flt_cutoff", 240.0f }, { "flt_reso", 0.45f }, { "flt_env_amt", 0.5f }, { "fenv_a", 0.001f }, { "fenv_d", 0.18f }, { "fenv_s", 0.0f },
            { "aenv_a", 0.001f }, { "aenv_d", 0.25f }, { "aenv_s", 0.0f }, { "aenv_r", 0.1f }, { "flt_velocity", 0.35f }, { "amp_level", 5.3f } }, "Bass" },

        { "Bite Acid Line", {
            { "flt_model", 4.0f }, { "voice_count", 1.0f }, { "legato", 1.0f }, { "glide", 0.05f }, { "mix_osc1", 1.0f }, { "mix_osc2", 0.0f },
            { "flt_cutoff", 300.0f }, { "flt_reso", 0.82f }, { "flt_env_amt", 0.6f }, { "fenv_a", 0.001f }, { "fenv_d", 0.22f }, { "fenv_s", 0.05f },
            { "aenv_a", 0.001f }, { "aenv_s", 0.8f }, { "aenv_r", 0.08f }, { "flt_velocity", 0.5f }, { "mix_drive", 0.5f },
            { "delay_on", 1.0f }, { "delay_sync", 1.0f }, { "delay_div", 6.0f }, { "delay_fb", 0.3f }, { "delay_mix", 0.15f }, { "amp_level", -6.5f } }, "Bass" },

        { "FM Knock Bass", {
            { "voice_count", 1.0f }, { "osc2_tri", 1.0f }, { "osc2_saw", 0.0f }, { "osc2_freq", 12.0f }, { "mix_osc2", 0.0f },
            { "osc_xmod", 0.0f }, { "menv_a", 0.001f }, { "menv_d", 0.12f }, { "menv_s", 0.0f }, { "mm5_src", 8.0f }, { "mm5_dst", 8.0f }, { "mm5_amt", 0.7f },
            { "osc1_saw", 0.0f }, { "osc1_pulse", 1.0f }, { "mix_sub", 0.5f },
            { "flt_cutoff", 1200.0f }, { "flt_env_amt", 0.2f }, { "fenv_d", 0.15f }, { "fenv_s", 0.0f },
            { "aenv_a", 0.001f }, { "aenv_d", 0.35f }, { "aenv_s", 0.0f }, { "aenv_r", 0.1f }, { "amp_level", -1.1f } }, "Bass" },

        { "Midnight Reese", {
            { "voice_mode", 1.0f }, { "voice_count", 2.0f }, { "voice_detune", 0.9f }, { "mix_osc1", 0.9f }, { "mix_osc2", 0.7f }, { "osc2_fine", -14.0f },
            { "flt_cutoff", 420.0f }, { "flt_reso", 0.2f }, { "lfo2_wave", 0.0f }, { "lfo2_rate", 0.2f }, { "mm5_src", 9.0f }, { "mm5_dst", 4.0f }, { "mm5_amt", 0.15f },
            { "aenv_a", 0.005f }, { "aenv_s", 1.0f }, { "aenv_r", 0.2f }, { "mix_drive", 0.45f }, { "amp_level", -10.9f } }, "Bass" },

        { "Offbeat Sub Bass", {
            { "voice_count", 1.0f }, { "osc1_saw", 0.0f }, { "osc1_pulse", 1.0f }, { "mix_osc1", 0.5f }, { "mix_osc2", 0.0f }, { "mix_sub", 0.9f },
            { "flt_cutoff", 400.0f }, { "flt_env_amt", 0.35f }, { "fenv_a", 0.001f }, { "fenv_d", 0.1f }, { "fenv_s", 0.0f },
            { "aenv_a", 0.001f }, { "aenv_d", 0.2f }, { "aenv_s", 0.0f }, { "aenv_r", 0.08f }, { "amp_level", 0.4f } }, "Bass" },

        { "Growl Motion Bass", {
            { "voice_count", 1.0f }, { "mix_osc2", 0.7f }, { "osc2_fine", 8.0f }, { "flt_cutoff", 350.0f }, { "flt_reso", 0.5f },
            { "lfo2_sync", 1.0f }, { "lfo2_rate", 1.2f }, { "lfo2_wave", 3.0f }, { "lfo2_retrig", 1.0f },
            { "mm5_src", 9.0f }, { "mm5_dst", 4.0f }, { "mm5_amt", 0.35f }, { "mix_drive", 0.6f },
            { "aenv_a", 0.002f }, { "aenv_s", 1.0f }, { "aenv_r", 0.12f }, { "amp_level", -5.2f } }, "Bass" },

        { "Deep Tech Bass", {
            { "voice_count", 1.0f }, { "osc2_freq", -12.0f }, { "mix_osc2", 0.5f }, { "flt_cutoff", 200.0f }, { "flt_reso", 0.35f },
            { "flt_env_amt", 0.45f }, { "fenv_a", 0.001f }, { "fenv_d", 0.3f }, { "fenv_s", 0.1f }, { "flt_velocity", 0.5f },
            { "aenv_a", 0.001f }, { "aenv_d", 0.5f }, { "aenv_s", 0.5f }, { "aenv_r", 0.1f }, { "mix_sub", 0.3f }, { "amp_level", 3.0f } }, "Bass" },

        { "Driven Twelve Bass", {
            { "voice_count", 1.0f }, { "flt_slope", 1.0f }, { "flt_cutoff", 500.0f }, { "flt_reso", 0.4f }, { "flt_env_amt", 0.35f },
            { "fenv_d", 0.25f }, { "fenv_s", 0.2f }, { "mix_drive", 0.85f }, { "mix_osc2", 0.6f }, { "osc2_freq", 7.0f },
            { "aenv_a", 0.002f }, { "aenv_s", 1.0f }, { "aenv_r", 0.1f }, { "amp_level", -7.5f } }, "Bass" },

        { "Hollow Square Bass", {
            { "voice_count", 1.0f }, { "osc1_saw", 0.0f }, { "osc1_pulse", 1.0f }, { "osc1_pw", 28.0f }, { "osc2_saw", 0.0f }, { "osc2_pulse", 1.0f },
            { "osc2_freq", -12.0f }, { "mix_osc2", 0.6f }, { "mix_ring", 0.15f }, { "flt_cutoff", 650.0f }, { "flt_env_amt", 0.25f },
            { "fenv_d", 0.3f }, { "fenv_s", 0.3f }, { "aenv_a", 0.001f }, { "aenv_s", 0.9f }, { "aenv_r", 0.1f }, { "amp_level", -4.8f } }, "Bass" },

        { "Progressive Rumble", {
            { "voice_count", 1.0f }, { "mix_osc1", 0.6f }, { "mix_osc2", 0.0f }, { "mix_sub", 0.8f }, { "sub_oct", 1.0f }, { "mix_noise", 0.15f },
            { "flt_cutoff", 160.0f }, { "flt_reso", 0.3f }, { "hpf_cutoff", 35.0f },
            { "aenv_a", 0.01f }, { "aenv_d", 0.6f }, { "aenv_s", 0.3f }, { "aenv_r", 0.4f },
            { "reverb_on", 1.0f }, { "reverb_mix", 0.2f }, { "amp_level", 3.6f } }, "Bass" },

        // ================= LEAD =================
        { "Silver Mono Lead", {
            { "voice_count", 1.0f }, { "legato", 1.0f }, { "glide", 0.07f }, { "mix_osc2", 0.8f }, { "osc2_fine", 7.0f },
            { "flt_cutoff", 2000.0f }, { "flt_reso", 0.3f }, { "flt_env_amt", 0.25f }, { "fenv_d", 0.8f }, { "fenv_s", 0.5f },
            { "lfo_rate", 5.5f }, { "lfo_delay", 0.5f }, { "mm4_src", 3.0f }, { "mm4_dst", 0.0f }, { "mm4_amt", 0.08f },
            { "delay_on", 1.0f }, { "delay_sync", 1.0f }, { "delay_div", 6.0f }, { "delay_fb", 0.4f }, { "delay_mix", 0.25f },
            { "reverb_on", 1.0f }, { "reverb_mix", 0.25f }, { "amp_level", -0.1f } }, "Lead" },

        { "Bite Scream Lead", {
            { "flt_model", 4.0f }, { "voice_count", 1.0f }, { "legato", 1.0f }, { "glide", 0.04f }, { "osc1_sync", 1.0f }, { "osc1_freq", 7.0f },
            { "mix_osc2", 0.4f }, { "flt_cutoff", 1500.0f }, { "flt_reso", 0.88f }, { "flt_env_amt", 0.3f }, { "fenv_d", 0.6f }, { "fenv_s", 0.4f },
            { "mm4_src", 4.0f }, { "mm4_dst", 4.0f }, { "mm4_amt", 0.3f }, { "delay_on", 1.0f }, { "delay_mix", 0.2f }, { "amp_level", -4.0f } }, "Lead" },

        { "FM Glass Lead", {
            { "voice_count", 1.0f }, { "osc2_tri", 1.0f }, { "osc2_saw", 0.0f }, { "osc2_freq", 12.0f }, { "mix_osc2", 0.0f }, { "osc_xmod", 0.35f },
            { "osc1_saw", 0.0f }, { "osc1_pulse", 1.0f }, { "flt_cutoff", 6000.0f }, { "glide", 0.05f }, { "legato", 1.0f },
            { "mm4_src", 4.0f }, { "mm4_dst", 8.0f }, { "mm4_amt", 0.4f },
            { "delay_on", 1.0f }, { "delay_sync", 1.0f }, { "delay_div", 6.0f }, { "delay_pingpong", 1.0f }, { "delay_mix", 0.25f },
            { "reverb_on", 1.0f }, { "reverb_type", 1.0f }, { "reverb_mix", 0.3f }, { "amp_level", -4.9f } }, "Lead" },

        { "Ring Siren Lead", {
            { "voice_count", 1.0f }, { "osc2_freq", 5.0f }, { "osc2_fine", 20.0f }, { "mix_osc1", 0.4f }, { "mix_osc2", 0.2f }, { "mix_ring", 0.8f },
            { "flt_cutoff", 3500.0f }, { "flt_reso", 0.2f }, { "lfo_rate", 6.0f }, { "mm4_src", 3.0f }, { "mm4_dst", 1.0f }, { "mm4_amt", 0.12f },
            { "delay_on", 1.0f }, { "delay_mix", 0.2f }, { "amp_level", 3.8f } }, "Lead" },

        { "Duo Anthem Lead", {
            { "voice_mode", 1.0f }, { "voice_count", 4.0f }, { "voice_detune", 0.5f }, { "mix_osc2", 0.8f }, { "osc2_freq", 12.0f },
            { "flt_cutoff", 2600.0f }, { "flt_reso", 0.2f }, { "flt_env_amt", 0.2f }, { "fenv_d", 0.6f }, { "fenv_s", 0.6f },
            { "aenv_a", 0.01f }, { "aenv_s", 1.0f }, { "aenv_r", 0.5f },
            { "delay_on", 1.0f }, { "delay_sync", 1.0f }, { "delay_div", 6.0f }, { "delay_fb", 0.35f }, { "delay_mix", 0.25f },
            { "reverb_on", 1.0f }, { "reverb_mix", 0.3f }, { "amp_level", -6.0f } }, "Lead" },

        { "Soft Flute Lead", {
            { "voice_count", 1.0f }, { "legato", 1.0f }, { "glide", 0.05f }, { "osc1_saw", 0.0f }, { "osc1_pulse", 0.0f }, { "mix_osc1", 0.0f },
            { "osc2_saw", 0.0f }, { "osc2_tri", 1.0f }, { "mix_osc2", 1.0f }, { "mix_noise", 0.05f },
            { "menv_a", 0.08f }, { "menv_d", 0.3f }, { "menv_s", 0.2f }, { "mm5_src", 8.0f }, { "mm5_dst", 14.0f }, { "mm5_amt", 0.25f },
            { "flt_cutoff", 3000.0f }, { "aenv_a", 0.06f }, { "aenv_s", 1.0f }, { "aenv_r", 0.3f },
            { "lfo_rate", 4.8f }, { "lfo_delay", 0.8f }, { "mm4_src", 3.0f }, { "mm4_dst", 1.0f }, { "mm4_amt", 0.07f },
            { "reverb_on", 1.0f }, { "reverb_type", 1.0f }, { "reverb_mix", 0.3f }, { "amp_level", -3.6f } }, "Lead" },

        { "Phase Lead", {
            { "voice_count", 1.0f }, { "mix_osc2", 0.7f }, { "osc2_fine", -6.0f }, { "flt_cutoff", 2200.0f }, { "flt_reso", 0.3f },
            { "phaser_on", 1.0f }, { "phaser_rate", 0.5f }, { "phaser_depth", 0.8f }, { "phaser_fb", 0.6f }, { "phaser_mix", 0.5f },
            { "delay_on", 1.0f }, { "delay_sync", 1.0f }, { "delay_div", 5.0f }, { "delay_mix", 0.2f }, { "amp_level", 1.1f } }, "Lead" },

        { "Hollow Sync Lead", {
            { "voice_count", 1.0f }, { "osc1_sync", 1.0f }, { "osc1_freq", 5.0f }, { "mix_osc2", 0.0f },
            { "menv_a", 0.001f }, { "menv_d", 0.7f }, { "menv_s", 0.2f }, { "mm5_src", 8.0f }, { "mm5_dst", 0.0f }, { "mm5_amt", 0.45f },
            { "flt_cutoff", 3000.0f }, { "flt_reso", 0.2f }, { "glide", 0.04f }, { "legato", 1.0f },
            { "delay_on", 1.0f }, { "delay_sync", 1.0f }, { "delay_div", 6.0f }, { "delay_mix", 0.22f }, { "amp_level", 2.5f } }, "Lead" },

        { "Fuzz Solo", {
            { "voice_count", 1.0f }, { "legato", 1.0f }, { "glide", 0.06f }, { "mix_osc2", 0.6f }, { "osc2_fine", 5.0f },
            { "flt_cutoff", 2800.0f }, { "flt_reso", 0.2f }, { "lfo_rate", 5.2f }, { "lfo_delay", 0.5f }, { "mm4_src", 3.0f }, { "mm4_dst", 0.0f }, { "mm4_amt", 0.09f },
            { "fuzz_on", 1.0f }, { "fuzz_sustain", 0.9f }, { "fuzz_tone", 0.55f }, { "fuzz_volume", 0.6f }, { "fuzz_mix", 1.0f },
            { "delay_on", 1.0f }, { "delay_sync", 1.0f }, { "delay_div", 9.0f }, { "delay_fb", 0.35f }, { "delay_mix", 0.25f },
            { "reverb_on", 1.0f }, { "reverb_type", 1.0f }, { "reverb_mix", 0.25f }, { "amp_level", -0.8f } }, "Lead" },

        { "Portamento Dream Lead", {
            { "voice_count", 1.0f }, { "glide", 0.25f }, { "osc1_saw", 0.0f }, { "osc1_pulse", 1.0f }, { "osc1_pw", 40.0f }, { "mix_osc2", 0.5f },
            { "osc2_freq", 12.0f }, { "flt_cutoff", 1800.0f }, { "flt_reso", 0.25f }, { "aenv_a", 0.05f }, { "aenv_s", 1.0f }, { "aenv_r", 0.8f },
            { "chorus_on", 1.0f }, { "chorus_mode", 1.0f }, { "chorus_mix", 0.4f },
            { "reverb_on", 1.0f }, { "reverb_size", 0.9f }, { "reverb_decay", 6.0f }, { "reverb_mix", 0.4f }, { "amp_level", -4.5f } }, "Lead" },

        // ================= PAD =================
        { "Evening Choir Pad", {
            { "flt_model", 3.0f }, { "flt_mode", 1.0f }, { "flt_cutoff", 1100.0f }, { "flt_reso", 0.5f }, { "mix_osc2", 0.8f }, { "osc2_fine", 8.0f },
            { "mix_noise", 0.1f }, { "lfo2_wave", 6.0f }, { "lfo2_rate", 0.25f }, { "mm5_src", 9.0f }, { "mm5_dst", 4.0f }, { "mm5_amt", 0.1f },
            { "aenv_a", 1.2f }, { "aenv_s", 1.0f }, { "aenv_r", 3.0f }, { "chorus_on", 1.0f }, { "chorus_mode", 2.0f }, { "chorus_mix", 0.5f },
            { "reverb_on", 1.0f }, { "reverb_size", 0.9f }, { "reverb_decay", 7.0f }, { "reverb_mix", 0.45f }, { "amp_level", -13.6f } }, "Pad" },

        { "Slow Motion Strings", {
            { "mix_osc2", 0.8f }, { "osc2_fine", -10.0f }, { "flt_cutoff", 2400.0f }, { "flt_slope", 1.0f }, { "flt_reso", 0.1f },
            { "hpf_cutoff", 180.0f }, { "aenv_a", 1.8f }, { "aenv_s", 1.0f }, { "aenv_r", 3.5f },
            { "lfo_rate", 5.0f }, { "mm3_src", 3.0f }, { "mm3_dst", 0.0f }, { "mm3_amt", 0.04f },
            { "chorus_on", 1.0f }, { "chorus_mode", 3.0f }, { "chorus_mix", 0.5f },
            { "reverb_on", 1.0f }, { "reverb_size", 0.9f }, { "reverb_decay", 6.0f }, { "reverb_mix", 0.4f }, { "amp_level", -9.0f } }, "Pad" },

        { "Plate Glow Pad", {
            { "mix_osc2", 0.7f }, { "osc2_freq", 7.0f }, { "flt_cutoff", 900.0f }, { "flt_reso", 0.25f }, { "flt_env_amt", 0.3f },
            { "fenv_a", 1.0f }, { "fenv_d", 3.0f }, { "fenv_s", 0.4f }, { "aenv_a", 0.8f }, { "aenv_s", 1.0f }, { "aenv_r", 3.0f },
            { "reverb_on", 1.0f }, { "reverb_type", 1.0f }, { "reverb_size", 1.0f }, { "reverb_decay", 9.0f }, { "reverb_mix", 0.5f }, { "amp_level", -7.9f } }, "Pad" },

        { "Aurora Duo Pad", {
            { "voice_mode", 1.0f }, { "voice_count", 12.0f }, { "voice_detune", 0.55f }, { "mix_osc2", 0.7f }, { "osc2_freq", 12.0f },
            { "flt_cutoff", 1500.0f }, { "flt_reso", 0.15f }, { "aenv_a", 1.0f }, { "aenv_s", 1.0f }, { "aenv_r", 3.0f },
            { "lfo2_wave", 0.0f }, { "lfo2_rate", 0.1f }, { "mm5_src", 9.0f }, { "mm5_dst", 4.0f }, { "mm5_amt", 0.15f },
            { "reverb_on", 1.0f }, { "reverb_size", 1.0f }, { "reverb_decay", 8.0f }, { "reverb_mix", 0.45f }, { "amp_level", -11.3f } }, "Pad" },

        { "Phased Nebula Pad", {
            { "mix_osc2", 0.8f }, { "osc2_fine", 12.0f }, { "flt_cutoff", 2000.0f }, { "aenv_a", 1.5f }, { "aenv_s", 1.0f }, { "aenv_r", 4.0f },
            { "phaser_on", 1.0f }, { "phaser_rate", 0.06f }, { "phaser_depth", 1.0f }, { "phaser_fb", 0.55f }, { "phaser_mix", 0.5f },
            { "reverb_on", 1.0f }, { "reverb_size", 1.0f }, { "reverb_decay", 10.0f }, { "reverb_mix", 0.45f }, { "amp_level", -5.0f } }, "Pad" },

        { "Wind Glass Pad", {
            { "flt_model", 3.0f }, { "flt_mode", 2.0f }, { "flt_cutoff", 1500.0f }, { "flt_reso", 0.4f }, { "mix_osc1", 0.5f }, { "mix_osc2", 0.5f },
            { "mix_noise", 0.4f }, { "lfo2_wave", 6.0f }, { "lfo2_rate", 0.2f }, { "mm5_src", 9.0f }, { "mm5_dst", 4.0f }, { "mm5_amt", 0.3f },
            { "aenv_a", 2.0f }, { "aenv_s", 1.0f }, { "aenv_r", 4.0f },
            { "reverb_on", 1.0f }, { "reverb_size", 1.0f }, { "reverb_decay", 12.0f }, { "reverb_mix", 0.55f }, { "amp_level", -4.0f } }, "Pad" },

        { "Soft Machine Pad", {
            { "mix_osc2", 0.7f }, { "osc2_fine", 6.0f }, { "flt_cutoff", 700.0f }, { "flt_reso", 0.35f },
            { "lfo2_wave", 5.0f }, { "lfo2_sync", 1.0f }, { "lfo2_rate", 1.0f }, { "lfo2_retrig", 0.0f },
            { "mm5_src", 9.0f }, { "mm5_dst", 4.0f }, { "mm5_amt", 0.25f }, { "aenv_a", 0.5f }, { "aenv_s", 1.0f }, { "aenv_r", 2.5f },
            { "delay_on", 1.0f }, { "delay_sync", 1.0f }, { "delay_div", 6.0f }, { "delay_mix", 0.2f }, { "reverb_on", 1.0f }, { "reverb_mix", 0.35f }, { "amp_level", -5.9f } }, "Pad" },

        { "Distant Shore Pad", {
            { "osc1_saw", 0.0f }, { "osc1_pulse", 1.0f }, { "osc1_pw", 30.0f }, { "mix_osc2", 0.7f }, { "osc2_fine", -7.0f },
            { "lfo_rate", 0.25f }, { "mm3_src", 3.0f }, { "mm3_dst", 2.0f }, { "mm3_amt", 0.35f }, { "flt_cutoff", 1300.0f },
            { "aenv_a", 1.5f }, { "aenv_s", 1.0f }, { "aenv_r", 4.0f }, { "chorus_on", 1.0f }, { "chorus_mode", 1.0f }, { "chorus_mix", 0.5f },
            { "reverb_on", 1.0f }, { "reverb_size", 1.0f }, { "reverb_decay", 9.0f }, { "reverb_mix", 0.5f }, { "amp_level", -8.1f } }, "Pad" },

        { "Frozen Lake Pad", {
            { "osc2_tri", 1.0f }, { "osc2_saw", 0.0f }, { "osc2_freq", 12.0f }, { "mix_osc2", 0.4f }, { "osc_xmod", 0.12f },
            { "flt_cutoff", 3000.0f }, { "flt_slope", 1.0f }, { "aenv_a", 1.0f }, { "aenv_s", 1.0f }, { "aenv_r", 4.0f },
            { "reverb_on", 1.0f }, { "reverb_type", 1.0f }, { "reverb_size", 1.0f }, { "reverb_decay", 10.0f }, { "reverb_mix", 0.5f }, { "amp_level", -9.7f } }, "Pad" },

        { "Blue Hour Pad", {
            { "mix_osc2", 0.8f }, { "osc2_fine", 9.0f }, { "flt_cutoff", 600.0f }, { "flt_reso", 0.3f }, { "flt_env_amt", 0.4f },
            { "fenv_a", 2.0f }, { "fenv_d", 4.0f }, { "fenv_s", 0.5f }, { "fenv_r", 3.0f }, { "aenv_a", 1.0f }, { "aenv_s", 1.0f }, { "aenv_r", 3.5f },
            { "mm5_src", 4.0f }, { "mm5_dst", 4.0f }, { "mm5_amt", 0.4f }, { "voice_spread", 0.9f },
            { "reverb_on", 1.0f }, { "reverb_size", 0.9f }, { "reverb_decay", 8.0f }, { "reverb_mix", 0.45f }, { "amp_level", -6.0f } }, "Pad" },

        { "Deep Canvas Pad", {
            { "mix_osc2", 0.8f }, { "osc2_freq", -12.0f }, { "mix_sub", 0.3f }, { "flt_cutoff", 500.0f }, { "flt_reso", 0.2f },
            { "aenv_a", 1.5f }, { "aenv_s", 1.0f }, { "aenv_r", 4.0f }, { "lfo2_wave", 6.0f }, { "lfo2_rate", 0.08f },
            { "mm5_src", 9.0f }, { "mm5_dst", 4.0f }, { "mm5_amt", 0.2f }, { "chorus_on", 1.0f }, { "chorus_mode", 2.0f }, { "chorus_mix", 0.45f },
            { "reverb_on", 1.0f }, { "reverb_mix", 0.4f }, { "amp_level", -5.9f } }, "Pad" },

        { "Weightless Pad", {
            { "flt_model", 2.0f }, { "mix_osc2", 0.8f }, { "osc2_freq", 7.0f }, { "osc2_fine", 5.0f }, { "flt_cutoff", 2500.0f },
            { "hpf_cutoff", 300.0f }, { "aenv_a", 2.5f }, { "aenv_s", 1.0f }, { "aenv_r", 5.0f },
            { "delay_on", 1.0f }, { "delay_sync", 1.0f }, { "delay_div", 9.0f }, { "delay_pingpong", 1.0f }, { "delay_fb", 0.5f }, { "delay_mix", 0.3f },
            { "reverb_on", 1.0f }, { "reverb_size", 1.0f }, { "reverb_decay", 14.0f }, { "reverb_mix", 0.55f }, { "amp_level", -4.4f } }, "Pad" },

        // ================= PLUCK =================
        { "Crystal Rain Pluck", {
            { "osc2_tri", 1.0f }, { "osc2_saw", 0.0f }, { "osc2_freq", 24.0f }, { "mix_osc2", 0.0f },
            { "menv_a", 0.001f }, { "menv_d", 0.3f }, { "menv_s", 0.0f }, { "mm5_src", 8.0f }, { "mm5_dst", 8.0f }, { "mm5_amt", 0.35f },
            { "osc1_saw", 0.0f }, { "osc1_pulse", 1.0f }, { "flt_cutoff", 4000.0f },
            { "aenv_a", 0.001f }, { "aenv_d", 1.2f }, { "aenv_s", 0.0f }, { "aenv_r", 1.0f },
            { "delay_on", 1.0f }, { "delay_sync", 1.0f }, { "delay_div", 6.0f }, { "delay_pingpong", 1.0f }, { "delay_fb", 0.45f }, { "delay_mix", 0.3f },
            { "reverb_on", 1.0f }, { "reverb_type", 1.0f }, { "reverb_mix", 0.35f }, { "amp_level", -5.1f } }, "Pluck" },

        { "Cascade Pluck", {
            { "flt_model", 2.0f }, { "mix_osc2", 0.7f }, { "osc2_fine", 6.0f }, { "flt_cutoff", 280.0f }, { "flt_reso", 0.4f },
            { "flt_env_amt", 0.6f }, { "flt_velocity", 0.4f }, { "fenv_a", 0.001f }, { "fenv_d", 0.3f }, { "fenv_s", 0.0f },
            { "aenv_a", 0.001f }, { "aenv_d", 0.9f }, { "aenv_s", 0.0f }, { "aenv_r", 0.6f },
            { "delay_on", 1.0f }, { "delay_sync", 1.0f }, { "delay_div", 6.0f }, { "delay_mix", 0.25f }, { "reverb_on", 1.0f }, { "reverb_mix", 0.3f }, { "amp_level", -1.9f } }, "Pluck" },

        { "Bite Pluck", {
            { "flt_model", 4.0f }, { "mix_osc2", 0.5f }, { "flt_cutoff", 400.0f }, { "flt_reso", 0.6f }, { "flt_env_amt", 0.55f },
            { "fenv_a", 0.001f }, { "fenv_d", 0.2f }, { "fenv_s", 0.0f }, { "aenv_a", 0.001f }, { "aenv_d", 0.5f }, { "aenv_s", 0.0f }, { "aenv_r", 0.3f },
            { "delay_on", 1.0f }, { "delay_sync", 1.0f }, { "delay_div", 5.0f }, { "delay_mix", 0.2f }, { "amp_level", -5.7f } }, "Pluck" },

        { "Pingpong Dew Pluck", {
            { "osc1_saw", 0.0f }, { "osc1_pulse", 1.0f }, { "osc1_pw", 30.0f }, { "mix_osc2", 0.5f }, { "osc2_freq", 12.0f },
            { "flt_cutoff", 600.0f }, { "flt_env_amt", 0.5f }, { "fenv_a", 0.001f }, { "fenv_d", 0.25f }, { "fenv_s", 0.0f },
            { "aenv_a", 0.001f }, { "aenv_d", 0.6f }, { "aenv_s", 0.0f }, { "aenv_r", 0.5f },
            { "delay_on", 1.0f }, { "delay_sync", 1.0f }, { "delay_div", 6.0f }, { "delay_pingpong", 1.0f }, { "delay_fb", 0.55f }, { "delay_mix", 0.35f },
            { "reverb_on", 1.0f }, { "reverb_mix", 0.25f }, { "amp_level", -3.4f } }, "Pluck" },

        { "Glass Harp Pluck", {
            { "osc2_freq", 7.0f }, { "osc2_fine", 3.0f }, { "mix_osc1", 0.5f }, { "mix_osc2", 0.3f }, { "mix_ring", 0.5f },
            { "flt_cutoff", 3000.0f }, { "flt_env_amt", 0.3f }, { "fenv_d", 0.4f }, { "fenv_s", 0.0f },
            { "aenv_a", 0.001f }, { "aenv_d", 1.5f }, { "aenv_s", 0.0f }, { "aenv_r", 1.2f },
            { "reverb_on", 1.0f }, { "reverb_type", 1.0f }, { "reverb_size", 0.8f }, { "reverb_mix", 0.4f }, { "amp_level", 0.8f } }, "Pluck" },

        { "Warm Wood Pluck", {
            { "osc1_saw", 0.0f }, { "osc1_pulse", 1.0f }, { "osc2_saw", 0.0f }, { "osc2_tri", 1.0f }, { "mix_osc2", 0.8f },
            { "flt_slope", 1.0f }, { "flt_cutoff", 500.0f }, { "flt_env_amt", 0.45f }, { "fenv_d", 0.3f }, { "fenv_s", 0.0f },
            { "aenv_a", 0.001f }, { "aenv_d", 0.8f }, { "aenv_s", 0.0f }, { "aenv_r", 0.5f }, { "reverb_on", 1.0f }, { "reverb_mix", 0.25f }, { "amp_level", -5.5f } }, "Pluck" },

        { "Random Glint Pluck", {
            { "mix_osc2", 0.6f }, { "osc2_fine", 7.0f }, { "flt_cutoff", 500.0f }, { "flt_reso", 0.35f }, { "flt_env_amt", 0.5f },
            { "fenv_d", 0.3f }, { "fenv_s", 0.0f }, { "mm5_src", 11.0f }, { "mm5_dst", 4.0f }, { "mm5_amt", 0.3f },
            { "mm6_src", 11.0f }, { "mm6_dst", 2.0f }, { "mm6_amt", 0.3f },
            { "aenv_a", 0.001f }, { "aenv_d", 0.7f }, { "aenv_s", 0.0f }, { "aenv_r", 0.5f },
            { "delay_on", 1.0f }, { "delay_sync", 1.0f }, { "delay_div", 6.0f }, { "delay_mix", 0.25f }, { "reverb_on", 1.0f }, { "reverb_mix", 0.3f }, { "amp_level", 1.2f } }, "Pluck" },

        { "Sub Pluck", {
            { "mix_osc2", 0.4f }, { "mix_sub", 0.6f }, { "flt_cutoff", 350.0f }, { "flt_env_amt", 0.55f }, { "fenv_d", 0.2f }, { "fenv_s", 0.0f },
            { "aenv_a", 0.001f }, { "aenv_d", 0.5f }, { "aenv_s", 0.0f }, { "aenv_r", 0.3f }, { "amp_level", -4.7f } }, "Pluck" },

        { "Phase Pluck", {
            { "mix_osc2", 0.7f }, { "osc2_fine", 5.0f }, { "flt_cutoff", 450.0f }, { "flt_env_amt", 0.55f }, { "fenv_d", 0.3f }, { "fenv_s", 0.0f },
            { "aenv_a", 0.001f }, { "aenv_d", 0.9f }, { "aenv_s", 0.0f }, { "aenv_r", 0.6f },
            { "phaser_on", 1.0f }, { "phaser_rate", 0.2f }, { "phaser_fb", 0.5f }, { "phaser_mix", 0.5f },
            { "delay_on", 1.0f }, { "delay_sync", 1.0f }, { "delay_div", 6.0f }, { "delay_mix", 0.25f }, { "amp_level", -2.9f } }, "Pluck" },

        { "Duo Silk Pluck", {
            { "voice_mode", 1.0f }, { "voice_count", 10.0f }, { "voice_detune", 0.4f }, { "mix_osc2", 0.6f }, { "flt_cutoff", 400.0f },
            { "flt_env_amt", 0.55f }, { "flt_velocity", 0.4f }, { "fenv_d", 0.35f }, { "fenv_s", 0.0f },
            { "aenv_a", 0.001f }, { "aenv_d", 1.0f }, { "aenv_s", 0.0f }, { "aenv_r", 0.7f },
            { "reverb_on", 1.0f }, { "reverb_type", 1.0f }, { "reverb_mix", 0.35f }, { "amp_level", -4.8f } }, "Pluck" },

        { "Mallet Glow Pluck", {
            { "osc2_tri", 1.0f }, { "osc2_saw", 0.0f }, { "osc2_freq", 12.0f }, { "osc2_fine", 3.0f }, { "mix_osc2", 0.3f },
            { "osc1_saw", 0.0f }, { "osc1_pulse", 1.0f }, { "mix_osc1", 0.6f },
            { "menv_a", 0.001f }, { "menv_d", 0.15f }, { "menv_s", 0.0f }, { "mm5_src", 8.0f }, { "mm5_dst", 8.0f }, { "mm5_amt", 0.3f },
            { "flt_cutoff", 2500.0f }, { "aenv_a", 0.001f }, { "aenv_d", 1.0f }, { "aenv_s", 0.0f }, { "aenv_r", 0.8f },
            { "reverb_on", 1.0f }, { "reverb_mix", 0.3f }, { "amp_level", -3.1f } }, "Pluck" },

        { "Hollow Pipe Pluck", {
            { "flt_model", 3.0f }, { "flt_mode", 1.0f }, { "flt_cutoff", 900.0f }, { "flt_reso", 0.42f }, { "flt_env_amt", 0.35f },
            { "fenv_d", 0.3f }, { "fenv_s", 0.0f }, { "mix_osc2", 0.6f }, { "aenv_a", 0.001f }, { "aenv_d", 0.7f }, { "aenv_s", 0.0f }, { "aenv_r", 0.5f },
            { "delay_on", 1.0f }, { "delay_sync", 1.0f }, { "delay_div", 6.0f }, { "delay_mix", 0.25f }, { "amp_level", -6.8f } }, "Pluck" },

        // ================= KEYS =================
        { "Tine Piano FM", {
            { "osc2_tri", 1.0f }, { "osc2_saw", 0.0f }, { "osc2_freq", 24.0f }, { "mix_osc2", 0.0f },
            { "osc1_saw", 0.0f }, { "osc1_pulse", 1.0f }, { "osc1_pw", 50.0f },
            { "menv_a", 0.001f }, { "menv_d", 0.6f }, { "menv_s", 0.05f }, { "mm5_src", 8.0f }, { "mm5_dst", 8.0f }, { "mm5_amt", 0.3f },
            { "mm6_src", 5.0f }, { "mm6_dst", 8.0f }, { "mm6_amt", 0.2f }, { "flt_cutoff", 3500.0f },
            { "aenv_a", 0.001f }, { "aenv_d", 2.5f }, { "aenv_s", 0.2f }, { "aenv_r", 0.8f },
            { "lfo2_wave", 0.0f }, { "lfo2_rate", 4.5f }, { "mm7_src", 9.0f }, { "mm7_dst", 6.0f }, { "mm7_amt", -0.15f },
            { "chorus_on", 1.0f }, { "chorus_mode", 1.0f }, { "chorus_mix", 0.3f }, { "reverb_on", 1.0f }, { "reverb_mix", 0.2f }, { "amp_level", -9.5f } }, "Keys" },

        { "Warm Clav Bite", {
            { "flt_model", 4.0f }, { "osc1_saw", 0.0f }, { "osc1_pulse", 1.0f }, { "osc1_pw", 18.0f }, { "mix_osc2", 0.0f },
            { "flt_cutoff", 700.0f }, { "flt_reso", 0.45f }, { "flt_env_amt", 0.4f }, { "fenv_d", 0.18f }, { "fenv_s", 0.1f },
            { "aenv_a", 0.001f }, { "aenv_d", 0.5f }, { "aenv_s", 0.3f }, { "aenv_r", 0.1f }, { "phaser_on", 1.0f }, { "phaser_mix", 0.3f }, { "amp_level", -5.3f } }, "Keys" },

        { "Ring Bell Keys", {
            { "osc2_freq", 17.0f }, { "osc2_tri", 1.0f }, { "osc2_saw", 0.0f }, { "mix_osc1", 0.3f }, { "mix_osc2", 0.2f }, { "mix_ring", 0.7f },
            { "flt_cutoff", 5000.0f }, { "aenv_a", 0.001f }, { "aenv_d", 2.0f }, { "aenv_s", 0.0f }, { "aenv_r", 1.5f },
            { "reverb_on", 1.0f }, { "reverb_type", 1.0f }, { "reverb_mix", 0.3f }, { "amp_level", -0.8f } }, "Keys" },

        { "Soft Organ Sub", {
            { "osc1_saw", 0.0f }, { "osc1_pulse", 1.0f }, { "osc2_saw", 0.0f }, { "osc2_pulse", 1.0f }, { "osc2_freq", 12.0f }, { "mix_osc2", 0.5f },
            { "mix_sub", 0.5f }, { "flt_cutoff", 2500.0f }, { "flt_slope", 1.0f }, { "aenv_a", 0.005f }, { "aenv_s", 1.0f }, { "aenv_r", 0.1f },
            { "chorus_on", 1.0f }, { "chorus_mode", 3.0f }, { "chorus_mix", 0.4f }, { "amp_level", -12.5f } }, "Keys" },

        { "Vibes Tremolo", {
            { "osc1_saw", 0.0f }, { "osc1_pulse", 0.0f }, { "mix_osc1", 0.0f }, { "osc2_saw", 0.0f }, { "osc2_tri", 1.0f }, { "mix_osc2", 1.0f },
            { "osc_xmod", 0.05f }, { "flt_cutoff", 5000.0f }, { "aenv_a", 0.001f }, { "aenv_d", 2.5f }, { "aenv_s", 0.0f }, { "aenv_r", 1.2f },
            { "lfo2_wave", 0.0f }, { "lfo2_rate", 5.5f }, { "lfo2_retrig", 0.0f }, { "mm5_src", 9.0f }, { "mm5_dst", 6.0f }, { "mm5_amt", -0.3f },
            { "reverb_on", 1.0f }, { "reverb_type", 1.0f }, { "reverb_mix", 0.3f }, { "amp_level", -8.1f } }, "Keys" },

        { "Dusty Electric Keys", {
            { "osc1_saw", 0.0f }, { "osc1_pulse", 1.0f }, { "osc1_pw", 45.0f }, { "mix_osc2", 0.4f }, { "osc2_freq", 12.0f },
            { "flt_cutoff", 1200.0f }, { "flt_env_amt", 0.35f }, { "flt_velocity", 0.5f }, { "fenv_d", 0.6f }, { "fenv_s", 0.2f },
            { "aenv_a", 0.001f }, { "aenv_d", 1.8f }, { "aenv_s", 0.3f }, { "aenv_r", 0.6f }, { "analog_age", 0.8f },
            { "fuzz_on", 1.0f }, { "fuzz_sustain", 0.2f }, { "fuzz_tone", 0.35f }, { "fuzz_volume", 0.65f }, { "fuzz_mix", 0.3f },
            { "chorus_on", 1.0f }, { "chorus_mode", 2.0f }, { "chorus_mix", 0.4f }, { "amp_level", -6.6f } }, "Keys" },

        { "Plate Rhodes", {
            { "osc2_tri", 1.0f }, { "osc2_saw", 0.0f }, { "osc2_freq", 12.0f }, { "mix_osc2", 0.5f }, { "osc1_saw", 0.0f }, { "osc1_pulse", 1.0f },
            { "osc_xmod", 0.08f }, { "flt_cutoff", 1800.0f }, { "flt_env_amt", 0.3f }, { "flt_velocity", 0.4f }, { "fenv_d", 0.8f }, { "fenv_s", 0.2f },
            { "aenv_a", 0.001f }, { "aenv_d", 2.2f }, { "aenv_s", 0.25f }, { "aenv_r", 0.7f },
            { "reverb_on", 1.0f }, { "reverb_type", 1.0f }, { "reverb_decay", 3.0f }, { "reverb_mix", 0.3f }, { "amp_level", -9.0f } }, "Keys" },

        // ================= STAB =================
        { "Deep House Chord", {
            { "flt_slope", 1.0f }, { "mix_osc2", 0.7f }, { "osc2_fine", 7.0f }, { "flt_cutoff", 900.0f }, { "flt_reso", 0.2f },
            { "flt_env_amt", 0.3f }, { "fenv_d", 0.25f }, { "fenv_s", 0.1f }, { "aenv_a", 0.002f }, { "aenv_d", 0.4f }, { "aenv_s", 0.0f }, { "aenv_r", 0.3f },
            { "reverb_on", 1.0f }, { "reverb_type", 1.0f }, { "reverb_decay", 2.0f }, { "reverb_mix", 0.3f }, { "amp_level", -3.9f } }, "Stab" },

        { "Techno Chord Stab", {
            { "mix_osc2", 0.8f }, { "osc2_fine", 10.0f }, { "flt_cutoff", 1200.0f }, { "flt_reso", 0.3f }, { "flt_env_amt", 0.4f },
            { "fenv_d", 0.2f }, { "fenv_s", 0.0f }, { "aenv_a", 0.001f }, { "aenv_d", 0.3f }, { "aenv_s", 0.0f }, { "aenv_r", 0.2f },
            { "delay_on", 1.0f }, { "delay_sync", 1.0f }, { "delay_div", 6.0f }, { "delay_pingpong", 1.0f }, { "delay_fb", 0.6f }, { "delay_mix", 0.35f },
            { "reverb_on", 1.0f }, { "reverb_mix", 0.25f }, { "amp_level", -0.9f } }, "Stab" },

        { "Fuzz Rave Stab", {
            { "mix_osc2", 0.8f }, { "osc2_freq", 12.0f }, { "osc2_fine", 8.0f }, { "flt_cutoff", 2000.0f }, { "flt_env_amt", 0.3f },
            { "fenv_d", 0.3f }, { "fenv_s", 0.2f }, { "aenv_a", 0.001f }, { "aenv_d", 0.5f }, { "aenv_s", 0.0f }, { "aenv_r", 0.3f },
            { "fuzz_on", 1.0f }, { "fuzz_sustain", 0.8f }, { "fuzz_tone", 0.6f }, { "fuzz_volume", 0.6f }, { "fuzz_mix", 0.7f },
            { "reverb_on", 1.0f }, { "reverb_mix", 0.25f }, { "amp_level", -0.4f } }, "Stab" },

        { "Bite Chord Stab", {
            { "flt_model", 4.0f }, { "mix_osc2", 0.6f }, { "flt_cutoff", 800.0f }, { "flt_reso", 0.55f }, { "flt_env_amt", 0.45f },
            { "fenv_d", 0.15f }, { "fenv_s", 0.0f }, { "aenv_a", 0.001f }, { "aenv_d", 0.25f }, { "aenv_s", 0.0f }, { "aenv_r", 0.2f },
            { "delay_on", 1.0f }, { "delay_sync", 1.0f }, { "delay_div", 5.0f }, { "delay_mix", 0.25f }, { "amp_level", -6.7f } }, "Stab" },

        { "Phased Stab", {
            { "mix_osc2", 0.7f }, { "osc2_freq", 7.0f }, { "flt_cutoff", 1500.0f }, { "flt_env_amt", 0.3f }, { "fenv_d", 0.3f }, { "fenv_s", 0.0f },
            { "aenv_a", 0.001f }, { "aenv_d", 0.4f }, { "aenv_s", 0.0f }, { "aenv_r", 0.3f },
            { "phaser_on", 1.0f }, { "phaser_rate", 0.35f }, { "phaser_fb", 0.6f }, { "phaser_mix", 0.5f },
            { "reverb_on", 1.0f }, { "reverb_mix", 0.25f }, { "amp_level", -1.1f } }, "Stab" },

        { "Duo Brass Stab", {
            { "voice_mode", 1.0f }, { "voice_count", 10.0f }, { "voice_detune", 0.35f }, { "mix_osc2", 0.8f }, { "flt_cutoff", 600.0f },
            { "flt_env_amt", 0.55f }, { "fenv_a", 0.03f }, { "fenv_d", 0.3f }, { "fenv_s", 0.3f },
            { "aenv_a", 0.01f }, { "aenv_d", 0.4f }, { "aenv_s", 0.5f }, { "aenv_r", 0.2f }, { "amp_level", -10.0f } }, "Stab" },

        { "Hollow Dub Stab", {
            { "flt_model", 3.0f }, { "flt_mode", 1.0f }, { "flt_cutoff", 700.0f }, { "flt_reso", 0.55f }, { "mix_osc2", 0.7f }, { "osc2_freq", 7.0f },
            { "aenv_a", 0.001f }, { "aenv_d", 0.3f }, { "aenv_s", 0.0f }, { "aenv_r", 0.2f },
            { "delay_on", 1.0f }, { "delay_sync", 1.0f }, { "delay_div", 6.0f }, { "delay_fb", 0.65f }, { "delay_mix", 0.4f },
            { "reverb_on", 1.0f }, { "reverb_type", 1.0f }, { "reverb_mix", 0.3f }, { "amp_level", -9.3f } }, "Stab" },

        // ================= ARP (built-in arpeggiator) =================
        { "Up Pulse Sequence", {
            { "arp_on", 1.0f }, { "arp_mode", 0.0f }, { "arp_rate", 5.0f }, { "arp_oct", 2.0f }, { "arp_gate", 0.5f },
            { "osc1_saw", 0.0f }, { "osc1_pulse", 1.0f }, { "osc1_pw", 35.0f }, { "mix_osc2", 0.4f }, { "flt_cutoff", 700.0f },
            { "flt_env_amt", 0.4f }, { "fenv_d", 0.2f }, { "fenv_s", 0.1f }, { "aenv_a", 0.001f }, { "aenv_d", 0.25f }, { "aenv_s", 0.2f }, { "aenv_r", 0.15f },
            { "delay_on", 1.0f }, { "delay_sync", 1.0f }, { "delay_div", 6.0f }, { "delay_mix", 0.25f }, { "amp_level", -1.5f } }, "Arp" },

        { "Downward Spiral Arp", {
            { "arp_on", 1.0f }, { "arp_mode", 1.0f }, { "arp_rate", 5.0f }, { "arp_oct", 3.0f }, { "arp_gate", 0.35f },
            { "mix_osc2", 0.6f }, { "osc2_fine", 6.0f }, { "flt_cutoff", 900.0f }, { "flt_reso", 0.35f }, { "flt_env_amt", 0.4f },
            { "fenv_d", 0.18f }, { "fenv_s", 0.0f }, { "aenv_a", 0.001f }, { "aenv_d", 0.3f }, { "aenv_s", 0.0f }, { "aenv_r", 0.2f },
            { "reverb_on", 1.0f }, { "reverb_type", 1.0f }, { "reverb_mix", 0.3f }, { "amp_level", 4.9f } }, "Arp" },

        { "Random Glitter Arp", {
            { "arp_on", 1.0f }, { "arp_mode", 3.0f }, { "arp_rate", 7.0f }, { "arp_oct", 2.0f }, { "arp_gate", 0.4f },
            { "osc2_tri", 1.0f }, { "osc2_saw", 0.0f }, { "osc2_freq", 24.0f }, { "mix_osc2", 0.0f }, { "osc_xmod", 0.2f },
            { "osc1_saw", 0.0f }, { "osc1_pulse", 1.0f }, { "flt_cutoff", 5000.0f },
            { "aenv_a", 0.001f }, { "aenv_d", 0.15f }, { "aenv_s", 0.0f }, { "aenv_r", 0.15f },
            { "delay_on", 1.0f }, { "delay_sync", 1.0f }, { "delay_div", 6.0f }, { "delay_pingpong", 1.0f }, { "delay_mix", 0.3f },
            { "reverb_on", 1.0f }, { "reverb_mix", 0.35f }, { "amp_level", 1.7f } }, "Arp" },

        { "Swing Pluck Arp", {
            { "arp_on", 1.0f }, { "arp_mode", 4.0f }, { "arp_rate", 5.0f }, { "arp_swing", 0.3f }, { "arp_gate", 0.4f },
            { "mix_osc2", 0.5f }, { "flt_cutoff", 450.0f }, { "flt_env_amt", 0.55f }, { "fenv_d", 0.2f }, { "fenv_s", 0.0f },
            { "aenv_a", 0.001f }, { "aenv_d", 0.3f }, { "aenv_s", 0.0f }, { "aenv_r", 0.2f },
            { "delay_on", 1.0f }, { "delay_sync", 1.0f }, { "delay_div", 6.0f }, { "delay_mix", 0.25f }, { "amp_level", 0.9f } }, "Arp" },

        { "Triplet Dream Arp", {
            { "arp_on", 1.0f }, { "arp_mode", 2.0f }, { "arp_rate", 6.0f }, { "arp_oct", 3.0f }, { "arp_gate", 0.6f },
            { "osc2_tri", 1.0f }, { "osc2_saw", 0.0f }, { "osc2_freq", 12.0f }, { "mix_osc2", 0.5f }, { "flt_cutoff", 1500.0f },
            { "aenv_a", 0.002f }, { "aenv_d", 0.3f }, { "aenv_s", 0.3f }, { "aenv_r", 0.3f },
            { "reverb_on", 1.0f }, { "reverb_type", 1.0f }, { "reverb_size", 0.9f }, { "reverb_decay", 6.0f }, { "reverb_mix", 0.4f }, { "amp_level", -0.4f } }, "Arp" },

        { "Octave Bass Arp", {
            { "arp_on", 1.0f }, { "arp_mode", 0.0f }, { "arp_rate", 2.0f }, { "arp_oct", 2.0f }, { "arp_gate", 0.5f }, { "voice_count", 1.0f },
            { "mix_osc2", 0.0f }, { "mix_sub", 0.5f }, { "flt_cutoff", 300.0f }, { "flt_reso", 0.3f }, { "flt_env_amt", 0.4f },
            { "fenv_d", 0.15f }, { "fenv_s", 0.1f }, { "aenv_a", 0.001f }, { "aenv_s", 0.8f }, { "aenv_r", 0.08f }, { "amp_level", -0.1f } }, "Arp" },

        { "Gated Pad Arp", {
            { "arp_on", 1.0f }, { "arp_mode", 4.0f }, { "arp_rate", 2.0f }, { "arp_gate", 0.9f },
            { "mix_osc2", 0.8f }, { "osc2_fine", 8.0f }, { "flt_cutoff", 1200.0f }, { "aenv_a", 0.01f }, { "aenv_s", 1.0f }, { "aenv_r", 0.2f },
            { "chorus_on", 1.0f }, { "chorus_mode", 2.0f }, { "chorus_mix", 0.4f }, { "reverb_on", 1.0f }, { "reverb_mix", 0.35f }, { "amp_level", -6.0f } }, "Arp" },

        { "Latch Hypnotic Arp", {
            { "arp_on", 1.0f }, { "arp_latch", 1.0f }, { "arp_mode", 4.0f }, { "arp_rate", 5.0f }, { "arp_oct", 1.0f }, { "arp_gate", 0.3f },
            { "flt_model", 2.0f }, { "mix_osc2", 0.5f }, { "flt_cutoff", 500.0f }, { "flt_reso", 0.45f }, { "flt_env_amt", 0.4f },
            { "fenv_d", 0.15f }, { "fenv_s", 0.0f }, { "aenv_a", 0.001f }, { "aenv_d", 0.2f }, { "aenv_s", 0.0f }, { "aenv_r", 0.15f },
            { "lfo2_wave", 0.0f }, { "lfo2_rate", 0.05f }, { "lfo2_retrig", 0.0f }, { "mm5_src", 9.0f }, { "mm5_dst", 4.0f }, { "mm5_amt", 0.3f },
            { "delay_on", 1.0f }, { "delay_sync", 1.0f }, { "delay_div", 6.0f }, { "delay_mix", 0.3f }, { "amp_level", 3.7f } }, "Arp" },

        { "Echo Cascade Arp", {
            { "arp_on", 1.0f }, { "arp_mode", 0.0f }, { "arp_rate", 2.0f }, { "arp_oct", 2.0f }, { "arp_gate", 0.3f },
            { "osc2_tri", 1.0f }, { "osc2_saw", 0.0f }, { "mix_osc2", 0.6f }, { "osc2_freq", 12.0f }, { "flt_cutoff", 1800.0f },
            { "aenv_a", 0.001f }, { "aenv_d", 0.4f }, { "aenv_s", 0.0f }, { "aenv_r", 0.3f },
            { "delay_on", 1.0f }, { "delay_sync", 1.0f }, { "delay_div", 6.0f }, { "delay_pingpong", 1.0f }, { "delay_fb", 0.65f }, { "delay_mix", 0.4f },
            { "reverb_on", 1.0f }, { "reverb_type", 1.0f }, { "reverb_mix", 0.35f }, { "amp_level", 1.2f } }, "Arp" },

        { "Fuzz Arp", {
            { "arp_on", 1.0f }, { "arp_mode", 2.0f }, { "arp_rate", 5.0f }, { "arp_oct", 2.0f }, { "arp_gate", 0.55f },
            { "mix_osc2", 0.5f }, { "flt_cutoff", 1800.0f }, { "flt_env_amt", 0.3f }, { "fenv_d", 0.2f }, { "fenv_s", 0.2f },
            { "aenv_a", 0.001f }, { "aenv_d", 0.3f }, { "aenv_s", 0.4f }, { "aenv_r", 0.2f },
            { "fuzz_on", 1.0f }, { "fuzz_sustain", 0.75f }, { "fuzz_tone", 0.5f }, { "fuzz_volume", 0.6f }, { "fuzz_mix", 0.6f },
            { "delay_on", 1.0f }, { "delay_sync", 1.0f }, { "delay_div", 6.0f }, { "delay_mix", 0.25f }, { "amp_level", 1.6f } }, "Arp" },

        // ================= DRUMS =================
        { "Ring Metal Perc", {
            { "osc1_freq", 24.0f }, { "osc2_freq", 17.0f }, { "osc2_fine", 30.0f }, { "mix_osc1", 0.5f }, { "mix_osc2", 0.5f }, { "mix_ring", 1.0f }, { "mix_drive", 0.6f },
            { "flt_cutoff", 900.0f },
            { "flt_mode", 2.0f }, { "flt_model", 3.0f }, { "flt_slope", 1.0f }, { "flt_keytrack", 0.0f },
            { "aenv_a", 0.001f }, { "aenv_d", 0.18f }, { "aenv_s", 0.0f }, { "aenv_r", 0.15f }, { "amp_level", -0.4f } }, "Drums" },

        { "FM Tom", {
            { "osc2_tri", 1.0f }, { "osc2_saw", 0.0f }, { "mix_osc2", 0.0f }, { "osc1_saw", 0.0f }, { "osc1_pulse", 1.0f }, { "mix_osc1", 0.8f },
            { "menv_a", 0.001f }, { "menv_d", 0.12f }, { "menv_s", 0.0f }, { "mm5_src", 8.0f }, { "mm5_dst", 0.0f }, { "mm5_amt", 0.6f },
            { "mm6_src", 8.0f }, { "mm6_dst", 8.0f }, { "mm6_amt", 0.3f }, { "flt_cutoff", 2000.0f },
            { "aenv_a", 0.001f }, { "aenv_d", 0.35f }, { "aenv_s", 0.0f }, { "aenv_r", 0.3f }, { "amp_level", 1.6f } }, "Drums" },

        { "Noise Shaker", {
            { "mix_osc1", 0.0f }, { "mix_osc2", 0.0f }, { "mix_noise", 1.0f }, { "flt_model", 3.0f }, { "flt_mode", 2.0f }, { "flt_cutoff", 6000.0f },
            { "flt_reso", 0.3f }, { "flt_keytrack", 0.0f }, { "aenv_a", 0.004f }, { "aenv_d", 0.07f }, { "aenv_s", 0.0f }, { "aenv_r", 0.06f }, { "amp_level", -1.3f } }, "Drums" },

        { "Sub Boom", {
            { "osc1_saw", 0.0f }, { "osc1_pulse", 0.0f }, { "mix_osc1", 0.0f }, { "osc2_saw", 0.0f }, { "osc2_tri", 1.0f }, { "mix_osc2", 1.0f },
            { "mix_sub", 0.6f }, { "menv_a", 0.001f }, { "menv_d", 0.08f }, { "menv_s", 0.0f }, { "mm5_src", 8.0f }, { "mm5_dst", 1.0f }, { "mm5_amt", 0.7f },
            { "flt_cutoff", 400.0f }, { "flt_keytrack", 0.0f }, { "aenv_a", 0.001f }, { "aenv_d", 0.8f }, { "aenv_s", 0.0f }, { "aenv_r", 0.5f }, { "amp_level", -0.3f } }, "Drums" },

        // ================= ATMOS & FX =================
        { "Fuzz Drone", {
            { "mix_osc2", 0.8f }, { "osc2_freq", 7.0f }, { "osc2_fine", 4.0f }, { "mix_sub", 0.4f }, { "flt_cutoff", 900.0f }, { "flt_reso", 0.3f },
            { "lfo2_wave", 6.0f }, { "lfo2_rate", 0.06f }, { "lfo2_retrig", 0.0f }, { "mm5_src", 9.0f }, { "mm5_dst", 4.0f }, { "mm5_amt", 0.35f },
            { "aenv_a", 3.0f }, { "aenv_s", 1.0f }, { "aenv_r", 6.0f },
            { "fuzz_on", 1.0f }, { "fuzz_sustain", 0.9f }, { "fuzz_tone", 0.4f }, { "fuzz_volume", 0.55f }, { "fuzz_mix", 0.7f },
            { "reverb_on", 1.0f }, { "reverb_size", 1.0f }, { "reverb_decay", 15.0f }, { "reverb_mix", 0.55f }, { "amp_level", 2.5f } }, "Atmos & FX" },

        { "Sample Hold Computer", {
            { "mix_osc2", 0.0f }, { "osc1_saw", 0.0f }, { "osc1_pulse", 1.0f }, { "flt_cutoff", 2500.0f }, { "flt_reso", 0.5f },
            { "lfo2_wave", 5.0f }, { "lfo2_sync", 1.0f }, { "lfo2_rate", 3.0f }, { "lfo2_retrig", 0.0f },
            { "mm5_src", 9.0f }, { "mm5_dst", 0.0f }, { "mm5_amt", 0.45f }, { "mm6_src", 9.0f }, { "mm6_dst", 4.0f }, { "mm6_amt", 0.3f },
            { "aenv_a", 0.01f }, { "aenv_s", 1.0f }, { "aenv_r", 0.5f },
            { "delay_on", 1.0f }, { "delay_sync", 1.0f }, { "delay_div", 6.0f }, { "delay_pingpong", 1.0f }, { "delay_mix", 0.35f }, { "amp_level", -3.4f } }, "Atmos & FX" },

        { "Wind Chimes FM", {
            { "osc2_tri", 1.0f }, { "osc2_saw", 0.0f }, { "osc2_freq", 19.0f }, { "osc2_fine", 12.0f }, { "mix_osc2", 0.0f }, { "osc_xmod", 0.25f },
            { "osc1_saw", 0.0f }, { "osc1_pulse", 1.0f }, { "mm5_src", 11.0f }, { "mm5_dst", 8.0f }, { "mm5_amt", 0.3f },
            { "flt_cutoff", 6000.0f }, { "aenv_a", 0.001f }, { "aenv_d", 2.5f }, { "aenv_s", 0.0f }, { "aenv_r", 2.5f },
            { "reverb_on", 1.0f }, { "reverb_type", 1.0f }, { "reverb_size", 1.0f }, { "reverb_decay", 10.0f }, { "reverb_mix", 0.55f }, { "amp_level", -5.1f } }, "Atmos & FX" },

        { "Ring Nebula", {
            { "osc2_freq", 3.0f }, { "osc2_fine", 15.0f }, { "mix_osc1", 0.5f }, { "mix_osc2", 0.5f }, { "mix_ring", 1.0f },
            { "lfo2_wave", 6.0f }, { "lfo2_rate", 0.1f }, { "mm5_src", 9.0f }, { "mm5_dst", 1.0f }, { "mm5_amt", 0.2f },
            { "flt_cutoff", 1500.0f }, { "flt_reso", 0.4f }, { "aenv_a", 2.0f }, { "aenv_s", 1.0f }, { "aenv_r", 5.0f },
            { "phaser_on", 1.0f }, { "phaser_rate", 0.05f }, { "phaser_mix", 0.4f },
            { "reverb_on", 1.0f }, { "reverb_size", 1.0f }, { "reverb_decay", 14.0f }, { "reverb_mix", 0.55f }, { "amp_level", 5.6f } }, "Atmos & FX" },

        { "Eight Bar Riser", {
            { "mix_osc2", 0.8f }, { "osc2_fine", 12.0f }, { "mix_noise", 0.4f }, { "flt_cutoff", 150.0f }, { "flt_reso", 0.4f },
            { "menv_a", 10.0f }, { "menv_d", 1.0f }, { "menv_s", 1.0f }, { "menv_r", 0.5f },
            { "mm5_src", 8.0f }, { "mm5_dst", 4.0f }, { "mm5_amt", 1.0f }, { "mm6_src", 8.0f }, { "mm6_dst", 0.0f }, { "mm6_amt", 0.3f },
            { "aenv_a", 4.0f }, { "aenv_s", 1.0f }, { "aenv_r", 2.0f },
            { "delay_on", 1.0f }, { "delay_sync", 1.0f }, { "delay_div", 6.0f }, { "delay_mix", 0.3f },
            { "reverb_on", 1.0f }, { "reverb_size", 1.0f }, { "reverb_decay", 8.0f }, { "reverb_mix", 0.45f }, { "amp_level", 1.8f } }, "Atmos & FX" },

        { "Phaser Wind", {
            { "mix_osc1", 0.0f }, { "mix_osc2", 0.0f }, { "mix_noise", 1.0f }, { "flt_cutoff", 1500.0f }, { "flt_reso", 0.5f },
            { "lfo2_wave", 6.0f }, { "lfo2_rate", 0.15f }, { "mm5_src", 9.0f }, { "mm5_dst", 4.0f }, { "mm5_amt", 0.4f },
            { "aenv_a", 2.0f }, { "aenv_s", 1.0f }, { "aenv_r", 4.0f },
            { "phaser_on", 1.0f }, { "phaser_rate", 0.1f }, { "phaser_depth", 1.0f }, { "phaser_fb", 0.7f }, { "phaser_mix", 0.5f },
            { "reverb_on", 1.0f }, { "reverb_mix", 0.4f }, { "amp_level", 3.8f } }, "Atmos & FX" },

        { "Alien Radio", {
            { "osc2_lofreq", 1.0f }, { "osc2_kbd", 0.0f }, { "mix_osc2", 0.0f }, { "osc_xmod", 0.5f }, { "mix_ring", 0.3f },
            { "lfo2_wave", 5.0f }, { "lfo2_rate", 6.0f }, { "mm5_src", 9.0f }, { "mm5_dst", 4.0f }, { "mm5_amt", 0.35f },
            { "flt_model", 3.0f }, { "flt_mode", 1.0f }, { "flt_cutoff", 1500.0f }, { "flt_reso", 0.6f },
            { "aenv_a", 0.1f }, { "aenv_s", 1.0f }, { "aenv_r", 1.0f },
            { "delay_on", 1.0f }, { "delay_sync", 1.0f }, { "delay_div", 4.0f }, { "delay_fb", 0.6f }, { "delay_mix", 0.35f }, { "amp_level", -13.8f } }, "Atmos & FX" },

        { "Deep Ocean Drone", {
            { "mix_osc2", 0.7f }, { "osc2_freq", -12.0f }, { "osc2_fine", 6.0f }, { "mix_sub", 0.5f }, { "sub_oct", 1.0f },
            { "flt_model", 3.0f }, { "flt_cutoff", 300.0f }, { "flt_reso", 0.4f },
            { "lfo2_wave", 0.0f }, { "lfo2_rate", 0.05f }, { "lfo2_retrig", 0.0f }, { "mm5_src", 9.0f }, { "mm5_dst", 4.0f }, { "mm5_amt", 0.3f },
            { "aenv_a", 4.0f }, { "aenv_s", 1.0f }, { "aenv_r", 6.0f },
            { "reverb_on", 1.0f }, { "reverb_size", 1.0f }, { "reverb_decay", 16.0f }, { "reverb_mix", 0.5f }, { "amp_level", -3.6f } }, "Atmos & FX" },

        // ================= TAPE ECHO & SPRING =================
        { "Dub Tape Chords", {
            { "mix_osc2", 0.7f }, { "osc2_fine", 7.0f }, { "flt_slope", 1.0f }, { "flt_cutoff", 900.0f }, { "flt_env_amt", 0.35f },
            { "fenv_d", 0.25f }, { "fenv_s", 0.0f }, { "aenv_a", 0.002f }, { "aenv_d", 0.35f }, { "aenv_s", 0.0f }, { "aenv_r", 0.25f },
            { "echo_on", 1.0f }, { "echo_mode", 8.0f }, { "echo_rate", 0.35f }, { "echo_intensity", 0.6f }, { "echo_bass", -0.3f },
            { "echo_treble", -0.2f }, { "echo_wow", 0.5f }, { "echo_volume", 0.6f }, { "echo_reverb", 0.4f }, { "amp_level", -3.7f } }, "Stab" },

        { "Runaway Tape Lead", {
            { "voice_count", 1.0f }, { "legato", 1.0f }, { "glide", 0.05f }, { "mix_osc2", 0.6f }, { "osc2_fine", 5.0f },
            { "flt_cutoff", 2200.0f }, { "flt_reso", 0.25f }, { "lfo_rate", 5.0f }, { "lfo_delay", 0.6f },
            { "mm4_src", 3.0f }, { "mm4_dst", 0.0f }, { "mm4_amt", 0.08f },
            { "echo_on", 1.0f }, { "echo_mode", 3.0f }, { "echo_rate", 0.55f }, { "echo_intensity", 0.8f }, { "echo_wow", 0.6f },
            { "echo_input", 0.65f }, { "echo_volume", 0.55f }, { "amp_level", -5.4f } }, "Lead" },

        { "Spring Tine Keys", {
            { "osc2_tri", 1.0f }, { "osc2_saw", 0.0f }, { "osc2_freq", 12.0f }, { "mix_osc2", 0.5f }, { "osc1_saw", 0.0f }, { "osc1_pulse", 1.0f },
            { "osc_xmod", 0.08f }, { "flt_cutoff", 2000.0f }, { "flt_env_amt", 0.3f }, { "flt_velocity", 0.4f }, { "fenv_d", 0.8f }, { "fenv_s", 0.2f },
            { "aenv_a", 0.001f }, { "aenv_d", 2.2f }, { "aenv_s", 0.25f }, { "aenv_r", 0.7f },
            { "reverb_on", 1.0f }, { "reverb_type", 2.0f }, { "reverb_size", 0.5f }, { "reverb_decay", 2.5f }, { "reverb_mix", 0.4f }, { "amp_level", -8.2f } }, "Keys" },

        { "Tape Loop Pad", {
            { "mix_osc2", 0.8f }, { "osc2_fine", 9.0f }, { "flt_cutoff", 1300.0f }, { "flt_reso", 0.2f },
            { "aenv_a", 1.2f }, { "aenv_s", 1.0f }, { "aenv_r", 3.0f }, { "analog_age", 0.7f },
            { "echo_on", 1.0f }, { "echo_mode", 10.0f }, { "echo_rate", 0.2f }, { "echo_intensity", 0.55f }, { "echo_treble", -0.4f },
            { "echo_wow", 0.8f }, { "echo_volume", 0.45f }, { "echo_reverb", 0.5f }, { "amp_level", -9.0f } }, "Pad" },

        { "Echo Chamber Pluck", {
            { "mix_osc2", 0.6f }, { "osc2_freq", 12.0f }, { "flt_cutoff", 400.0f }, { "flt_reso", 0.35f }, { "flt_env_amt", 0.55f },
            { "fenv_d", 0.25f }, { "fenv_s", 0.0f }, { "aenv_a", 0.001f }, { "aenv_d", 0.8f }, { "aenv_s", 0.0f }, { "aenv_r", 0.5f },
            { "echo_on", 1.0f }, { "echo_mode", 7.0f }, { "echo_rate", 0.6f }, { "echo_intensity", 0.5f }, { "echo_volume", 0.55f },
            { "echo_reverb", 0.45f }, { "amp_level", 2.8f } }, "Pluck" },

        { "Spring Drip Stab", {
            { "osc1_saw", 0.0f }, { "osc1_pulse", 1.0f }, { "osc1_pw", 30.0f }, { "mix_osc2", 0.5f }, { "osc2_freq", 7.0f },
            { "flt_cutoff", 1500.0f }, { "flt_env_amt", 0.3f }, { "fenv_d", 0.15f }, { "fenv_s", 0.0f },
            { "aenv_a", 0.001f }, { "aenv_d", 0.25f }, { "aenv_s", 0.0f }, { "aenv_r", 0.15f },
            { "echo_on", 1.0f }, { "echo_mode", 11.0f }, { "echo_reverb", 0.8f }, { "amp_level", -5.2f } }, "Stab" },

        // Big trumpeting brass lead: detuned saws in unison, a pitch scoop into every note (mod env -> both
        // oscillators), filter swell, legato slides, delayed vibrato. Play it mono with slides.
        { "Savanna Horn Lead", {
            { "unison", 1.0f }, { "voice_count", 4.0f }, { "voice_detune", 0.38f }, { "legato", 1.0f }, { "glide", 0.09f },
            { "mix_osc1", 0.9f }, { "mix_osc2", 0.8f }, { "osc2_fine", -9.0f }, { "mix_drive", 0.5f },
            { "flt_cutoff", 750.0f }, { "flt_reso", 0.25f }, { "flt_env_amt", 0.48f }, { "flt_keytrack", 1.0f },
            { "fenv_a", 0.07f }, { "fenv_d", 0.7f }, { "fenv_s", 0.45f }, { "fenv_r", 0.4f },
            { "aenv_a", 0.012f }, { "aenv_d", 0.6f }, { "aenv_s", 0.9f }, { "aenv_r", 0.35f },
            { "menv_a", 0.001f }, { "menv_d", 0.13f }, { "menv_s", 0.0f }, { "menv_r", 0.1f },
            { "mm5_src", 8.0f }, { "mm5_dst", 0.0f }, { "mm5_amt", -0.36f }, { "mm6_src", 8.0f }, { "mm6_dst", 1.0f }, { "mm6_amt", -0.36f },
            { "lfo_rate", 5.4f }, { "lfo_delay", 0.55f }, { "mm4_src", 3.0f }, { "mm4_dst", 0.0f }, { "mm4_amt", 0.08f },
            { "chorus_on", 1.0f }, { "chorus_mode", 1.0f }, { "chorus_mix", 0.3f },
            { "delay_on", 1.0f }, { "delay_sync", 1.0f }, { "delay_div", 6.0f }, { "delay_fb", 0.3f }, { "delay_mix", 0.15f },
            { "reverb_on", 1.0f }, { "reverb_type", 1.0f }, { "reverb_decay", 3.5f }, { "reverb_mix", 0.28f }, { "amp_level", -12.8f } }, "Lead" },
