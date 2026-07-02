/*
    BakePresets - renders the shipped preset library (5 Test scenes + the 20
    Slice 9 library scenes) as .wzpreset bundles (canonical, in
    Resources/Presets/) plus a packed .wzpkg per preset (for binary embedding,
    in Resources/EmbeddedPresets/).

    Usage: BakePresets [outputDir]   (default outputDir: ./Resources/Presets)
*/

#include <JuceHeader.h>
#include <iostream>
#include <vector>

#include "../../Source/Model/TestScenes.h"
#include "../../Source/Model/LibraryScenes.h"
#include "../../Source/DSP/RayTracer.h"
#include "../../Source/DSP/IRBuilder.h"
#include "../../Source/IO/WzPresetIO.h"
#include "../../Source/UI/RoomView2D.h"

using namespace Worldizer;

namespace
{
    struct Info
    {
        const char* scene;
        const char* id;
        const char* name;
        const char* category;
        const char* description;
        juce::StringArray tags;

        // Slice 5.5 / 6 defaults (empty = none). Beds/characters reference
        // CharacterLibrary ids; applied when the USER selects the preset.
        const char* ambientBed      = "";
        float       ambientLevelDb  = -20.0f;
        const char* sourceCharacter = "";
        const char* micCharacter    = "";
        bool        isLibraryScene  = false;   // LibraryScenes::byName vs TestScenes::byName
    };
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI guiInit; // graphics/fonts for thumbnail rendering

    const juce::File outputDir = (argc > 1)
        ? juce::File (juce::File::getCurrentWorkingDirectory().getChildFile (juce::String (argv[1])).getFullPathName())
        : juce::File::getCurrentWorkingDirectory().getChildFile ("Resources/Presets");
    const juce::File embedDir = outputDir.getParentDirectory().getChildFile ("EmbeddedPresets");

    outputDir.createDirectory();
    embedDir.createDirectory();

