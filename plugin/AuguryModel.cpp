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
    // SPREAD MODE is how the instrument places its voices, not a sound to fly to: a 1.6 scene (no value for it) would
    // otherwise switch an old project to CENTRED in flight.
    return ! (id.startsWith ("trim_") || id == P::pb_range || id == P::pb_down || id == P::osc_model || id == P::vintage_cv
              || id == P::spread_mode);
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

bool AuguryModel::isVoiceStructure (const juce::String& id)
{
    return id == P::voice_count || id == P::unison || id == P::voice_mode || id == P::arp_on || id == P::arp_latch;
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
    if (id.startsWith ("lfo") || id.startsWith ("wm_") || id.startsWith ("pm_") || id.startsWith ("mm") || id == P::at_amount)
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
            structure.push_back (isVoiceStructure (rp->getParameterID()));
        }
    xParam = state.getParameter (P::morph_x);
    yParam = state.getParameter (P::morph_y);
    xRaw = state.getRawParameterValue (P::morph_x);
    yRaw = state.getRawParameterValue (P::morph_y);
    jassert (xParam != nullptr && yParam != nullptr && xRaw != nullptr && yRaw != nullptr);
    // Everything the audio thread touches is allocated here, once.
    rtValues.assign (static_cast<size_t> (numCorners) * params.size(), 0.0f);
    rtNorm.assign (params.size(), 0.0f);
    rtPlain.assign (params.size(), 0.0f);
    rtSlots.assign (params.size(), -1);
    bindingSlot.assign (params.size(), -1);
    markMirrored();
}

void AuguryModel::setBindingSlots (const std::function<int (const juce::String&)>& slotOf)
{
    const juce::SpinLock::ScopedLockType sl (rtLock);
    for (size_t i = 0; i < params.size(); ++i)
        bindingSlot[i] = slotOf (params[i]->getParameterID());
    rtRevision = -1; // recompute
}

juce::StringArray AuguryModel::unboundParameters() const
{
    juce::StringArray ids;
    for (size_t i = 0; i < params.size(); ++i)
        if (bindingSlot[i] < 0)
            ids.add (params[i]->getParameterID());
    return ids;
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

void AuguryModel::publish()
{
    // Called with `lock` held. The copy is all the audio thread reads of the corners; it only ever tries this lock.
    const juce::SpinLock::ScopedLockType sl (rtLock);
    for (int c = 0; c < numCorners; ++c)
    {
        const auto& corner = corners[static_cast<size_t> (c)];
        rtFilled[static_cast<size_t> (c)] = corner.filled && corner.values.size() == params.size();
        if (rtFilled[static_cast<size_t> (c)])
            std::copy (corner.values.begin(), corner.values.end(),
                       rtValues.begin() + static_cast<std::ptrdiff_t> (static_cast<size_t> (c) * params.size()));
    }
    ++revision;
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
    publish();
}

void AuguryModel::clear (int corner)
{
    const juce::ScopedLock sl (lock);
    auto& c = corners[static_cast<size_t> (juce::jlimit (0, numCorners - 1, corner))];
    c = Corner {};
    publish();
}

int AuguryModel::filledCount() const
{
    const juce::ScopedLock sl (lock);
    int n = 0;
    for (const auto& c : corners)
        n += c.filled ? 1 : 0;
    return n;
}

void AuguryModel::setNormalised (size_t index, float v, bool gesture)
{
    auto* p = params[index];
    if (std::abs (p->getValue() - v) < 1.0e-5f)
        return;
    if (gesture && ! inGesture)
        p->beginChangeGesture();
    p->setValueNotifyingHost (v);
    if (gesture && ! inGesture)
        p->endChangeGesture();
}

juce::Point<float> AuguryModel::getPosition() const
{
    return { xRaw->load (std::memory_order_relaxed), yRaw->load (std::memory_order_relaxed) };
}

void AuguryModel::setBird (juce::Point<float> p)
{
    const std::pair<juce::RangedAudioParameter*, float> axes[] { { xParam, p.x }, { yParam, p.y } };
    for (const auto& [param, v] : axes)
    {
        const float n = param->convertTo0to1 (juce::jlimit (0.0f, 1.0f, v));
        if (param->getValue() != n)
            param->setValueNotifyingHost (n);
    }
}

void AuguryModel::placeBird (juce::Point<float> p)
{
    const ScopedNoMorph still (*this);
    setBird (p);
}

void AuguryModel::markMirrored() noexcept
{
    mirroredX.store (xRaw->load (std::memory_order_relaxed), std::memory_order_release);
    mirroredY.store (yRaw->load (std::memory_order_relaxed), std::memory_order_release);
}

AuguryModel::ScopedNoMorph::ScopedNoMorph (AuguryModel& m) noexcept : model (m)
{
    model.noMorph.fetch_add (1, std::memory_order_acq_rel);
}

AuguryModel::ScopedNoMorph::~ScopedNoMorph()
{
    model.markMirrored(); // whatever moved the bird also set the sound that goes with it
    model.noMorph.fetch_sub (1, std::memory_order_acq_rel);
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
    }
    const juce::Point<float> at (corner % 2 == 0 ? 0.0f : 1.0f, corner < 2 ? 0.0f : 1.0f);
    const auto apply = [this, &target, at] {
        const ScopedNoMorph still (*this);
        for (size_t i = 0; i < params.size(); ++i)
            setNormalised (i, target[i], true);
        setBird (at);
    };
    if (soundChange)
        soundChange (apply);
    else
        apply();
}

