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
