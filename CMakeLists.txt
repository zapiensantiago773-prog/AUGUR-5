cmake_minimum_required(VERSION 3.25)

# Must be set before project() to take effect.
if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Universal binary")
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
endif()

# Static MSVC runtime: the plugin must load on machines without the VC++ redistributable.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

project(AUGUR5 VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

option(AUGUR_BUILD_TESTS "Build DSP unit tests" ON)

# Warning set for our own targets (JUCE targets use juce_recommended_warning_flags).
add_library(augur_warnings INTERFACE)
if(MSVC)
    target_compile_options(augur_warnings INTERFACE /W4 /permissive- /utf-8)
else()
    target_compile_options(augur_warnings INTERFACE -Wall -Wextra -Wpedantic -Wshadow -Wconversion)
endif()

add_subdirectory(external/JUCE)
add_subdirectory(dsp)               # augur_dsp: pure C++, no JUCE
add_subdirectory(plugin)            # JUCE plugin, thin layer over augur_dsp
add_subdirectory(tools/render_cli)  # offline WAV renderer, augur_dsp only

if(AUGUR_BUILD_TESTS)
    add_subdirectory(external/Catch2)
    enable_testing()
    add_subdirectory(tests)
endif()
