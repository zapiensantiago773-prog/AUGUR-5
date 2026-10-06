#include "AuguryModel.h"

#include "Parameters.h"
#include "Presets.h"

namespace augur5
{

namespace
{
namespace P = params;

// Instrument settings and per-unit calibration: never morphed or varied.
bool isMorphable (const juce::String& id)
{
    if (PresetManager::isGlobalSetting (id))
        return false;
    return ! (id.startsWith ("trim_") || id == P::pb_range || id == P::osc_model || id == P::vintage_cv);
}

bool isDiscrete (const juce::RangedAudioParameter* p)
{
    return dynamic_cast<const juce::AudioParameterChoice*> (p) != nullptr || dynamic_cast<const juce::AudioParameterBool*> (p) != nullptr
        || dynamic_cast<const juce::AudioParameterInt*> (p) != nullptr;
}
} // namespace

const char* AuguryModel::groupName (int g)
{
    static const char* const names[numGroups] { "OSCILLATORS", "MIXER", "FILTER", "ENVELOPES", "MODULATION", "EFFECTS" };
    return names[juce::jlimit (0, numGroups - 1, g)];
}

int AuguryModel::groupOf (const juce::String& id)
{
    if (id.startsWith ("osc") || id == P::sub_oct)
        return oscillators;
    if (id.startsWith ("mix_"))
        return mixer;
    if (id.startsWith ("flt_") || id == P::hpf_cutoff)
        return filter;
    if (id.startsWith ("fenv_") || id.startsWith ("aenv_") || id.startsWith ("menv_"))
        return envelopes;
    if (id.startsWith ("lfo") || id.startsWith ("pm_") || id.startsWith ("mm") || id == P::at_amount)
        return modulation;
    if (id.startsWith ("fx_") || id.startsWith ("fuzz_") || id.startsWith ("delay_"))
        return effects;
    return -1; // voice, glide, arp, level...: kept by OMEN (still morphed)
}

AuguryModel::AuguryModel (juce::AudioProcessorValueTreeState& s) : state (s)
{
    for (auto* p : state.processor.getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p); rp != nullptr && isMorphable (rp->getParameterID()))
        {
            params.push_back (rp);
            discrete.push_back (isDiscrete (rp));
        }
}

bool AuguryModel::isFilled (int corner) const
{
    const juce::ScopedLock sl (lock);
    return corners[static_cast<size_t> (juce::jlimit (0, numCorners - 1, corner))].filled;
}

juce::String AuguryModel::getName (int corner) const
{
    const juce::ScopedLock sl (lock);
    return corners[static_cast<size_t> (juce::jlimit (0, numCorners - 1, corner))].name;
}

void AuguryModel::capture (int corner, const juce::String& soundName)
{
    const juce::ScopedLock sl (lock);
    auto& c = corners[static_cast<size_t> (juce::jlimit (0, numCorners - 1, corner))];
    c.values.resize (params.size());
    for (size_t i = 0; i < params.size(); ++i)
        c.values[i] = params[i]->getValue();
    c.name = soundName;
    c.filled = true;
}

void AuguryModel::clear (int corner)
{
    const juce::ScopedLock sl (lock);
    auto& c = corners[static_cast<size_t> (juce::jlimit (0, numCorners - 1, corner))];
    c = Corner {};
}

int AuguryModel::filledCount() const
{
    const juce::ScopedLock sl (lock);
    int n = 0;
    for (const auto& c : corners)
        n += c.filled ? 1 : 0;
    return n;
}

void AuguryModel::setNormalised (size_t index, float v)
{
    auto* p = params[index];
    if (std::abs (p->getValue() - v) < 1.0e-5f)
        return;
    if (! inGesture)
        p->beginChangeGesture();
    p->setValueNotifyingHost (v);
    if (! inGesture)
        p->endChangeGesture();
}

void AuguryModel::recall (int corner)
{
    std::vector<float> target;
    {
        // The values are copied under the lock; the host is notified without it (no lock-order inversion).
        const juce::ScopedLock sl (lock);
        const auto& c = corners[static_cast<size_t> (juce::jlimit (0, numCorners - 1, corner))];
        if (! c.filled || c.values.size() != params.size())
            return;
        target = c.values;
        position = { corner % 2 == 0 ? 0.0f : 1.0f, corner < 2 ? 0.0f : 1.0f };
    }
    for (size_t i = 0; i < params.size(); ++i)
        setNormalised (i, target[i]);
}

