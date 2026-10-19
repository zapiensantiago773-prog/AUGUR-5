#pragma once

#include <juce_core/juce_core.h>

#include <utility>
#include <vector>

namespace augur5
{

// AUGUR-5 1.0 had its own chorus, phaser, tape echo and reverb (parameter ids chorus_*, phaser_*, echo_*,
// reverb_*). 1.1 replaced them with the TONAL LAB effects rack (fx_* ids). This turns 1.0 settings into
// the rack's equivalents, so factory presets, user presets and sessions saved with 1.0 keep their effects.
// Settings that are not 1.0 effect ids pass through unchanged.
using Settings = std::vector<std::pair<juce::String, float>>;
void convertLegacyEffects (Settings& settings);

// True for a parameter id that only existed in 1.0 (converted by convertLegacyEffects).
bool isLegacyEffectId (const juce::String& id);

// Settings written before 1.3 lack what 1.3 added; this adds it with the values that keep their sound:
//  - pb_down = pb_range (the pitch wheel had one range for both directions);
//  - lfo_div / lfo2_div for a synced LFO, from its RATE knob as 1.2 read it (augur::legacyLfoDivisionFor).
// Settings that already have them pass through unchanged.
void completeOlderSettings (Settings& settings);

} // namespace augur5
