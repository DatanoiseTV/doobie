cmake_minimum_required(VERSION 3.22)

project(Doobie VERSION 0.4.1 LANGUAGES C CXX)

# -----------------------------------------------------------------------------
# Project-wide settings
# -----------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Universal binary on Apple silicon + Intel for distributable plugins. Can be
# overridden on the command line (e.g. -DCMAKE_OSX_ARCHITECTURES=arm64) for fast
# local iteration builds.
if(APPLE)
    if(NOT CMAKE_OSX_ARCHITECTURES)
        set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Build architectures")
    endif()
    if(NOT CMAKE_OSX_DEPLOYMENT_TARGET)
        set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
    endif()
endif()

# -----------------------------------------------------------------------------
# Version / build identity baked into the binary (see CLAUDE.md house rules:
# branch + commit hash shown in the app's startup/help).
# -----------------------------------------------------------------------------
find_package(Git QUIET)
set(DOOBIE_GIT_HASH "unknown")
set(DOOBIE_GIT_BRANCH "unknown")
if(GIT_FOUND)
    execute_process(COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        OUTPUT_VARIABLE DOOBIE_GIT_HASH OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET)
    execute_process(COMMAND ${GIT_EXECUTABLE} rev-parse --abbrev-ref HEAD
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        OUTPUT_VARIABLE DOOBIE_GIT_BRANCH OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET)
endif()

# -----------------------------------------------------------------------------
# JUCE (vendored as a git submodule under external/JUCE)
# -----------------------------------------------------------------------------
add_subdirectory(external/JUCE)

juce_add_plugin(Doobie
    COMPANY_NAME            "DatanoiseTV"
    BUNDLE_ID               "com.datanoisetv.doobie"
    PLUGIN_MANUFACTURER_CODE Dnse
    PLUGIN_CODE             Doob
    FORMATS                 AU VST3 Standalone
    PRODUCT_NAME            "Doobie"
    DESCRIPTION             "Analog dub delay"
    IS_SYNTH                FALSE
    NEEDS_MIDI_INPUT        FALSE
    NEEDS_MIDI_OUTPUT       FALSE
    IS_MIDI_EFFECT          FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    VST3_CATEGORIES         Fx Delay
    AU_MAIN_TYPE            kAudioUnitType_Effect)

juce_generate_juce_header(Doobie)

# -----------------------------------------------------------------------------
# Sources
# -----------------------------------------------------------------------------
target_sources(Doobie PRIVATE
    src/PluginProcessor.cpp
    src/PluginEditor.cpp
    src/dsp/DubDelayEngine.cpp
    src/dsp/SpringReverb.cpp
    src/dsp/PlateReverb.cpp
    src/dsp/FdnReverb.cpp
    src/dsp/ShimmerReverb.cpp
    src/dsp/FftPitchShifter.cpp
    src/presets/PresetManager.cpp
    src/ui/LookAndFeel.cpp
    src/ui/VuMeter.cpp
    src/ui/EchoVisualiser.cpp
    src/ui/ReverbResponseView.cpp)

target_include_directories(Doobie PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_compile_definitions(Doobie
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_REPORT_APP_USAGE=0
    PRIVATE
        DOOBIE_VERSION="${PROJECT_VERSION}"
        DOOBIE_GIT_HASH="${DOOBIE_GIT_HASH}"
        DOOBIE_GIT_BRANCH="${DOOBIE_GIT_BRANCH}")

target_link_libraries(Doobie PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)

# -----------------------------------------------------------------------------
# Unit tests (TDD for the DSP cores). Built only when this is the top project.
# -----------------------------------------------------------------------------
if(PROJECT_IS_TOP_LEVEL)
    enable_testing()
    add_subdirectory(tests)

    # Offline UI snapshot tool (renders the editor to a PNG, no DAW needed).
    add_executable(doobie_snapshot tools/Snapshot.cpp)
    target_include_directories(doobie_snapshot PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(doobie_snapshot PRIVATE
        Doobie
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_gui_extra
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
endif()
