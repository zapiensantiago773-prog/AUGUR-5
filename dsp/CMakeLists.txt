add_library(augur_dsp STATIC
    Util/Smoother.h
    Util/Random.h
    Util/FastMath.h
    Util/Denormals.h
    Util/Halfband.h
    Util/HalfbandFir.h
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
    Modulation/Arpeggiator.h
    Modulation/Arpeggiator.cpp
    Modulation/Lfo.h
    Effects/Fuzz.h
    Effects/Fuzz.cpp
    Effects/SpringReverb.h
    Effects/SpringReverb.cpp
    Effects/TapeDelay.h
    Rack/FxCommon.h
    Rack/RackFastMath.h
    Rack/RackRng.h
    Rack/Drive.h
    Rack/Drive.cpp
    Rack/Chorus.h
    Rack/Chorus.cpp
    Rack/Phaser.h
    Rack/Phaser.cpp
    Rack/Flanger.h
    Rack/Flanger.cpp
    Rack/TapeEcho.h
    Rack/TapeEcho.cpp
    Rack/Reverb.h
    Rack/Reverb.cpp
    Rack/BusComp.h
    Rack/BusComp.cpp
    Rack/FxRack.h
    Rack/FxRack.cpp
    Effects/TapeDelay.cpp
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

