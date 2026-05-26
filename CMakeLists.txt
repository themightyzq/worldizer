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
        Source/Plugin/RenderThread.h
        Source/Plugin/RenderThread.cpp
        Source/Plugin/PresetManager.h
        Source/Plugin/PresetManager.cpp

        # --- Shared ---
        Source/Shared/Constants.h
        Source/Shared/Vec3.h
        Source/Shared/WorldizerLookAndFeel.h
        Source/Shared/WorldizerLookAndFeel.cpp

        # --- Model ---
        Source/Model/Brush.h
        Source/Model/Brush.cpp
        Source/Model/Material.h
        Source/Model/Material.cpp
        Source/Model/MaterialResolver.h
        Source/Model/MaterialResolver.cpp
        Source/Model/MicNode.h
        Source/Model/MicNode.cpp
        Source/Model/Scene.h
        Source/Model/Scene.cpp
        Source/Model/SourceNode.h
        Source/Model/SourceNode.cpp
        Source/Model/TestScenes.h
        Source/Model/TestScenes.cpp

        # --- DSP (engine: RayTracer/IRBuilder/AirAbsorption implemented; rest stubs) ---
        Source/DSP/AirAbsorption.h
        Source/DSP/AirAbsorption.cpp
        Source/DSP/AmbientBed.h
        Source/DSP/ConvolutionEngine.h
        Source/DSP/ConvolutionEngine.cpp
        Source/DSP/ImageSourceSolver.h
        Source/DSP/IRBuilder.h
        Source/DSP/IRBuilder.cpp
        Source/DSP/MicCharacter.h
        Source/DSP/RayTracer.h
        Source/DSP/RayTracer.cpp
        Source/DSP/SourceCharacter.h

        # --- IO ---
        Source/IO/MapImporter.h
        Source/IO/WzPresetIO.h
        Source/IO/WzPresetIO.cpp

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

# Embedded default IR (small concrete room) for instant audio on cold start.
juce_add_binary_data(WorldizerBinaryData
    HEADER_NAME "WorldizerBinaryData.h"
    NAMESPACE   "WorldizerBinaryData"
    SOURCES
        Resources/Presets/default_ir.wav)

target_link_libraries(Worldizer
    PRIVATE
        WorldizerBinaryData
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_opengl
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# Shipped presets: embed Resources/EmbeddedPresets/*.wzpkg (one packed blob per
# preset) if they exist. Run BakePresets once to create them; CONFIGURE_DEPENDS
# makes the build pick them up automatically. A fresh checkout (no presets yet)
# builds cleanly with WORLDIZER_HAS_SHIPPED_PRESETS=0.
file(GLOB WZ_PRESET_PKGS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Resources/EmbeddedPresets/*.wzpkg")
if(WZ_PRESET_PKGS)
    juce_add_binary_data(WorldizerPresets
        HEADER_NAME "WorldizerPresets.h"
        NAMESPACE   "WorldizerPresets"
        SOURCES     ${WZ_PRESET_PKGS})
    target_link_libraries(Worldizer PRIVATE WorldizerPresets)
    target_compile_definitions(Worldizer PRIVATE WORLDIZER_HAS_SHIPPED_PRESETS=1)
else()
    target_compile_definitions(Worldizer PRIVATE WORLDIZER_HAS_SHIPPED_PRESETS=0)
endif()

# === Slice 3: preset baking CLI tool ===
juce_add_console_app(BakePresets
    PRODUCT_NAME "BakePresets")

juce_generate_juce_header(BakePresets)

target_sources(BakePresets
    PRIVATE
        Tools/bake_presets/bake_presets.cpp
        Source/Model/Material.cpp
        Source/Model/MaterialResolver.cpp
        Source/Model/Brush.cpp
        Source/Model/SourceNode.cpp
        Source/Model/MicNode.cpp
        Source/Model/Scene.cpp
        Source/Model/TestScenes.cpp
        Source/DSP/RayTracer.cpp
        Source/DSP/IRBuilder.cpp
        Source/DSP/AirAbsorption.cpp
        Source/IO/WzPresetIO.cpp
)

target_compile_definitions(BakePresets
    PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
)

target_link_libraries(BakePresets
    PRIVATE
        juce::juce_audio_formats
        juce::juce_audio_basics
        juce::juce_gui_basics   # graphics + ScopedJuceInitialiser_GUI for thumbnail rendering
        juce::juce_dsp
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

# === Slice 1: headless ray-tracer CLI tool ===
# Builds the acoustics engine into a standalone console app for offline IR
# rendering and validation. Uses Worldizer::Vec3 (not juce::Vector3D) so it does
# not need juce_opengl — keeping the engine core JUCE-light and the tool headless.
juce_add_console_app(RenderTestScene
    PRODUCT_NAME "RenderTestScene")

juce_generate_juce_header(RenderTestScene)

target_sources(RenderTestScene
    PRIVATE
        Tests/render_test_scene.cpp
        Source/Model/Material.cpp
        Source/Model/Brush.cpp
        Source/Model/SourceNode.cpp
        Source/Model/MicNode.cpp
        Source/Model/Scene.cpp
        Source/Model/TestScenes.cpp
        Source/DSP/RayTracer.cpp
        Source/DSP/IRBuilder.cpp
        Source/DSP/AirAbsorption.cpp
)

target_compile_definitions(RenderTestScene
    PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
)

target_link_libraries(RenderTestScene
    PRIVATE
        juce::juce_audio_formats
        juce::juce_audio_basics
        juce::juce_core
        juce::juce_dsp
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)