    const std::vector<Info> infos = {
        // === Test (Slices 1-5; beds added in Slice 6) ===
        { "anechoic", "anechoic", "Anechoic", "Test",
          "A single absorbent ground plane - essentially dry. Direct sound only; used to verify the dry path.",
          { "test", "dry", "reference" } },
        { "smallConcreteRoom", "small_concrete_room", "Small Concrete Room", "Test",
          "A 6x4x3 m room with a concrete floor and drywall walls/ceiling. Tight, dense early reflections; ~0.55 s decay.",
          { "test", "small", "indoor", "concrete", "reflective" }, "amb_small_room", -26.0f },
        { "hallway", "hallway", "Hallway", "Test",
          "A 20x2x2.5 m corridor with a wood floor and drywall walls. Flutter echo between parallel walls; ~1.1 s decay.",
          { "test", "hallway", "indoor", "corridor", "flutter" }, "amb_fluorescent", -28.0f },
        { "forestClearing", "forest_clearing", "Forest Clearing", "Test",
          "An outdoor clearing: gravel ground, scattered trees, no ceiling. Sparse, distant-sounding reflections.",
          { "test", "outdoor", "forest", "sparse", "open" }, "amb_outdoor_air", -22.0f },
        { "gymnasium", "gymnasium", "Gymnasium", "Test",
          "A 30x20x8 m hall with a concrete floor and drywall walls/ceiling. Long, dark ~4.8 s decay that darkens as it fades.",
          { "test", "large", "indoor", "gymnasium", "long" }, "amb_room_hvac", -24.0f },

        // === Indoor ===
        { "stairwell", "stairwell", "Stairwell", "Indoor",
          "A 12 m concrete shaft with landings. Stacked vertical slap and a hard institutional decay.",
          { "indoor", "concrete", "vertical", "slap" }, "amb_small_room", -26.0f, "", "", true },
        { "parkingGarage", "parking_garage", "Parking Garage", "Indoor",
          "A 40x30 m slab under a 2.6 m ceiling with columns. Boomy, smeared, claustrophobically wide.",
          { "indoor", "concrete", "large", "boomy" }, "amb_room_hvac", -22.0f, "", "", true },
        { "tiledBathroom", "tiled_bathroom", "Tiled Bathroom", "Indoor",
          "A tiny tile box with a porcelain tub. Bright, splashy ring - the classic bathroom sound.",
          { "indoor", "tile", "small", "bright" }, "amb_small_room", -30.0f, "", "", true },
        { "bedroom", "bedroom", "Bedroom", "Indoor",
          "A carpeted domestic room with a curtained wall and a bed. Short, soft, intimate decay.",
          { "indoor", "domestic", "small", "soft" }, "amb_small_room", -28.0f, "", "", true },
        { "cathedral", "cathedral", "Cathedral", "Indoor",
          "A 44 m stone nave under a 20 m vault with marble columns. Seconds of slowly blooming wash.",
          { "indoor", "stone", "huge", "long", "sacred" }, "amb_small_room", -30.0f, "", "", true },
        { "warehouse", "warehouse", "Warehouse", "Indoor",
          "A 45x28x9 m metal hall broken up by crate stacks. Big, ringy, industrially untidy.",
          { "indoor", "metal", "large", "industrial" }, "amb_room_hvac", -24.0f, "", "", true },
        { "closet", "closet", "Closet", "Indoor",
          "A clothes-lined booth. Nearly dead, uncomfortably close - a natural vocal booth.",
          { "indoor", "tiny", "dead", "intimate" }, "amb_small_room", -34.0f, "", "", true },
        { "kitchen", "kitchen", "Kitchen", "Indoor",
          "Tile floor, counter runs, and a fridge. Small, clattery, domestic with a hard edge.",
          { "indoor", "tile", "domestic", "small" }, "amb_fluorescent", -26.0f, "", "", true },

        // === Outdoor ===
        { "backAlley", "back_alley", "Back Alley", "Outdoor",
          "Two 12 m brick facades four metres apart over asphalt. Tight urban slap with open ends.",
          { "outdoor", "urban", "slap", "narrow" }, "amb_city_rumble", -22.0f, "", "", true },
        { "courtyard", "courtyard", "Courtyard", "Outdoor",
          "A walled 14 m court open to the sky: grass, a fountain, plaster walls. Contained but airy.",
          { "outdoor", "walled", "open-sky", "airy" }, "amb_outdoor_air", -24.0f, "", "", true },
        { "canyon", "canyon", "Canyon", "Outdoor",
          "Two colossal rock faces 26 m apart over gravel. Discrete, far-apart echoes under open sky.",
          { "outdoor", "rock", "echo", "huge" }, "amb_outdoor_air", -20.0f, "", "", true },
        { "streetTunnel", "street_tunnel", "Street Tunnel", "Outdoor",
          "A 36 m concrete tube open at both ends. Directional boom that vanishes out the exits.",
          { "outdoor", "tunnel", "concrete", "boom" }, "amb_city_rumble", -20.0f, "", "", true },

        // === Vehicles & Devices ===
        { "carInterior", "car_interior", "Car Interior", "Vehicles & Devices",
          "A padded, glass-sided cabin. Boxy closeness with almost no tail - pure early reflections.",
          { "vehicle", "cabin", "tiny", "boxy" }, "amb_city_rumble", -26.0f, "spk_car_speaker", "", true },
        { "cargoVan", "cargo_van", "Cargo Van", "Vehicles & Devices",
          "An empty steel cargo box on wheels. Ringy metallic clatter around a wood floor.",
          { "vehicle", "metal", "ringy" }, "amb_city_rumble", -24.0f, "", "", true },

        // === Cinematic ===
        { "bunker", "bunker", "Bunker", "Cinematic",
          "A low concrete cell behind a metal bulkhead. Oppressive close slap; nowhere for sound to go.",
          { "cinematic", "concrete", "low", "oppressive" }, "amb_room_hvac", -26.0f, "", "mic_dynamic_stage", true },
        { "cave", "cave", "Cave", "Cinematic",
          "An irregular rock chamber with a boulder. Diffuse, direction-less rumble and soft echo.",
          { "cinematic", "rock", "diffuse", "dark" }, "amb_small_room", -30.0f, "", "", true },
        { "grainSilo", "grain_silo", "Grain Silo", "Cinematic",
          "A 16 m metal shaft. A singing vertical ring that hangs high above the source.",
          { "cinematic", "metal", "vertical", "ring" }, "amb_small_room", -30.0f, "", "", true },

        // === Experimental ===
        { "infiniteCorridor", "infinite_corridor", "Infinite Corridor", "Experimental",
          "A 64 m glass hallway. Endless flutter that stops feeling like a place at all.",
          { "experimental", "flutter", "glass", "unreal" }, "amb_tape_hiss", -28.0f, "spk_intercom", "", true },
        { "resonantTank", "resonant_tank", "Resonant Tank", "Experimental",
          "A sealed all-metal cube. A brutal metallic ring - worldizing as sound design weapon.",
          { "experimental", "metal", "resonant", "aggressive" }, "amb_tape_hiss", -30.0f, "", "mic_contact", true },
        { "deadRoom", "dead_room", "Dead Room", "Experimental",
          "Curtains over carpet under acoustic tile. Drier than outdoors, yet unmistakably a room.",
          { "experimental", "dead", "dry", "booth" }, "amb_tape_hiss", -34.0f, "", "", true },
    };

