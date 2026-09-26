add_executable(augur_render main.cpp WavWriter.h)
target_link_libraries(augur_render PRIVATE augur_dsp augur_warnings)