std::array<float, AuguryModel::numCorners> AuguryModel::weightsAt (float x, float y, const std::array<bool, numCorners>& filled) noexcept
{
    std::array<float, numCorners> w { (1.0f - x) * (1.0f - y), x * (1.0f - y), (1.0f - x) * y, x * y };
    float sum = 0.0f;
    for (int i = 0; i < numCorners; ++i)
    {
        if (! filled[static_cast<size_t> (i)])
            w[static_cast<size_t> (i)] = 0.0f;
        sum += w[static_cast<size_t> (i)];
    }
    if (sum < 1.0e-6f)
    {
        // The point sits on an empty corner's side: inverse-distance weights of the filled ones.
        sum = 0.0f;
        const juce::Point<float> position (x, y);
        for (int i = 0; i < numCorners; ++i)
        {
            const juce::Point<float> c (i % 2 == 0 ? 0.0f : 1.0f, i < 2 ? 0.0f : 1.0f);
            w[static_cast<size_t> (i)] = filled[static_cast<size_t> (i)] ? 1.0f / (0.01f + position.getDistanceFrom (c)) : 0.0f;
            sum += w[static_cast<size_t> (i)];
        }
    }
    if (sum > 0.0f)
        for (auto& v : w)
            v /= sum;
    return w;
}

std::array<float, AuguryModel::numCorners> AuguryModel::weights() const
{
    std::array<bool, numCorners> filled {};
    {
        const juce::ScopedLock sl (lock);
        for (int c = 0; c < numCorners; ++c)
            filled[static_cast<size_t> (c)] = corners[static_cast<size_t> (c)].filled;
    }
    const auto p = getPosition();
    return weightsAt (p.x, p.y, filled);
}

void AuguryModel::blendInto (float x, float y, const std::array<bool, numCorners>& filled, const float* const* values,
                             float* out) const noexcept
{
    // The one blend, on both threads: the editor's drag, the mirror and the audio thread give the same values.
    const auto w = weightsAt (x, y, filled);
    int strongest = 0;
    for (int i = 1; i < numCorners; ++i)
        if (w[static_cast<size_t> (i)] > w[static_cast<size_t> (strongest)])
            strongest = i;
    for (size_t i = 0; i < params.size(); ++i)
    {
        if (structure[i])
            continue; // stays as played
        if (discrete[i])
        {
            out[i] = values[strongest][i];
            continue;
        }
        float v = 0.0f;
        for (int c = 0; c < numCorners; ++c)
            if (w[static_cast<size_t> (c)] > 0.0f)
                v += w[static_cast<size_t> (c)] * values[c][i];
        out[i] = v;
    }
}

void AuguryModel::applyBlend (float x, float y, bool gesture)
{
    std::vector<float> target (params.size());
    {
        const juce::ScopedLock sl (lock);
        std::array<bool, numCorners> filled {};
        std::array<const float*, numCorners> values {};
        int n = 0;
        for (int c = 0; c < numCorners; ++c)
        {
            const auto& corner = corners[static_cast<size_t> (c)];
            filled[static_cast<size_t> (c)] = corner.filled && corner.values.size() == params.size();
            values[static_cast<size_t> (c)] = filled[static_cast<size_t> (c)] ? corner.values.data() : nullptr;
            n += filled[static_cast<size_t> (c)] ? 1 : 0;
        }
        if (n < 2)
            return;
        blendInto (x, y, filled, values.data(), target.data());
    }
    for (size_t i = 0; i < params.size(); ++i)
        if (! structure[i])
            setNormalised (i, target[i], gesture);
}

void AuguryModel::beginMorph()
{
    if (inGesture)
        return;
    for (auto* p : params)
        p->beginChangeGesture();
    xParam->beginChangeGesture();
    yParam->beginChangeGesture();
    inGesture = true;
}

void AuguryModel::endMorph()
{
    if (! inGesture)
        return;
    for (auto* p : params)
        p->endChangeGesture();
    xParam->endChangeGesture();
    yParam->endChangeGesture();
    inGesture = false;
}

