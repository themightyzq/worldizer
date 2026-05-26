/*
    BakePresets — renders the five hardcoded test scenes as .wzpreset bundles
    (canonical, in Resources/Presets/) plus a packed .wzpkg per preset (for binary
    embedding, in Resources/EmbeddedPresets/).

    Usage: BakePresets [outputDir]   (default outputDir: ./Resources/Presets)
*/

#include <JuceHeader.h>
#include <iostream>
#include <vector>

#include "../../Source/Model/TestScenes.h"
#include "../../Source/DSP/RayTracer.h"
#include "../../Source/DSP/IRBuilder.h"
#include "../../Source/IO/WzPresetIO.h"

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
        { "anechoic", "anechoic", "Anechoic", "Test",
          "A single absorbent ground plane — essentially dry. Direct sound only; used to verify the dry path.",
          { "test", "dry", "reference" } },
        { "smallConcreteRoom", "small_concrete_room", "Small Concrete Room", "Test",
          "A 6x4x3 m room with a concrete floor and drywall walls/ceiling. Tight, dense early reflections; ~0.55 s decay.",
          { "test", "small", "indoor", "concrete", "reflective" } },
        { "hallway", "hallway", "Hallway", "Test",
          "A 20x2x2.5 m corridor with a wood floor and drywall walls. Flutter echo between parallel walls; ~1.1 s decay.",
          { "test", "hallway", "indoor", "corridor", "flutter" } },
        { "forestClearing", "forest_clearing", "Forest Clearing", "Test",
          "An outdoor clearing: gravel ground, scattered trees, no ceiling. Sparse, distant-sounding reflections.",
          { "test", "outdoor", "forest", "sparse", "open" } },
        { "gymnasium", "gymnasium", "Gymnasium", "Test",
          "A 30x20x8 m hall with a concrete floor and drywall walls/ceiling. Long, dark ~2.6 s decay that darkens as it fades.",
          { "test", "large", "indoor", "gymnasium", "long" } },
    };

    const int rays = 100000, bounces = 32, seed = 42;
    int failures = 0;

    for (const auto& info : infos)
    {
        bool ok = false;
        auto scene = TestScenes::byName (info.scene, ok);
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
        meta.renderNumRays    = rays;
        meta.renderMaxBounces = bounces;
        meta.renderSampleRate = 48000;
        meta.renderSeed       = seed;
        meta.renderedAt = juce::Time::getCurrentTime().toISO8601 (true);

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
