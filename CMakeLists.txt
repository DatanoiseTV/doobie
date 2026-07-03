cmake_minimum_required(VERSION 3.22)

project(Doobie VERSION 0.21.1 LANGUAGES C CXX)

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

# clap-juce-extensions (vendored recursively under external/clap-juce-extensions;
# bundles the CLAP SDK + helpers as its own nested submodules). Wraps the JUCE
# plugin as a CLAP alongside the AU/VST3/Standalone builds. EXCLUDE_FROM_ALL so
# only the CLAP target we attach below participates in the build.
add_subdirectory(external/clap-juce-extensions EXCLUDE_FROM_ALL)

# Local-install copy is on by default for developer iteration; CI builds set
# -DDOOBIE_INSTALL_LOCAL=OFF so they don't try to write into
# ~/Library/Audio/Plug-Ins (which doesn't exist on Linux runners and is the
# wrong destination for a runner-built artefact anyway).
option(DOOBIE_INSTALL_LOCAL "Copy built plugins into the user's plug-in dirs" ON)

juce_add_plugin(Doobie
    COMPANY_NAME            "DatanoiseTV"
    BUNDLE_ID               "com.datanoisetv.doobie"
    PLUGIN_MANUFACTURER_CODE Dnse
    PLUGIN_CODE             Doob
    FORMATS                 AU VST3 Standalone
    PRODUCT_NAME            "Doobie"
    DESCRIPTION             "Analog dub delay"
    IS_SYNTH                FALSE
    NEEDS_MIDI_INPUT        TRUE
    NEEDS_MIDI_OUTPUT       FALSE
    IS_MIDI_EFFECT          FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    NEEDS_WEB_BROWSER       TRUE   # pulls webkit2gtk + gtk pkg-config on Linux
    COPY_PLUGIN_AFTER_BUILD ${DOOBIE_INSTALL_LOCAL}
    VST3_CATEGORIES         Fx Delay
    # MusicEffect (aumf) is Apple's tag for "audio effect that ALSO takes MIDI"
    # — JUCE would auto-pick this when NEEDS_MIDI_INPUT is TRUE, but a hard
    # AU_MAIN_TYPE setting overrides it. Plain kAudioUnitType_Effect (aufx)
    # promises no MIDI, so Ableton refuses to expose Doobie as a MIDI-routing
    # target even with acceptsMidi() returning true at runtime (auval flags
    # this as a warning). Using aumf fixes the routing while staying
    # functionally an audio effect for hosts that don't care about the tag.
    AU_MAIN_TYPE            kAudioUnitType_MusicEffect)

juce_generate_juce_header(Doobie)

# -----------------------------------------------------------------------------
# CLAP wrapper — builds Doobie.clap from the same shared plugin code. Creates a
# `Doobie_CLAP` target (NOT part of Doobie_All, so build it explicitly). Honours
# COPY_PLUGIN_AFTER_BUILD, so a local build installs it into the user's CLAP dir
# alongside the AU/VST3. CLAP_FEATURES tags the plugin's category for hosts.
# -----------------------------------------------------------------------------
clap_juce_extensions_plugin(TARGET Doobie
    CLAP_ID              "com.datanoisetv.doobie"
    CLAP_FEATURES        audio-effect delay stereo
    CLAP_MANUAL_URL      "https://github.com/DatanoiseTV/doobie"
    CLAP_SUPPORT_URL     "https://github.com/DatanoiseTV/doobie/issues")

# -----------------------------------------------------------------------------
# Voxengo free impulse responses — bundled as JUCE BinaryData so the
# Convolution reverb has 38 real-room IRs out of the box. Created by Aleksey
# Vaneev (Voxengo) and distributed royalty-free under their own license; see
# external/voxengo-irs/license.txt (NOT under Doobie's GPL-3.0). The
# acknowledgment Voxengo's license requires lives in the README.
# -----------------------------------------------------------------------------
file(GLOB DOOBIE_VOXENGO_IRS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/external/voxengo-irs/*.wav"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/openair-irs/*.wav")
# Two BinaryData targets in this build (IRs + UI). Each needs its own
# namespace + header name, otherwise both generate symbols in `BinaryData::`
# and the linker silently picks whichever was linked first — UI lookups
# then hit the IR table and return nothing (404 from the resource provider).
juce_add_binary_data(DoobieIRs
    SOURCES     ${DOOBIE_VOXENGO_IRS}
    NAMESPACE   DoobieIRData
    HEADER_NAME DoobieIRData.h)
set_target_properties(DoobieIRs PROPERTIES POSITION_INDEPENDENT_CODE TRUE)

# -----------------------------------------------------------------------------
# UI assets — index.html / CSS / JS / vendored React + Babel-standalone.
# Served at runtime by WebBrowserComponent's resource provider. All bundled
# as BinaryData so the plugin never reaches the network. The vendored React
# UMDs are MIT-licensed; babel-standalone is also MIT.
# -----------------------------------------------------------------------------
file(GLOB DOOBIE_UI_SRC    CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/ui/src/*")
file(GLOB DOOBIE_UI_VENDOR CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/ui/vendor/*")
juce_add_binary_data(DoobieUI
    SOURCES     ${DOOBIE_UI_SRC} ${DOOBIE_UI_VENDOR}
    NAMESPACE   DoobieUIData
    HEADER_NAME DoobieUIData.h)
set_target_properties(DoobieUI PROPERTIES POSITION_INDEPENDENT_CODE TRUE)

# -----------------------------------------------------------------------------
# Sources
# -----------------------------------------------------------------------------
target_sources(Doobie PRIVATE
    src/PluginProcessor.cpp
    src/PluginEditor.cpp
    src/ui/WebEditor.cpp
    src/dsp/DubDelayEngine.cpp
    src/dsp/FactoryIRs.cpp
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
        JUCE_WEB_BROWSER=1
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_REPORT_APP_USAGE=0
    PRIVATE
        DOOBIE_VERSION="${PROJECT_VERSION}"
        DOOBIE_GIT_HASH="${DOOBIE_GIT_HASH}"
        DOOBIE_GIT_BRANCH="${DOOBIE_GIT_BRANCH}")

target_link_libraries(Doobie PRIVATE
    DoobieIRs
    DoobieUI
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_gui_extra
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

    # Offline preset-demo renderer (bounces the factory presets through a dry
    # source loop to WAVs + a manifest for the landing-page player, no DAW).
    add_executable(doobie_render_demo tools/RenderDemo.cpp)
    target_include_directories(doobie_render_demo PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(doobie_render_demo PRIVATE
        Doobie
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_gui_extra
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

    # Hero-video audio renderer: plays a dry loop through the engine while
    # automating parameters from tools/hero-timeline.json (paired with the
    # UI recorder in tools/record-hero.mjs).
    add_executable(doobie_render_hero tools/RenderHero.cpp)
    target_include_directories(doobie_render_hero PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_link_libraries(doobie_render_hero PRIVATE
        Doobie
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_gui_extra
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
endif()
