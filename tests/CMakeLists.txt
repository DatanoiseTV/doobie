# Lightweight DSP unit tests. These exercise the JUCE-free cores (delay line,
# saturation, reverbs) so they build and run quickly without the framework.
add_executable(doobie_tests
    test_dsp.cpp
    ${CMAKE_SOURCE_DIR}/src/dsp/PlateReverb.cpp
    ${CMAKE_SOURCE_DIR}/src/dsp/SpringReverb.cpp
    ${CMAKE_SOURCE_DIR}/src/dsp/FdnReverb.cpp)
# ShimmerReverb is intentionally excluded: it uses the FFT pitch shifter, which
# pulls in JUCE, whereas these tests stay framework-free. Shimmer is covered by
# the FdnReverb (its core) test plus the full-plugin auval render.

target_include_directories(doobie_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_features(doobie_tests PRIVATE cxx_std_20)

add_test(NAME dsp COMMAND doobie_tests)

# State / migration tests. These need the full plugin (parameter tree, state
# serialisation), so they link the shared code rather than staying framework-free.
add_executable(doobie_state_tests test_state.cpp)
target_include_directories(doobie_state_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_features(doobie_state_tests PRIVATE cxx_std_20)
target_link_libraries(doobie_state_tests PRIVATE
    Doobie
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)

add_test(NAME state COMMAND doobie_state_tests)
