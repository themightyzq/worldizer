cmake_minimum_required(VERSION 3.22)

# Universal Binary + deployment target for macOS — MUST be set BEFORE project()
if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Build architectures for macOS")
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum macOS deployment target")
endif()

# Keep in sync with Source/Shared/Constants.h kVersionString (this is the version
# JUCE reports to hosts).
project(Worldizer VERSION 0.0.8)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE 8.x as a git submodule under JUCE/
add_subdirectory(JUCE)

# The ZQ SFX house UI (tokens, LookAndFeel, bound controls, embedded OFL fonts). Must come
# AFTER JUCE. Pinned by tag: bump deliberately, then re-run worldizer_ui_snapshot and compare.
include(FetchContent)
FetchContent_Declare(zqsfx_ui
    GIT_REPOSITORY https://github.com/themightyzq/zqsfx_ui.git
    GIT_TAG v0.4.0
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(zqsfx_ui)

# Formats: VST3 + AU + Standalone (recommended for Soundminer; AU for Logic/GarageBand)
set(PLUGIN_FORMATS VST3 AU Standalone)

juce_add_plugin(Worldizer
    # === COMPANY INFO ===
    COMPANY_NAME "ZQ SFX"
    COMPANY_WEBSITE "https://www.zq-sfx.com"
    COMPANY_EMAIL "connect@zq-sfx.com"
    COMPANY_COPYRIGHT "Copyright (c) 2026 ZQ SFX"

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

    # === STANDALONE: request mic access so macOS doesn't silently deny input ===
    # (the Standalone processes live input; without the plist usage string modern
    # macOS blocks the mic and the app appears silent). No effect on the VST3.
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "Worldizer processes live audio input through the worldizing chain."

    # === BUILD SETTINGS ===
    COPY_PLUGIN_AFTER_BUILD TRUE
    FORMATS ${PLUGIN_FORMATS}

    # === DISPLAY INFO ===
    PRODUCT_NAME "Worldizer"
    PLUGIN_NAME "Worldizer"
    PLUGIN_DESCRIPTION "Worldizing: re-record sounds through a speaker into a simulated space"

    # === VST3 CATEGORIES ===
    VST3_CATEGORIES "Fx" "Reverb"
    AU_MAIN_TYPE "kAudioUnitType_Effect"
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
        Source/Plugin/PresetMigration.h
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
        Source/Model/MicArray.h
        Source/Model/MicArray.cpp
        Source/Model/Scene.h
        Source/Model/Scene.cpp
        Source/Model/SourceNode.h
        Source/Model/SourceNode.cpp
        Source/Model/TestScenes.h
        Source/Model/TestScenes.cpp
        Source/Model/Vertex.h
        Source/Model/LineDef.h
        Source/Model/Sector.h
        Source/Model/Sector.cpp
        Source/Model/SectorGeometry.h
        Source/Model/SectorGeometry.cpp

        # --- DSP (ImageSourceSolver is the one remaining deferred stub) ---
        Source/DSP/AirAbsorption.h
        Source/DSP/AirAbsorption.cpp
        Source/DSP/AmbientBed.h
        Source/DSP/AmbientBed.cpp
        Source/DSP/CharacterLibrary.h
        Source/DSP/CharacterLibrary.cpp
        Source/DSP/ConvolutionEngine.h
        Source/DSP/ConvolutionEngine.cpp
        Source/DSP/DistanceModel.h
        Source/DSP/ImageSourceSolver.h
        Source/DSP/IRBuilder.h
        Source/DSP/IRBuilder.cpp
        Source/DSP/MicCharacter.h
        Source/DSP/MicCharacter.cpp
        Source/DSP/RayTracer.h
        Source/DSP/RayTracer.cpp
        Source/DSP/SourceCharacter.h
        Source/DSP/SourceCharacter.cpp

        # --- IO ---
        Source/IO/WzPresetIO.h
        Source/IO/WzPresetIO.cpp

        # --- UI ---
        Source/UI/PresetBrowser.h
        Source/UI/PresetBrowser.cpp
        Source/UI/RoomView2D.h
        Source/UI/RoomView2D.cpp
        Source/UI/EditorToolPalette.h
        Source/UI/EditorToolPalette.cpp
        Source/UI/InspectorPanel.h
        Source/UI/InspectorPanel.cpp
        Source/UI/DrawingState.h
        Source/UI/UndoStack.h
        Source/UI/CharacterPicker.h
        Source/UI/CharacterPicker.cpp
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
        zqsfx::ui
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

# Shipped character IRs + room-tone beds: embed Resources/{Speakers,Mics,RoomTones}
# WAVs if they exist (run BakeAssets once to create them — same optional-embedding
# pattern as the presets above). The spk_/mic_/amb_ filename prefixes keep the
# binary-data symbols collision-free within the single target.
file(GLOB WZ_CHARACTER_WAVS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/Resources/Speakers/*.wav"
    "${CMAKE_CURRENT_SOURCE_DIR}/Resources/Mics/*.wav"
    "${CMAKE_CURRENT_SOURCE_DIR}/Resources/RoomTones/*.wav")
if(WZ_CHARACTER_WAVS)
    juce_add_binary_data(WorldizerCharacters
        HEADER_NAME "WorldizerCharacters.h"
        NAMESPACE   "WorldizerCharacters"
        SOURCES     ${WZ_CHARACTER_WAVS})
    target_link_libraries(Worldizer PRIVATE WorldizerCharacters)
    target_compile_definitions(Worldizer PRIVATE WORLDIZER_HAS_CHARACTERS=1)
else()
    target_compile_definitions(Worldizer PRIVATE WORLDIZER_HAS_CHARACTERS=0)
endif()

# === Slice 5.5 / 6: placeholder asset baking CLI tool (speakers/mics/room tones) ===
juce_add_console_app(BakeAssets PRODUCT_NAME "BakeAssets")
juce_generate_juce_header(BakeAssets)
target_sources(BakeAssets
    PRIVATE
        Tools/bake_assets/bake_assets.cpp
        Source/DSP/CharacterLibrary.cpp
        Source/Model/Material.cpp
        Source/Model/MaterialResolver.cpp
)
target_compile_definitions(BakeAssets PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(BakeAssets
    PRIVATE
        juce::juce_audio_formats
        juce::juce_audio_basics
        juce::juce_core
        juce::juce_dsp
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

# === Slice 3: preset baking CLI tool ===
juce_add_console_app(BakePresets
    PRODUCT_NAME "BakePresets")

juce_generate_juce_header(BakePresets)

target_sources(BakePresets
    PRIVATE
        Tools/bake_presets/bake_presets.cpp
        Source/Model/LibraryScenes.cpp
        Source/Model/Material.cpp
        Source/Model/Brush.cpp
        Source/Model/SourceNode.cpp
        Source/Model/MicNode.cpp
        Source/Model/MicArray.cpp
        Source/Model/Sector.cpp
        Source/Model/SectorGeometry.cpp
        Source/Model/MaterialResolver.cpp
        Source/Model/Scene.cpp
        Source/Model/TestScenes.cpp
        Source/DSP/RayTracer.cpp
        Source/DSP/IRBuilder.cpp
        Source/DSP/AirAbsorption.cpp
        Source/IO/WzPresetIO.cpp
        Source/UI/RoomView2D.cpp
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
        Source/Model/MicArray.cpp
        Source/Model/Sector.cpp
        Source/Model/SectorGeometry.cpp
        Source/Model/MaterialResolver.cpp
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

# === Slice 4: verifies moving the mic produces a different IR ===
juce_add_console_app(MicMoveTest PRODUCT_NAME "MicMoveTest")
juce_generate_juce_header(MicMoveTest)
target_sources(MicMoveTest
    PRIVATE
        Tests/mic_move_test.cpp
        Source/Model/Material.cpp
        Source/Model/Brush.cpp
        Source/Model/SourceNode.cpp
        Source/Model/MicNode.cpp
        Source/Model/MicArray.cpp
        Source/Model/Sector.cpp
        Source/Model/SectorGeometry.cpp
        Source/Model/MaterialResolver.cpp
        Source/Model/Scene.cpp
        Source/Model/TestScenes.cpp
        Source/DSP/RayTracer.cpp
        Source/DSP/IRBuilder.cpp
        Source/DSP/AirAbsorption.cpp
)
target_compile_definitions(MicMoveTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(MicMoveTest
    PRIVATE
        juce::juce_audio_formats
        juce::juce_audio_basics
        juce::juce_core
        juce::juce_dsp
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

# === Slice 4.5: validates the DistanceModel curves (header-only model) ===
juce_add_console_app(DistanceTest PRODUCT_NAME "DistanceTest")
juce_generate_juce_header(DistanceTest)
target_sources(DistanceTest PRIVATE Tests/distance_test.cpp)
target_compile_definitions(DistanceTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(DistanceTest
    PRIVATE
        juce::juce_audio_basics
        juce::juce_core
        juce::juce_dsp
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

# === Slice 4.5: IR analysis / regression tool (render + analyze) ===
juce_add_console_app(IRInspect PRODUCT_NAME "IRInspect")
juce_generate_juce_header(IRInspect)
target_sources(IRInspect
    PRIVATE
        Tools/ir_inspect/ir_inspect.cpp
        Source/Model/Material.cpp
        Source/Model/Brush.cpp
        Source/Model/SourceNode.cpp
        Source/Model/MicNode.cpp
        Source/Model/MicArray.cpp
        Source/Model/Sector.cpp
        Source/Model/SectorGeometry.cpp
        Source/Model/MaterialResolver.cpp
        Source/Model/Scene.cpp
        Source/Model/TestScenes.cpp
        Source/DSP/RayTracer.cpp
        Source/DSP/IRBuilder.cpp
        Source/DSP/AirAbsorption.cpp
)
target_compile_definitions(IRInspect PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(IRInspect
    PRIVATE
        juce::juce_audio_formats
        juce::juce_audio_basics
        juce::juce_core
        juce::juce_dsp
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

# === Slice 5: directional mic pattern verification ===
juce_add_console_app(DirectionalityTest PRODUCT_NAME "DirectionalityTest")
juce_generate_juce_header(DirectionalityTest)
target_sources(DirectionalityTest
    PRIVATE
        Tests/directionality_test.cpp
        Source/Model/Material.cpp
        Source/Model/Brush.cpp
        Source/Model/SourceNode.cpp
        Source/Model/MicNode.cpp
        Source/Model/MicArray.cpp
        Source/Model/Sector.cpp
        Source/Model/SectorGeometry.cpp
        Source/Model/MaterialResolver.cpp
        Source/Model/Scene.cpp
        Source/Model/TestScenes.cpp
        Source/DSP/RayTracer.cpp
        Source/DSP/IRBuilder.cpp
        Source/DSP/AirAbsorption.cpp
)
target_compile_definitions(DirectionalityTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(DirectionalityTest
    PRIVATE
        juce::juce_audio_formats
        juce::juce_audio_basics
        juce::juce_core
        juce::juce_dsp
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

# === Slice 6a: sector geometry compiler verification ===
juce_add_console_app(SectorCompilerTest PRODUCT_NAME "SectorCompilerTest")
juce_generate_juce_header(SectorCompilerTest)
target_sources(SectorCompilerTest
    PRIVATE
        Tests/sector_compiler_test.cpp
        Source/Model/Material.cpp
        Source/Model/Brush.cpp
        Source/Model/SourceNode.cpp
        Source/Model/MicNode.cpp
        Source/Model/MicArray.cpp
        Source/Model/Sector.cpp
        Source/Model/SectorGeometry.cpp
        Source/Model/MaterialResolver.cpp
        Source/Model/Scene.cpp
        Source/Model/TestScenes.cpp
        Source/DSP/RayTracer.cpp
        Source/DSP/IRBuilder.cpp
        Source/DSP/AirAbsorption.cpp
)
target_compile_definitions(SectorCompilerTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(SectorCompilerTest
    PRIVATE
        juce::juce_audio_formats
        juce::juce_audio_basics
        juce::juce_core
        juce::juce_dsp
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

# === Slice 6a: oriented-wall intersection regression ===
juce_add_console_app(OrientedWallIntersectionTest PRODUCT_NAME "OrientedWallIntersectionTest")
juce_generate_juce_header(OrientedWallIntersectionTest)
target_sources(OrientedWallIntersectionTest
    PRIVATE
        Tests/oriented_wall_intersection_test.cpp
        Source/Model/Material.cpp
        Source/Model/Brush.cpp
)
target_compile_definitions(OrientedWallIntersectionTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(OrientedWallIntersectionTest
    PRIVATE
        juce::juce_core
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

# === Slice 5: stereo / multi-mic IR verification ===
juce_add_console_app(StereoIRTest PRODUCT_NAME "StereoIRTest")
juce_generate_juce_header(StereoIRTest)
target_sources(StereoIRTest
    PRIVATE
        Tests/stereo_ir_test.cpp
        Source/Model/Material.cpp
        Source/Model/Brush.cpp
        Source/Model/SourceNode.cpp
        Source/Model/MicNode.cpp
        Source/Model/MicArray.cpp
        Source/Model/Sector.cpp
        Source/Model/SectorGeometry.cpp
        Source/Model/MaterialResolver.cpp
        Source/Model/Scene.cpp
        Source/Model/TestScenes.cpp
        Source/DSP/RayTracer.cpp
        Source/DSP/IRBuilder.cpp
        Source/DSP/AirAbsorption.cpp
)
target_compile_definitions(StereoIRTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(StereoIRTest
    PRIVATE
        juce::juce_audio_formats
        juce::juce_audio_basics
        juce::juce_core
        juce::juce_dsp
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

# === Room-shell -> sector conversion verification ===
juce_add_console_app(ShellConvertTest PRODUCT_NAME "ShellConvertTest")
juce_generate_juce_header(ShellConvertTest)
target_sources(ShellConvertTest
    PRIVATE
        Tests/shell_convert_test.cpp
        Source/Model/Material.cpp
        Source/Model/Brush.cpp
        Source/Model/SourceNode.cpp
        Source/Model/MicNode.cpp
        Source/Model/MicArray.cpp
        Source/Model/Sector.cpp
        Source/Model/SectorGeometry.cpp
        Source/Model/MaterialResolver.cpp
        Source/Model/Scene.cpp
        Source/Model/TestScenes.cpp
        Source/Model/LibraryScenes.cpp
        Source/DSP/RayTracer.cpp
        Source/DSP/IRBuilder.cpp
        Source/DSP/AirAbsorption.cpp
)
target_compile_definitions(ShellConvertTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(ShellConvertTest
    PRIVATE
        juce::juce_audio_formats
        juce::juce_audio_basics
        juce::juce_core
        juce::juce_dsp
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

# === Slice 5.5: character library spectral verification ===
juce_add_console_app(CharacterTest PRODUCT_NAME "CharacterTest")
juce_generate_juce_header(CharacterTest)
target_sources(CharacterTest
    PRIVATE
        Tests/character_test.cpp
        Source/DSP/CharacterLibrary.cpp
        Source/DSP/ConvolutionEngine.cpp
)
target_compile_definitions(CharacterTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(CharacterTest
    PRIVATE
        juce::juce_audio_formats
        juce::juce_audio_basics
        juce::juce_core
        juce::juce_dsp
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

# === Slice 6: ambient bed loop/swap verification ===
juce_add_console_app(AmbientBedTest PRODUCT_NAME "AmbientBedTest")
juce_generate_juce_header(AmbientBedTest)
target_sources(AmbientBedTest
    PRIVATE
        Tests/ambient_bed_test.cpp
        Source/DSP/AmbientBed.cpp
)
target_compile_definitions(AmbientBedTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(AmbientBedTest
    PRIVATE
        juce::juce_audio_basics
        juce::juce_core
        juce::juce_dsp
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

# === User-preset migration from the legacy "ZQSFX" company folder ===
juce_add_console_app(PresetMigrationTest PRODUCT_NAME "PresetMigrationTest")
juce_generate_juce_header(PresetMigrationTest)
target_sources(PresetMigrationTest PRIVATE Tests/preset_migration_test.cpp)
target_compile_definitions(PresetMigrationTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(PresetMigrationTest
    PRIVATE
        juce::juce_core
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

# === UI snapshot tool (headless before/after render gate for the zqsfx_ui migration) ===
# pamplejuce pattern: link against the plugin's own shared-code target ("Worldizer", produced
# by juce_add_plugin above) instead of recompiling PluginProcessor.cpp/PluginEditor.cpp a second
# time. juce_generate_juce_header(Worldizer)'s generated JuceHeader.h is only added to
# Worldizer's OWN include path as PRIVATE (JUCEUtils.cmake), so it does not propagate through
# target_link_libraries; it is added back explicitly below via JUCE_GENERATED_SOURCES_DIRECTORY.
juce_add_console_app(worldizer_ui_snapshot PRODUCT_NAME "worldizer_ui_snapshot")
target_sources(worldizer_ui_snapshot PRIVATE Tools/ui_snapshot/main.cpp)
target_include_directories(worldizer_ui_snapshot PRIVATE
    Source
    $<TARGET_PROPERTY:Worldizer,JUCE_GENERATED_SOURCES_DIRECTORY>)
target_link_libraries(worldizer_ui_snapshot
    PRIVATE
        Worldizer
        WorldizerBinaryData
        zqsfx::ui
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_opengl
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
target_compile_definitions(worldizer_ui_snapshot PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)

# === Test registration (CTest) ===
# Wire the assertion-based console tests into CTest so `ctest` (locally and in CI)
# runs them. Each returns 0 on pass / nonzero on fail (e.g. `return ok ? 0 : 1`).
# RenderTestScene / IRInspect / BakePresets are tools, not pass/fail tests, so they
# are intentionally excluded. Run locally with: ctest --test-dir build -C Release
enable_testing()
foreach(wz_test
        OrientedWallIntersectionTest
        SectorCompilerTest
        DistanceTest
        MicMoveTest
        DirectionalityTest
        StereoIRTest
        CharacterTest
        AmbientBedTest
        ShellConvertTest
        PresetMigrationTest)
    add_test(NAME ${wz_test} COMMAND ${wz_test})
endforeach()

