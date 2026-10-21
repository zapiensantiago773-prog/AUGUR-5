#pragma once

#include <array>
#include <cmath>

namespace augur
{

// 8th-order Butterworth low-pass (four biquads, transposed direct form II). In the voice it keeps the oscillators'
// ultrasonic content (24 kHz up to the internal Nyquist) out of the stages that saturate: what they would generate
// from it lands back in the audible band as aliasing (D-049). Off when the internal rate is too low to need it.
class Butterworth8
{
public:
    void prepare (double cutoffHz, double sampleRate) noexcept
    {
        enabled = cutoffHz < 0.45 * sampleRate;
        reset();
        if (! enabled)
            return;
        // Q of each pole pair: 1 / (2 cos ((2k - 1) pi / 16)).
        constexpr double qs[4] { 0.50979558, 0.60134489, 0.89997622, 2.56291545 };
        const double w0 = 2.0 * 3.14159265358979323846 * cutoffHz / sampleRate;
        const double cw = std::cos (w0), sw = std::sin (w0);
        for (size_t k = 0; k < 4; ++k)
        {
            const double alpha = sw / (2.0 * qs[k]);
            const double a0 = 1.0 + alpha;
            auto& s = sections[k];
            s.b0 = static_cast<float> ((1.0 - cw) * 0.5 / a0);
            s.b1 = static_cast<float> ((1.0 - cw) / a0);
            s.b2 = s.b0;
            s.a1 = static_cast<float> (-2.0 * cw / a0);
            s.a2 = static_cast<float> ((1.0 - alpha) / a0);
        }
    }

    void reset() noexcept
    {
        for (auto& s : sections)
            s.z1 = s.z2 = 0.0f;
    }

    float process (float x) noexcept
    {
        if (! enabled)
            return x;
        for (auto& s : sections)
        {
            const float y = s.b0 * x + s.z1;
            s.z1 = s.b1 * x - s.a1 * y + s.z2;
            s.z2 = s.b2 * x - s.a2 * y;
            x = y;
        }
        return x;
    }

    bool isEnabled() const noexcept { return enabled; }

private:
    struct Section
    {
        float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f, z1 = 0.0f, z2 = 0.0f;
    };
    std::array<Section, 4> sections {};
    bool enabled = false;
};

} // namespace augur