void AuguryModel::setPosition (juce::Point<float> p)
{
    p = { juce::jlimit (0.0f, 1.0f, p.x), juce::jlimit (0.0f, 1.0f, p.y) };
    // The blend first, then the bird: the audio thread never sees the bird ahead of the sound it means.
    applyBlend (p.x, p.y, true);
    setBird (p);
    markMirrored();
}

void AuguryModel::mirror()
{
    if (noMorph.load (std::memory_order_acquire) > 0)
        return;
    const auto p = getPosition();
    if (p.x == mirroredX.load (std::memory_order_acquire) && p.y == mirroredY.load (std::memory_order_acquire))
        return;
    applyBlend (p.x, p.y, false); // the host moved it: no gesture
    mirroredX.store (p.x, std::memory_order_release);
    mirroredY.store (p.y, std::memory_order_release);
}

AuguryModel::Values AuguryModel::audioOverride() noexcept
{
    if (noMorph.load (std::memory_order_acquire) > 0)
        return {};
    const float x = xRaw->load (std::memory_order_relaxed), y = yRaw->load (std::memory_order_relaxed);
    if (x == mirroredX.load (std::memory_order_acquire) && y == mirroredY.load (std::memory_order_acquire))
        return {}; // the parameters hold this sound
    const int rev = revision.load (std::memory_order_acquire);
    if (x != rtX || y != rtY || rev != rtRevision)
    {
        const juce::SpinLock::ScopedTryLockType tl (rtLock);
        if (tl.isLocked())
        {
            std::array<const float*, numCorners> values {};
            int n = 0;
            for (int c = 0; c < numCorners; ++c)
            {
                values[static_cast<size_t> (c)] = rtValues.data() + static_cast<size_t> (c) * params.size();
                n += rtFilled[static_cast<size_t> (c)] ? 1 : 0;
            }
            rtCount = 0;
            if (n >= 2)
            {
                blendInto (x, y, rtFilled, values.data(), rtNorm.data());
                for (size_t i = 0; i < params.size(); ++i)
                    if (! structure[i] && bindingSlot[i] >= 0)
                    {
                        rtSlots[static_cast<size_t> (rtCount)] = bindingSlot[i];
                        rtPlain[static_cast<size_t> (rtCount)] = params[i]->convertFrom0to1 (rtNorm[i]);
                        ++rtCount;
                    }
            }
            rtX = x;
            rtY = y;
            rtRevision = rev;
        }
        // (not locked: the corners are changing this instant; the last blend plays one more block)
    }
    return { rtSlots.data(), rtPlain.data(), rtCount };
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

    std::vector<float> next (params.size()), before (params.size());
    for (size_t i = 0; i < params.size(); ++i)
    {
        next[i] = before[i] = params[i]->getValue();
        const auto id = params[i]->getParameterID();
        const int group = groupOf (id);
        // Draw the random numbers for every parameter, so a seed means the same variation whatever is locked.
        const float r1 = gauss(), r2 = rng.nextFloat(), r3 = rng.nextFloat();
        if (group < 0 || (lockedGroups >> group & 1u) != 0)
            continue;
        if (id.endsWith ("_on") || id == P::fx_reverb_freeze || id == P::amp_level)
            continue; // the effects that are in stay in; no frozen tanks; the loudness match stays
        // The modulation patching stays (OMEN varies the amounts, it does not re-patch: a route moved away from the
        // cutoff left the filter closed in 9 of the 30 near-silent omens measured), and OSC B stays an audible,
        // tracking oscillator (LO FREQ takes it out of the audio, KBD off leaves it on one pitch).
        // The filter keeps its mode: the classic filter is a low-pass, and HP / BP took the fundamental of basses away.
        if (id.endsWith ("_src") || id.endsWith ("_dst") || id == P::osc2_lofreq || id == P::osc2_kbd || id == P::flt_mode)
            continue;
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

    // Guards measured on 20 omens in a row at 100 % (augur_preset_audit --check-augury): what took a sound close to
    // silence. The rule is "not worse than the sound was": a sound that already had it keeps it.
    const auto plain = [&] (const juce::String& id, const std::vector<float>& v) {
        const int i = indexOf (id.toRawUTF8());
        return i < 0 ? 0.0f : params[static_cast<size_t> (i)]->getNormalisableRange().convertFrom0to1 (v[static_cast<size_t> (i)]);
    };
    const auto setPlain = [&] (const juce::String& id, float value) {
        if (const int i = indexOf (id.toRawUTF8()); i >= 0)
            next[static_cast<size_t> (i)] = params[static_cast<size_t> (i)]->getNormalisableRange().convertTo0to1 (value);
    };
    // A sustained sound keeps sustaining: the amp sustain stays at 40 % or more (-8 dB) unless it was lower.
    if (const float s = plain (P::aenv_s, before); plain (P::aenv_s, next) < std::min (s, 0.4f))
        setPlain (P::aenv_s, std::min (s, 0.4f));
    // A percussive sound (attack up to 30 ms) stays percussive: a long attack silences short notes and arpeggios.
    // (30 ms read back from its normalised value is a hair above 30 ms: the test has a margin.)
    for (const char* id : { P::aenv_a, P::fenv_a })
        if (plain (id, before) <= 0.031f && plain (id, next) > 0.03f)
            setPlain (id, 0.03f);
    // HP and BP keep a cutoff where they let sound through (or where the sound had it).
    if (const int mode = juce::roundToInt (plain (P::flt_mode, next)); mode == 1 || mode == 2)
    {
        const float was = plain (P::flt_cutoff, before);
        const float lo = mode == 2 ? 180.0f : 300.0f, hi = mode == 2 ? 1500.0f : 2500.0f;
        float cut = juce::jlimit (std::min (lo, was), std::max (hi, was), plain (P::flt_cutoff, next));
        if (mode == 1)
        {
            // A band-pass's centre is its colour: it moves an octave per omen at most, and velocity does not push it
            // further up than the sound did (30 %).
            cut = juce::jlimit (0.5f * was, 2.0f * was, cut);
            if (const float v = std::max (0.3f, plain (P::flt_velocity, before)); plain (P::flt_velocity, next) > v)
                setPlain (P::flt_velocity, v);
        }
        setPlain (P::flt_cutoff, cut);
    }
    // An inverted filter envelope may pull the cutoff down to 180 Hz at its peak (7 octaves at ENV AMT -1), no further.
    {
        const float cut = plain (P::flt_cutoff, next);
        const float floorAmt = -std::log2 (std::max (1.0f, cut / 180.0f)) / 7.0f;
        const float amt = std::max (plain (P::flt_env_amt, next), std::min (floorAmt, plain (P::flt_env_amt, before)));
        setPlain (P::flt_env_amt, amt);
        // The high-pass stays an octave below the lowest point of the low-pass (and below 400 Hz unless it was higher).
        const float lowest = cut * std::exp2 (7.0f * std::min (0.0f, amt));
        const float hpfMax = std::min (0.5f * lowest, std::max (400.0f, plain (P::hpf_cutoff, before)));
        if (plain (P::hpf_cutoff, next) > hpfMax)
            setPlain (P::hpf_cutoff, std::max (10.0f, hpfMax));
    }
    // Matrix routes into the cutoff and the levels do not pull further down than they did, and an empty slot (amount 0)
    // stays empty: OMEN varies the patch, it does not add routes.
    for (int k = 1; k <= 8; ++k)
    {
        const auto slot = "mm" + juce::String (k);
        if (std::abs (plain (slot + "_amt", before)) < 1.0e-4f)
        {
            setPlain (slot + "_amt", 0.0f);
            continue;
        }
        const int dst = juce::roundToInt (plain (slot + "_dst", next));
        if (dst == 4 || dst == 6 || dst == 12 || dst == 13) // FILTER CUTOFF, AMP LEVEL, OSC 1 LEVEL, OSC 2 LEVEL
            setPlain (slot + "_amt", std::max (plain (slot + "_amt", next), std::min (0.0f, plain (slot + "_amt", before))));
    }

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
    // A locked group does not move, guards included.
    for (size_t i = 0; i < params.size(); ++i)
        if (const int group = groupOf (params[i]->getParameterID()); group >= 0 && (lockedGroups >> group & 1u) != 0)
            next[i] = before[i];

    const auto apply = [this, &next] {
        const ScopedNoMorph still (*this); // the bird stays where it is, with the new sound
        for (size_t i = 0; i < params.size(); ++i)
            setNormalised (i, next[i], true);
    };
    if (soundChange)
        soundChange (apply);
    else
        apply();
    return seed;
}

juce::ValueTree AuguryModel::toTree() const
{
    const auto position = getPosition();
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
    {
        const juce::ScopedLock sl (lock);
        loadCorners (tree);
        publish();
    }
    // The bird where the scene had it, with the sound that came with it (no morph).
    if (tree.hasType ("AUGURY"))
    {
        const ScopedNoMorph still (*this);
        setBird ({ static_cast<float> (tree.getProperty ("x", 0.5)), static_cast<float> (tree.getProperty ("y", 0.5)) });
    }
}

void AuguryModel::loadCorners (const juce::ValueTree& tree)
{
    for (auto& c : corners)
        c = Corner {};
    if (! tree.hasType ("AUGURY"))
        return;
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
