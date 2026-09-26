add_library(augur_dsp STATIC
    Util/Smoother.h
    Oscillator/TestSine.h
    Voice/MonoTestVoice.h
    Voice/MonoTestVoice.cpp
)

target_include_directories(augur_dsp PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(augur_dsp PUBLIC cxx_std_20)
target_link_libraries(augur_dsp PRIVATE augur_warnings)