std::array<float, AuguryModel::numCorners> AuguryModel::weights() const
{
    const juce::ScopedLock sl (lock);
    const float x = position.x, y = position.y;
    std::array<float, numCorners> w { (1.0f - x) * (1.0f - y), x * (1.0f - y), (1.0f - x) * y, x * y };
    float sum = 0.0f;
    for (int i = 0; i < numCorners; ++i)
    {
        if (! corners[static_cast<size_t> (i)].filled)
            w[static_cast<size_t> (i)] = 0.0f;
        sum += w[static_cast<size_t> (i)];
    }
    if (sum < 1.0e-6f)
    {
        // The point sits on an empty corner's side: inverse-distance weights of the filled ones.
        sum = 0.0f;
        for (int i = 0; i < numCorners; ++i)
        {
            const juce::Point<float> c (i % 2 == 0 ? 0.0f : 1.0f, i < 2 ? 0.0f : 1.0f);
            w[static_cast<size_t> (i)] = corners[static_cast<size_t> (i)].filled ? 1.0f / (0.01f + position.getDistanceFrom (c)) : 0.0f;
            sum += w[static_cast<size_t> (i)];
        }
    }
    if (sum > 0.0f)
        for (auto& v : w)
            v /= sum;
    return w;
}

void AuguryModel::beginMorph()
{
    if (inGesture)
        return;
    for (auto* p : params)
        p->beginChangeGesture();
    inGesture = true;
}

void AuguryModel::endMorph()
{
    if (! inGesture)
        return;
    for (auto* p : params)
        p->endChangeGesture();
    inGesture = false;
}

void AuguryModel::setPosition (juce::Point<float> p)
{
    std::vector<float> target (params.size());
    {
        const juce::ScopedLock sl (lock);
        position = { juce::jlimit (0.0f, 1.0f, p.x), juce::jlimit (0.0f, 1.0f, p.y) };
        if (filledCount() < 2)
            return;
        const auto w = weights();
        int strongest = 0;
        for (int i = 1; i < numCorners; ++i)
            if (w[static_cast<size_t> (i)] > w[static_cast<size_t> (strongest)])
                strongest = i;
        for (size_t i = 0; i < params.size(); ++i)
        {
            if (discrete[i])
            {
                target[i] = corners[static_cast<size_t> (strongest)].values[i];
                continue;
            }
            float v = 0.0f;
            for (int c = 0; c < numCorners; ++c)
                if (w[static_cast<size_t> (c)] > 0.0f)
                    v += w[static_cast<size_t> (c)] * corners[static_cast<size_t> (c)].values[i];
            target[i] = v;
        }
    }
    for (size_t i = 0; i < params.size(); ++i)
        setNormalised (i, target[i]);
}

juce::uint32 AuguryModel::castOmen (float amount, juce::uint32 lockedGroups, juce::uint32 seed)
{
    if (seed == 0)
        seed = static_cast<juce::uint32> (juce::Random::getSystemRandom().nextInt (999999) + 1);
    lastSeed = seed;
    juce::Random rng (static_cast<juce::int64> (seed));
    amount = juce::jlimit (0.0f, 1.0f, amount);
    const auto gauss = [&rng] {
        // Sum of three uniforms: a cheap, bounded bell (sd ~ 0.5).
        return rng.nextFloat() + rng.nextFloat() + rng.nextFloat() - 1.5f;
    };

    std::vector<float> next (params.size());
    for (size_t i = 0; i < params.size(); ++i)
    {
        next[i] = params[i]->getValue();
        const auto id = params[i]->getParameterID();
        const int group = groupOf (id);
        // Draw the random numbers for every parameter, so a seed means the same variation whatever is locked.
        const float r1 = gauss(), r2 = rng.nextFloat(), r3 = rng.nextFloat();
        if (group < 0 || (lockedGroups >> group & 1u) != 0)
            continue;
        if (id.endsWith ("_on") || id == P::fx_reverb_freeze || id == P::amp_level)
            continue; // the effects that are in stay in; no frozen tanks; the loudness match stays
        if (discrete[i])
        {
            if (r2 < 0.22f * amount)
            {
                const int steps = juce::jmax (2, params[i]->getNumSteps());
                next[i] = static_cast<float> (juce::jlimit (0, steps - 1, static_cast<int> (r3 * static_cast<float> (steps))))
                        / static_cast<float> (steps - 1);
            }
            continue;
        }
        next[i] = juce::jlimit (0.0f, 1.0f, next[i] + 0.7f * amount * r1);
    }

    // Guards: keep the result playable.
    const auto indexOf = [this] (const char* id) {
        for (size_t i = 0; i < params.size(); ++i)
            if (params[i]->getParameterID() == id)
                return static_cast<int> (i);
        return -1;
    };
    const auto clampPlain = [&] (const char* id, float lo, float hi) {
        if (const int i = indexOf (id); i >= 0)
        {
            const auto range = params[static_cast<size_t> (i)]->getNormalisableRange();
            const float v = juce::jlimit (lo, hi, range.convertFrom0to1 (next[static_cast<size_t> (i)]));
            next[static_cast<size_t> (i)] = range.convertTo0to1 (v);
        }
    };
    clampPlain (P::flt_cutoff, 180.0f, 20000.0f);
    clampPlain (P::flt_reso, 0.0f, 0.82f);
    clampPlain (P::aenv_a, 0.001f, 1.2f);
    clampPlain (P::aenv_s, 0.15f, 1.0f);
    clampPlain (P::fx_echo_intensity, 0.0f, 0.72f);
    clampPlain (P::delay_fb, 0.0f, 0.72f);
    clampPlain (P::fx_flanger_feedback, -0.8f, 0.8f);
    for (const char* mix : { P::fx_drive_mix, P::fx_chorus_mix, P::fx_phaser_mix, P::fx_flanger_mix, P::delay_mix, P::fx_echo_mix,
                             P::fx_reverb_mix, P::fuzz_mix })
        clampPlain (mix, 0.0f, 0.6f);
    clampPlain (P::fx_reverb_decay, 0.2f, 8.0f);
    const auto on = [&] (const char* id) {
        const int i = indexOf (id);
        return i >= 0 && next[static_cast<size_t> (i)] > 0.5f;
    };
    const auto setOn = [&] (const char* id) {
        if (const int i = indexOf (id); i >= 0)
            next[static_cast<size_t> (i)] = 1.0f;
    };
    if (! on (P::osc1_saw) && ! on (P::osc1_pulse))
        setOn (P::osc1_saw);
    if (! on (P::osc2_saw) && ! on (P::osc2_tri) && ! on (P::osc2_pulse))
        setOn (P::osc2_saw);
    {
        const int a = indexOf (P::mix_osc1), b = indexOf (P::mix_osc2);
        if (a >= 0 && b >= 0 && next[static_cast<size_t> (a)] < 0.3f && next[static_cast<size_t> (b)] < 0.3f)
            next[static_cast<size_t> (a)] = 0.7f;
    }

    for (size_t i = 0; i < params.size(); ++i)
        setNormalised (i, next[i]);
    return seed;
}

