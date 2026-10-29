cmake_minimum_required(VERSION 3.22)

project(Vespera VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")

# -----------------------------------------------------------------------------
# JUCE
#
# By default JUCE is fetched into the build tree, so the project is fully
# self-contained. To reuse an existing checkout (and skip the download),
# configure with -DVESPERA_JUCE_DIR=/path/to/JUCE.
# -----------------------------------------------------------------------------
set(VESPERA_JUCE_DIR "" CACHE PATH "Optional path to a local JUCE checkout")

if (VESPERA_JUCE_DIR)
    add_subdirectory(${VESPERA_JUCE_DIR} juce)
else()
    include(FetchContent)

    FetchContent_Declare(
        JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.6
        GIT_SHALLOW    TRUE
    )

    FetchContent_MakeAvailable(JUCE)
endif()

# -----------------------------------------------------------------------------
# Plugin target
# -----------------------------------------------------------------------------
juce_add_plugin(Vespera
    COMPANY_NAME              "Starshinov"
    PLUGIN_MANUFACTURER_CODE  Strv
    PLUGIN_CODE               Vspa
    FORMATS                   VST3 AU Standalone
    PRODUCT_NAME              "Vespera"
    BUNDLE_ID                 "com.starshinov.vespera"
    DESCRIPTION               "Melody idea generator for sound design"
    IS_SYNTH                  TRUE
    NEEDS_MIDI_INPUT          TRUE
    NEEDS_MIDI_OUTPUT         FALSE
    IS_MIDI_EFFECT            FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD   TRUE
    PLUGIN_IS_RTAS            FALSE
)

target_sources(Vespera PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/DebugLogger.cpp
    Source/ui/VesperaLookAndFeel.cpp
    Source/ui/PatternDisplay.cpp
)

target_include_directories(Vespera PRIVATE Source)

target_compile_definitions(Vespera PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1
)

target_link_libraries(Vespera
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# -----------------------------------------------------------------------------
# Offline test helpers
# -----------------------------------------------------------------------------
function(vespera_add_dsp_test name source)
    juce_add_console_app(${name} PRODUCT_NAME ${name})
    target_sources(${name} PRIVATE ${source})
    target_link_libraries(${name}
        PRIVATE
            juce::juce_audio_basics
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags
    )
endfunction()

function(vespera_add_plugin_test name source)
    juce_add_console_app(${name} PRODUCT_NAME ${name})
    target_sources(${name} PRIVATE
        ${source}
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/DebugLogger.cpp
        Source/ui/VesperaLookAndFeel.cpp
        Source/ui/PatternDisplay.cpp
    )
    target_include_directories(${name} PRIVATE Source)
    target_compile_definitions(${name} PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_STRICT_REFCOUNTEDPOINTER=1
        JucePlugin_Name="Vespera"
        # Keeps the black-box log out of the real per-user file.
        VESPERA_TEST_BUILD=1
    )
    target_link_libraries(${name}
        PRIVATE
            juce::juce_audio_utils
            juce::juce_dsp
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags
    )
endfunction()

# --- DSP / engine (header-only, fast to build) --------------------------------
vespera_add_dsp_test(VesperaGeneratorTest Tests/GeneratorTest.cpp)
vespera_add_dsp_test(VesperaSynthTest     Tests/SynthTest.cpp)

# --- full plugin (real processor + editor) ------------------------------------
vespera_add_plugin_test(VesperaPluginTest Tests/PluginTest.cpp)
