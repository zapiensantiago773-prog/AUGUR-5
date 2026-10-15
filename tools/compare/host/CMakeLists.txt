# compare_host: plays any VST3 instrument's presets with the pack audit's phrases (tools/compare/README.md).
# A separate project, so the plug-in build stays as it is:
#   cmake -S tools/compare/host -B build/compare_host -G Ninja -DCMAKE_BUILD_TYPE=Release
#   cmake --build build/compare_host
cmake_minimum_required(VERSION 3.22)
project(compare_host VERSION 1.0.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../../external/JUCE ${CMAKE_BINARY_DIR}/juce)
juce_add_console_app(compare_host PRODUCT_NAME "compare_host")
target_sources(compare_host PRIVATE main.cpp)
target_compile_definitions(compare_host PRIVATE
    JUCE_PLUGINHOST_VST3=1
    JUCE_PLUGINHOST_AU=0
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_MODAL_LOOPS_PERMITTED=1)
target_link_libraries(compare_host PRIVATE
    juce::juce_audio_processors
    juce::juce_gui_basics
    juce::juce_recommended_config_flags)