    const int rays = 100000, bounces = 32, seed = 42;
    int failures = 0;

    for (const auto& info : infos)
    {
        bool ok = false;
        auto scene = info.isLibraryScene ? LibraryScenes::byName (info.scene, ok)
                                         : TestScenes::byName (info.scene, ok);
        if (! ok) { std::cerr << "Unknown scene: " << info.scene << "\n"; ++failures; continue; }

        RayTracer tracer;
        RayTracer::Settings rt;
        rt.numRays = rays; rt.maxBounces = bounces; rt.randomSeed = seed;
        const auto result = tracer.trace (scene, rt, 48000);

        IRBuilder builder;
        IRBuilder::Settings irSettings;
        irSettings.sampleRate = 48000;
        const auto ir = builder.build (result, irSettings);

        WzPresetIO::Loaded meta;
        meta.presetId    = info.id;
        meta.name        = info.name;
        meta.category    = info.category;
        meta.description = info.description;
        meta.author      = "ZQSFX";
        meta.tags        = info.tags;
        meta.ambientBed             = info.ambientBed;
        meta.ambientLevelDb         = info.ambientLevelDb;
        meta.defaultSourceCharacter = info.sourceCharacter;
        meta.defaultMicCharacter    = info.micCharacter;
        meta.renderNumRays    = rays;
        meta.renderMaxBounces = bounces;
        meta.renderSampleRate = 48000;
        meta.renderSeed       = seed;
        meta.renderedAt = juce::Time::getCurrentTime().toISO8601 (true);
        meta.thumbnail  = renderSceneThumbnail (scene, 128, 128); // real top-down geometry render

        const auto bundleDir = outputDir.getChildFile (juce::String (info.id) + ".wzpreset");
        juce::String err;
        if (! WzPresetIO::writeBundle (bundleDir, scene, ir, 48000.0, meta, err))
        {
            std::cerr << "Write failed for " << info.id << ": " << err << "\n";
            ++failures; continue;
        }

        const auto pkg = embedDir.getChildFile (juce::String (info.id) + ".wzpkg");
        if (! WzPresetIO::packBundle (bundleDir, pkg, err))
        {
            std::cerr << "Pack failed for " << info.id << ": " << err << "\n";
            ++failures; continue;
        }

        std::cout << "Wrote " << bundleDir.getFullPathName() << "  (+ EmbeddedPresets/" << pkg.getFileName() << ")\n";
    }

    std::cout << (failures == 0 ? "All presets baked." : juce::String (failures) + " preset(s) failed.").toRawUTF8() << "\n";
    return failures == 0 ? 0 : 1;
}
