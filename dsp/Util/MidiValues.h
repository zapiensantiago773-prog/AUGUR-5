#pragma once

#include <algorithm>
#include <cmath>

namespace augur
{

// MIDI pitch bend (14 bits, centre 8192) <-> -1..+1. The two halves are not the same size: 8192 steps down,
// 8191 up. Scaling each half on its own puts the wheel's top (16383) at exactly +1, so a full bend reaches the
// BEND UP range exactly (16383 / 8192 would stop 0.15 cents short of 12 semitones).
inline float pitchBendFromMidi (int value14) noexcept
{
    const int v = std::clamp (value14, 0, 16383) - 8192;
    return v >= 0 ? static_cast<float> (v) / 8191.0f : static_cast<float> (v) / 8192.0f;
}

inline int pitchBendToMidi (float bend) noexcept
{
    const float b = std::clamp (bend, -1.0f, 1.0f);
    return 8192 + static_cast<int> (std::lround (b >= 0.0f ? b * 8191.0f : b * 8192.0f));
}

} // namespace augur
