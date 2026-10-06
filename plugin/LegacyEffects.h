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

} // namespace augur5
