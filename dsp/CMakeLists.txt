add_library(augur_dsp STATIC
    Util/Smoother.h
    Util/Random.h
    Util/FastMath.h
    Util/Denormals.h
    Util/Halfband.h
    Util/Simd4.h
    Analog/Drift.h
    Analog/Fingerprint.h
    Analog/Autotune.h
    Mixer/OtaMixer.h
    Oscillator/BlepTable.h
    Oscillator/BlepTable.cpp
    Oscillator/BlepRing.h
    Oscillator/Cem3340.h
    Oscillator/Cem3340.cpp
    Oscillator/Ssm2030.h
    Oscillator/Ssm2030.cpp
    Oscillator/Noise.h
    Filter/LadderFilter.h
    Filter/LadderFilter.cpp
    Envelope/RcAdsr.h
    Modulation/Lfo.h
    Effects/Chorus.h
    Effects/Chorus.cpp
    Effects/TapeDelay.h
    Effects/TapeDelay.cpp
    Effects/Reverb.h
    Effects/Reverb.cpp
    Voice/SynthVoice.h
    Voice/SynthVoice.cpp
    Engine/SynthParams.h
    Engine/ChunkSignals.h
    Engine/SynthEngine.h
    Engine/SynthEngine.cpp
)

target_include_directories(augur_dsp PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(augur_dsp PUBLIC cxx_std_20)
target_link_libraries(augur_dsp PRIVATE augur_warnings)

