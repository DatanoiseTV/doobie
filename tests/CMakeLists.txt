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

# Linking Doobie pulls in juce_gui_extra (whose source is rebuilt per
# consumer, JUCE-module style). With JUCE_WEB_BROWSER=1 that source needs
# gtk + webkit headers on Linux. JUCE's pkg-config target propagates from
# the plugin target's NEEDS_WEB_BROWSER, but doesn't reach the test
# target through INTERFACE_LINK_LIBRARIES -- so wire it up here explicitly
# for non-Apple / non-Windows hosts. On macOS WKWebView is the SDK; no
# pkg-config dance needed.
if(UNIX AND NOT APPLE)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(DOOBIE_WEBKIT_TEST REQUIRED IMPORTED_TARGET
                      gtk+-x11-3.0 webkit2gtk-4.0)
    target_link_libraries(doobie_state_tests PRIVATE PkgConfig::DOOBIE_WEBKIT_TEST)
endif()

add_test(NAME state COMMAND doobie_state_tests)