juce::ValueTree AuguryModel::toTree() const
{
    const juce::ScopedLock sl (lock);
    juce::ValueTree tree ("AUGURY");
    tree.setProperty ("x", position.x, nullptr);
    tree.setProperty ("y", position.y, nullptr);
    tree.setProperty ("seed", static_cast<juce::int64> (lastSeed), nullptr);
    for (int c = 0; c < numCorners; ++c)
    {
        const auto& corner = corners[static_cast<size_t> (c)];
        if (! corner.filled)
            continue;
        juce::ValueTree node ("CORNER");
        node.setProperty ("index", c, nullptr);
        node.setProperty ("name", corner.name, nullptr);
        for (size_t i = 0; i < params.size() && i < corner.values.size(); ++i)
        {
            juce::ValueTree v ("V");
            v.setProperty ("id", params[i]->getParameterID(), nullptr);
            v.setProperty ("value", corner.values[i], nullptr);
            node.appendChild (v, nullptr);
        }
        tree.appendChild (node, nullptr);
    }
    return tree;
}

void AuguryModel::fromTree (const juce::ValueTree& tree)
{
    const juce::ScopedLock sl (lock);
    for (auto& c : corners)
        c = Corner {};
    if (! tree.hasType ("AUGURY"))
        return;
    position = { static_cast<float> (tree.getProperty ("x", 0.5)), static_cast<float> (tree.getProperty ("y", 0.5)) };
    lastSeed = static_cast<juce::uint32> (static_cast<juce::int64> (tree.getProperty ("seed", 0)));
    for (const auto& node : tree)
    {
        if (! node.hasType ("CORNER"))
            continue;
        const int c = juce::jlimit (0, numCorners - 1, static_cast<int> (node.getProperty ("index", 0)));
        auto& corner = corners[static_cast<size_t> (c)];
        corner.filled = true;
        corner.name = node.getProperty ("name").toString();
        corner.values.resize (params.size());
        // By id, so corners saved by another version still line up; parameters missing there keep their default.
        for (size_t i = 0; i < params.size(); ++i)
            corner.values[i] = params[i]->getDefaultValue();
        for (const auto& v : node)
        {
            const auto id = v.getProperty ("id").toString();
            for (size_t i = 0; i < params.size(); ++i)
                if (params[i]->getParameterID() == id)
                {
                    corner.values[i] = juce::jlimit (0.0f, 1.0f, static_cast<float> (v.getProperty ("value", 0.0)));
                    break;
                }
        }
    }
}

} // namespace augur5
