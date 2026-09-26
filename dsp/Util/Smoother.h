#pragma once

#include <algorithm>
#include <cmath>

namespace augur
{

// Linear ramp towards a target over a fixed duration in seconds.
// The ramp length is derived from the sample rate, so it sounds identical at any rate.
// Real-time safe: no allocation, no branching beyond the ramp counter.
class LinearSmoother
{
public:
    void reset (double sampleRate, double rampSeconds, float initialValue) noexcept
    {
        rampSamples = std::max (1, static_cast<int> (std::lround (sampleRate * rampSeconds)));
        snapTo (initialValue);
    }

    void setTarget (float newTarget) noexcept
    {
        if (newTarget == target)
            return;

        target = newTarget;
        remaining = rampSamples;
        step = (target - current) / static_cast<float> (rampSamples);
    }

    void snapTo (float value) noexcept
    {
        current = target = value;
        step = 0.0f;
        remaining = 0;
    }

    float next() noexcept
    {
        if (remaining > 0)
        {
            current += step;
            if (--remaining == 0)
                current = target; // land exactly, no float drift
        }
        return current;
    }

    bool isSmoothing() const noexcept { return remaining > 0; }
    float getCurrent() const noexcept { return current; }
    float getTarget() const noexcept { return target; }
    int getRampSamples() const noexcept { return rampSamples; }

private:
    float current = 0.0f;
    float target = 0.0f;
    float step = 0.0f;
    int rampSamples = 1;
    int remaining = 0;
};

} // namespace augur
