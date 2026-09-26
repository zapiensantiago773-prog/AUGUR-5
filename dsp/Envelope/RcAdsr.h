#pragma once

#include <algorithm>
#include <cmath>

namespace augur
{

// CEM3310-style ADSR: every segment is an RC charge/discharge (exponential), not a linear or
// "digital" curve. The attack charges towards 1.5 and is cut by the comparator at 1.0, which gives
// the characteristic convex, fast-finishing attack. Retriggers continue from the current level.
class RcAdsr
{
public:
    enum class Stage
    {
        Idle,
        Attack,
        Decay, // decays towards sustain and stays there
        Release
    };

    void prepare (double newSampleRate) noexcept
    {
        sampleRate = newSampleRate;
        reset();
    }

    void reset() noexcept
    {
        stage = Stage::Idle;
        level = 0.0f;
    }

    // Times in seconds. Attack = time to reach full level; decay/release = time to fall 60 dB.
    void setParameters (float attack, float decay, float sustainLevel, float release) noexcept
    {
        constexpr float attackTarget = 1.5f;
        const float ln3 = std::log (attackTarget / (attackTarget - 1.0f)); // ln 3
        attackCoeff = coeff (std::max (attack, 0.0005f) / ln3);
        decayCoeff = coeff (std::max (decay, 0.001f) / 6.9078f);
        releaseCoeff = coeff (std::max (release, 0.001f) / 6.9078f);
        sustain = std::clamp (sustainLevel, 0.0f, 1.0f);
    }

    void noteOn() noexcept { stage = Stage::Attack; }

    void noteOff() noexcept
    {
        if (stage != Stage::Idle)
            stage = Stage::Release;
    }

    float next() noexcept
    {
        switch (stage)
        {
            case Stage::Attack:
                level = 1.5f + (level - 1.5f) * attackCoeff;
                if (level >= 1.0f)
                {
                    level = 1.0f;
                    stage = Stage::Decay;
                }
                break;
            case Stage::Decay:
                level = sustain + (level - sustain) * decayCoeff;
                break;
            case Stage::Release:
                level *= releaseCoeff;
                if (level < 1.0e-5f)
                {
                    level = 0.0f;
                    stage = Stage::Idle;
                }
                break;
            case Stage::Idle:
                break;
        }
        return level;
    }

    bool isActive() const noexcept { return stage != Stage::Idle; }
    bool isReleasing() const noexcept { return stage == Stage::Release; }
    Stage getStage() const noexcept { return stage; }
    float getLevel() const noexcept { return level; }

private:
    float coeff (float tau) const noexcept
    {
        return static_cast<float> (std::exp (-1.0 / (static_cast<double> (tau) * sampleRate)));
    }

    double sampleRate = 48000.0;
    Stage stage = Stage::Idle;
    float level = 0.0f;
    float sustain = 1.0f;
    float attackCoeff = 0.0f, decayCoeff = 0.0f, releaseCoeff = 0.0f;
};

} // namespace augur
