cmake_minimum_required(VERSION 3.22)

# Static MSVC runtime (/MT): the VST3 has no dependency on the VC++ redistributable.
# Must be set before the first project()/target is created.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" CACHE STRING "" FORCE)

project(PepelSpace VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

option(PEPEL_BUILD_TOOLS      "Build DspTest and Snapshot (disables LTO)" OFF)
option(PEPEL_BUILD_STANDALONE "Also build the Standalone application"      OFF)
option(PEPEL_COPY_AFTER_BUILD "Copy the plugin into the system plugin folder after build" OFF)

include(FetchContent)
set(FETCHCONTENT_QUIET OFF)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.12
    GIT_SHALLOW    ON)
FetchContent_MakeAvailable(JUCE)

# ---- fonts: three replaceable slots (see Assets/README.md) ----
# Names in BinaryData come from file names: FontTitle.ttf -> BinaryData::FontTitle_ttf
juce_add_binary_data(PepelFonts SOURCES
    Assets/FontTitle.ttf
    Assets/FontText.ttf
    Assets/FontKanji.ttf)
set_target_properties(PepelFonts PROPERTIES POSITION_INDEPENDENT_CODE ON)

set(PEPEL_FORMATS VST3)
if(PEPEL_BUILD_STANDALONE)
    list(APPEND PEPEL_FORMATS Standalone)
endif()

juce_add_plugin(PepelSpace
    COMPANY_NAME                "PEPEL"
    PLUGIN_MANUFACTURER_CODE    Pepl
    PLUGIN_CODE                 Pspc
    FORMATS                     ${PEPEL_FORMATS}
    PRODUCT_NAME                "PEPEL SPACE"
    VST3_CATEGORIES             Fx Spatial
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    COPY_PLUGIN_AFTER_BUILD     ${PEPEL_COPY_AFTER_BUILD}
    BUNDLE_ID                   com.pepel.space)

target_sources(PepelSpace PRIVATE
    Source/PluginProcessor.cpp Source/PluginProcessor.h
    Source/PluginEditor.cpp    Source/PluginEditor.h
    Source/PepelDsp.h          Source/Theme.h)

target_compile_definitions(PepelSpace PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(PepelSpace
    PRIVATE
        PepelFonts
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

if(NOT PEPEL_BUILD_TOOLS)
    target_link_libraries(PepelSpace PUBLIC juce::juce_recommended_lto_flags)
endif()

# ---- developer tools (LTO stays off) ----
if(PEPEL_BUILD_TOOLS)
    # DSP check without a DAW: only the header-only engine + juce_audio_basics
    juce_add_console_app(DspTest PRODUCT_NAME "DspTest")
    target_sources(DspTest PRIVATE Tools/DspTest.cpp)
    target_compile_definitions(DspTest PRIVATE JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0)
    target_include_directories(DspTest PRIVATE Source)
    target_link_libraries(DspTest PRIVATE juce::juce_audio_basics juce::juce_recommended_config_flags)

    # Renders the real editor to PNG via createComponentSnapshot
    juce_add_console_app(Snapshot PRODUCT_NAME "Snapshot")
    target_sources(Snapshot PRIVATE Tools/Snapshot.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp)
    target_compile_definitions(Snapshot PRIVATE JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0)
    target_include_directories(Snapshot PRIVATE Source)
    target_link_libraries(Snapshot PRIVATE PepelFonts juce::juce_audio_utils juce::juce_recommended_config_flags)
endif()
