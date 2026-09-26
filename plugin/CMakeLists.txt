set(AUGUR_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND AUGUR_FORMATS AU)
endif()

# Manufacturer/plugin codes and bundle id identify the plugin inside every DAW
# (saved projects, presets, AU registry). Never change them after release.
juce_add_plugin(Augur5
    PRODUCT_NAME "AUGUR-5"
    COMPANY_NAME "Augur Audio"
    BUNDLE_ID "com.auguraudio.augur5"
    VERSION ${PROJECT_VERSION}
    PLUGIN_MANUFACTURER_CODE Augr
    PLUGIN_CODE Au53
    FORMATS ${AUGUR_FORMATS}
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Instrument Synth
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    COPY_PLUGIN_AFTER_BUILD FALSE
)

target_sources(Augur5 PRIVATE
    Parameters.h
    PluginProcessor.h
    PluginProcessor.cpp
    PluginEditor.h
    PluginEditor.cpp
)

target_compile_definitions(Augur5 PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(Augur5
    PRIVATE
        augur_dsp
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
