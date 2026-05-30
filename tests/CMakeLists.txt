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

# State / migration tests. Linking `Doobie` cascades JUCE's INTERFACE_SOURCES
# (incl. juce_gui_extra.cpp) into the test target. On macOS that's free --
# WKWebView is SDK-provided. On Linux it forces the test to build with
# webkit2gtk + a generated subprocess-helper header that only the plug-in's
# juceaide step produces. The state-migration logic is platform-independent,
# so we run the test on Apple + Windows hosts only and trust macOS CI to
# catch any regressions.
if(APPLE OR WIN32)
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
endif()
