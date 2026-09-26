set(AUGUR_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND AUGUR_FORMATS AU)
endif()

# Manufacturer/plugin codes and bundle id identify the plugin inside every DAW
# (saved projects, presets, AU registry). Never change them after release.
juce_add_plugin(Augur5
    PRODUCT_NAME "AUGUR-5"
    COMPANY_NAME "TONAL LAB"
    COMPANY_COPYRIGHT "TONAL LAB"
    BUNDLE_ID "com.tonallab.augur5"
    VERSION ${PROJECT_VERSION}
    PLUGIN_MANUFACTURER_CODE Tnlb
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

# Embedded OFL fonts (licenses next to the files).
juce_add_binary_data(AugurBinary
    HEADER_NAME AugurBinary.h
    NAMESPACE AugurBinary
    SOURCES
        resources/fonts/Jost-Regular.ttf
        resources/fonts/Jost-Medium.ttf
        resources/fonts/Michroma-Regular.ttf
        resources/fonts/JetBrainsMono-Medium.ttf
)

target_sources(Augur5 PRIVATE
    Parameters.h
    ParameterLayout.h
    ParameterLayout.cpp
    Presets.h
    Presets.cpp
    PluginProcessor.h
    PluginProcessor.cpp
    PluginEditor.h
    PluginEditor.cpp
    gui/Theme.h
    gui/Theme.cpp
    gui/AugurLookAndFeel.h
    gui/AugurLookAndFeel.cpp
    gui/Widgets.h
    gui/Widgets.cpp
)

target_compile_definitions(Augur5 PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(Augur5
    PRIVATE
        augur_dsp
        AugurBinary
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)


# Factory library audit: a console app built from the same processor/preset sources, which plays every
# factory preset through the real processor and checks its level (see tools/preset_audit).
juce_add_console_app(augur_preset_audit PRODUCT_NAME "augur_preset_audit")
target_include_directories(augur_preset_audit PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_sources(augur_preset_audit PRIVATE
    ${CMAKE_SOURCE_DIR}/tools/preset_audit/main.cpp
    PluginProcessor.cpp
    ParameterLayout.cpp
    Presets.cpp
    PluginEditor.cpp
    gui/Theme.cpp
    gui/AugurLookAndFeel.cpp
    gui/Widgets.cpp
)
target_compile_definitions(augur_preset_audit PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JucePlugin_Name="AUGUR-5"
    JucePlugin_VersionString="${PROJECT_VERSION}"
)
target_link_libraries(augur_preset_audit PRIVATE
    augur_dsp
    AugurBinary
    juce::juce_audio_utils
    juce::juce_recommended_config_flags
)
