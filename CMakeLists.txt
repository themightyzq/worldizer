cmake_minimum_required(VERSION 3.22)

# Universal Binary + deployment target for macOS — MUST be set BEFORE project()
if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Build architectures for macOS")
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum macOS deployment target")
endif()

project(Worldizer VERSION 0.0.1)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE 8.x as a git submodule under JUCE/
add_subdirectory(JUCE)

# Formats: VST3 + Standalone (recommended for Soundminer; AU omitted by design)
set(PLUGIN_FORMATS VST3 Standalone)

juce_add_plugin(Worldizer
    # === COMPANY INFO ===
    COMPANY_NAME "ZQSFX"
    COMPANY_WEBSITE "https://github.com/zqsfx/worldizer"
    COMPANY_EMAIL "connect@zq-sfx.com"

    # === UNIQUE IDENTIFIERS (locked in — see Source/Shared/Constants.h) ===
    PLUGIN_MANUFACTURER_CODE ZQSF
    PLUGIN_CODE Wrld
    BUNDLE_ID "com.zqsfx.worldizer"

    # === PLUGIN TYPE ===
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE

    # === UI SETTINGS ===
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE

    # === BUILD SETTINGS ===
    COPY_PLUGIN_AFTER_BUILD TRUE
    FORMATS ${PLUGIN_FORMATS}

    # === DISPLAY INFO ===
    PRODUCT_NAME "Worldizer"
    PLUGIN_NAME "Worldizer"
    PLUGIN_DESCRIPTION "Worldizing: re-record sounds through a speaker into a simulated space"

    # === VST3 CATEGORIES ===
    VST3_CATEGORIES "Fx" "Reverb"
)

# Source uses <JuceHeader.h>
juce_generate_juce_header(Worldizer)

target_sources(Worldizer
    PRIVATE
        # --- Plugin (compiled) ---
        Source/Plugin/PluginProcessor.cpp
        Source/Plugin/PluginEditor.cpp
        Source/Plugin/PluginProcessor.h
        Source/Plugin/PluginEditor.h

        # --- Shared ---
        Source/Shared/Constants.h
        Source/Shared/WorldizerLookAndFeel.h

        # --- Model (stubs) ---
        Source/Model/Brush.h
        Source/Model/Material.h
        Source/Model/MicNode.h
        Source/Model/Scene.h
        Source/Model/SourceNode.h

        # --- DSP (stubs) ---
        Source/DSP/AirAbsorption.h
        Source/DSP/AmbientBed.h
        Source/DSP/ConvolutionEngine.h
        Source/DSP/ImageSourceSolver.h
        Source/DSP/IRBuilder.h
        Source/DSP/MicCharacter.h
        Source/DSP/RayTracer.h
        Source/DSP/SourceCharacter.h

        # --- IO (stubs) ---
        Source/IO/MapImporter.h
        Source/IO/WzPresetIO.h

        # --- UI (stubs) ---
        Source/UI/PresetBrowser.h
        Source/UI/RoomView2D.h
)

target_include_directories(Worldizer
    PRIVATE
        Source
)

target_compile_definitions(Worldizer
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_REPORT_APP_USAGE=0
)

if(APPLE)
    target_compile_definitions(Worldizer PUBLIC
        JUCE_JACK=0
        JUCE_ALSA=0
    )
endif()

target_link_libraries(Worldizer
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_opengl
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
