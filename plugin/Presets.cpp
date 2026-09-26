#include "Presets.h"
#include "Parameters.h"

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
    std::vector<Setting> settings; // applied on top of the defaults (= Init)
};

const std::vector<FactoryPreset>& factoryBank()
{
    static const std::vector<FactoryPreset> bank {
        { "Init", {} },

        { "Warm Horizon", {
            { "mix_osc1", 0.8f }, { "mix_osc2", 0.72f }, { "mix_noise", 0.05f }, { "mix_drive", 0.3f },
            { "osc2_fine", 7.0f }, { "flt_cutoff", 1200.0f }, { "flt_reso", 0.3f }, { "flt_env_amt", 0.3f },
            { "fenv_a", 0.01f }, { "fenv_d", 1.2f }, { "fenv_s", 0.5f }, { "fenv_r", 1.5f },
            { "aenv_a", 0.02f }, { "aenv_d", 1.0f }, { "aenv_s", 0.75f }, { "aenv_r", 1.2f },
            { "amp_velocity", 0.4f }, { "flt_velocity", 0.25f },
            { "mm3_amt", 0.08f }, { "lfo_rate", 0.4f }, { "lfo_amount", 0.6f },
            { "voice_detune", 0.3f }, { "voice_spread", 0.4f }, { "analog_age", 0.4f },
            { "chorus_on", 1.0f }, { "chorus_mix", 0.3f }, { "reverb_on", 1.0f }, { "reverb_mix", 0.25f } } },

        { "Brass Section", {
            { "mix_osc1", 0.9f }, { "mix_osc2", 0.8f }, { "osc2_fine", -6.0f }, { "flt_cutoff", 500.0f },
            { "flt_reso", 0.1f }, { "flt_env_amt", 0.55f }, { "fenv_a", 0.06f }, { "fenv_d", 0.5f }, { "fenv_s", 0.45f },
            { "fenv_r", 0.3f }, { "aenv_a", 0.03f }, { "aenv_d", 0.4f }, { "aenv_s", 0.9f }, { "aenv_r", 0.3f },
            { "flt_velocity", 0.3f }, { "analog_age", 0.45f } } },

        { "Sync Lead", {
            { "osc1_sync", 1.0f }, { "osc1_freq", 12.0f }, { "mix_osc1", 1.0f }, { "mix_osc2", 0.0f },
            { "pm_on", 1.0f }, { "pm_fenv_amt", 0.35f }, { "pm_dst_freqa", 1.0f },
            { "flt_cutoff", 3500.0f }, { "flt_reso", 0.2f }, { "flt_env_amt", 0.2f },
            { "fenv_a", 0.001f }, { "fenv_d", 0.9f }, { "fenv_s", 0.2f }, { "voice_count", 1.0f }, { "glide", 0.06f },
            { "legato", 1.0f }, { "mm4_src", 3.0f }, { "mm4_dst", 0.0f }, { "mm4_amt", 0.1f }, { "lfo_rate", 5.5f },
            { "delay_on", 1.0f }, { "delay_mix", 0.2f } } },

        { "Poly Strings", {
            { "osc1_pulse", 1.0f }, { "osc1_saw", 0.0f }, { "osc1_pw", 30.0f }, { "osc2_fine", 9.0f },
            { "mix_osc1", 0.8f }, { "mix_osc2", 0.8f }, { "flt_cutoff", 2200.0f }, { "flt_env_amt", 0.1f },
            { "aenv_a", 0.45f }, { "aenv_s", 0.85f }, { "aenv_r", 1.4f }, { "fenv_a", 0.5f },
            { "mm1_src", 3.0f }, { "mm1_dst", 2.0f }, { "mm1_amt", 0.35f }, { "lfo_rate", 0.7f },
            { "voice_spread", 0.7f }, { "chorus_on", 1.0f }, { "chorus_mix", 0.5f }, { "reverb_on", 1.0f }, { "reverb_mix", 0.3f } } },

        { "Pluck Keys", {
            { "mix_osc2", 0.5f }, { "osc2_freq", 12.0f }, { "flt_cutoff", 300.0f }, { "flt_reso", 0.25f },
            { "flt_env_amt", 0.6f }, { "fenv_a", 0.001f }, { "fenv_d", 0.35f }, { "fenv_s", 0.0f }, { "fenv_r", 0.3f },
            { "aenv_a", 0.001f }, { "aenv_d", 1.1f }, { "aenv_s", 0.0f }, { "aenv_r", 0.4f }, { "flt_velocity", 0.5f },
            { "delay_on", 1.0f }, { "delay_time", 0.33f }, { "delay_mix", 0.18f } } },

        { "Fat Bass", {
            { "osc2_freq", -12.0f }, { "osc2_pulse", 1.0f }, { "osc2_saw", 0.0f }, { "mix_osc2", 0.9f }, { "mix_drive", 0.55f },
            { "flt_cutoff", 180.0f }, { "flt_reso", 0.3f }, { "flt_env_amt", 0.45f }, { "fenv_a", 0.001f },
            { "fenv_d", 0.25f }, { "fenv_s", 0.1f }, { "aenv_a", 0.001f }, { "aenv_d", 0.5f }, { "aenv_s", 0.7f },
            { "aenv_r", 0.08f }, { "voice_count", 1.0f }, { "flt_model", 1.0f }, { "flt_keytrack", 1.0f } } },

        { "Poly-Mod Bell", {
            { "osc2_freq", 19.0f }, { "osc1_saw", 0.0f }, { "osc1_pulse", 1.0f }, { "osc1_pw", 50.0f },
            { "mix_osc2", 0.0f }, { "pm_on", 1.0f }, { "pm_osc2_amt", 0.35f }, { "pm_fenv_amt", 0.15f },
            { "pm_dst_freqa", 1.0f }, { "flt_cutoff", 5000.0f }, { "flt_env_amt", 0.2f },
            { "aenv_a", 0.001f }, { "aenv_d", 2.5f }, { "aenv_s", 0.0f }, { "aenv_r", 2.0f },
            { "fenv_d", 1.5f }, { "fenv_s", 0.0f }, { "reverb_on", 1.0f }, { "reverb_mix", 0.35f }, { "reverb_decay", 4.0f } } },

        { "Unison Monster", {
            { "unison", 1.0f }, { "voice_count", 6.0f }, { "voice_detune", 0.35f }, { "voice_spread", 0.9f },
            { "mix_osc2", 0.9f }, { "osc2_freq", -12.0f }, { "mix_drive", 0.5f }, { "flt_cutoff", 1400.0f },
            { "flt_reso", 0.35f }, { "flt_env_amt", 0.4f }, { "fenv_d", 0.8f }, { "fenv_s", 0.3f }, { "glide", 0.05f } } },

        { "Dream Pad", {
            { "osc1_pulse", 1.0f }, { "osc1_pw", 40.0f }, { "osc2_tri", 1.0f }, { "osc2_saw", 0.0f }, { "osc2_fine", 12.0f },
            { "mix_osc2", 0.7f }, { "mix_noise", 0.08f }, { "flt_cutoff", 900.0f }, { "flt_reso", 0.35f },
            { "flt_env_amt", 0.25f }, { "fenv_a", 2.5f }, { "fenv_d", 3.0f }, { "fenv_s", 0.4f }, { "fenv_r", 3.0f },
            { "aenv_a", 1.8f }, { "aenv_d", 2.0f }, { "aenv_s", 0.8f }, { "aenv_r", 3.5f },
            { "mm1_src", 3.0f }, { "mm1_dst", 4.0f }, { "mm1_amt", 0.25f }, { "lfo_rate", 0.15f }, { "lfo_amount", 0.8f },
            { "mm2_src", 3.0f }, { "mm2_dst", 2.0f }, { "mm2_amt", 0.3f },
            { "voice_spread", 0.8f }, { "chorus_on", 1.0f }, { "chorus_mix", 0.45f },
            { "reverb_on", 1.0f }, { "reverb_mix", 0.45f }, { "reverb_size", 0.85f }, { "reverb_decay", 7.0f } } },

        { "Vintage Keys", {
            { "osc1_saw", 0.0f }, { "osc1_pulse", 1.0f }, { "osc1_pw", 22.0f }, { "osc2_pulse", 1.0f }, { "osc2_saw", 0.0f },
            { "osc2_pw", 45.0f }, { "osc2_freq", 12.0f }, { "mix_osc2", 0.4f }, { "flt_cutoff", 1600.0f },
            { "flt_env_amt", 0.3f }, { "fenv_d", 0.7f }, { "fenv_s", 0.15f }, { "aenv_a", 0.002f }, { "aenv_d", 1.6f },
            { "aenv_s", 0.35f }, { "aenv_r", 0.35f }, { "analog_age", 0.85f }, { "flt_model", 1.0f }, { "osc_model", 1.0f },
            { "chorus_on", 1.0f }, { "chorus_mix", 0.35f } } },

        { "Resonant Sweep", {
            { "mix_osc2", 0.7f }, { "osc2_fine", 11.0f }, { "flt_cutoff", 150.0f }, { "flt_reso", 0.72f },
            { "flt_env_amt", 0.7f }, { "fenv_a", 1.8f }, { "fenv_d", 3.5f }, { "fenv_s", 0.2f }, { "fenv_r", 2.5f },
            { "aenv_a", 0.4f }, { "aenv_s", 0.9f }, { "aenv_r", 2.5f }, { "delay_on", 1.0f }, { "delay_mix", 0.25f },
            { "delay_fb", 0.5f }, { "reverb_on", 1.0f }, { "reverb_mix", 0.3f } } },
    };
    return bank;
}

constexpr const char* presetExtension = ".augur5";
} // namespace

PresetManager::PresetManager (juce::AudioProcessorValueTreeState& s) : state (s) {}

int PresetManager::getNumFactoryPresets() const noexcept { return static_cast<int> (factoryBank().size()); }

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
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("AUGUR-5").getChildFile ("Presets");
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
