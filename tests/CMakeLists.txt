add_executable(augur_tests
    TestHelpers.h
    test_smoother.cpp
    test_vco.cpp
    test_filter_env.cpp
    test_engine.cpp
    test_arp.cpp
)
target_link_libraries(augur_tests PRIVATE augur_dsp augur_warnings Catch2::Catch2WithMain)

list(APPEND CMAKE_MODULE_PATH ${Catch2_SOURCE_DIR}/extras)
include(Catch)
catch_discover_tests(augur_tests)
