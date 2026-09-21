/*
    PresetMigrationTest — validates the one-time copy of user presets from the
    legacy "ZQSFX" company folder into "ZQ SFX" (Source/Plugin/PresetMigration.h).

    Runs entirely inside a temp directory; never touches the real preset folders.
*/
#include <JuceHeader.h>
#include <iostream>

#include "../Source/Plugin/PresetMigration.h"

using namespace Worldizer;

namespace
{
    bool pass = true;

    void check (bool condition, const char* what)
    {
        std::cout << (condition ? "  ok    " : "  FAIL  ") << what << "\n";
        pass = pass && condition;
    }

    juce::File makePreset (const juce::File& parent, const juce::String& name, const juce::String& body)
    {
        auto bundle = parent.getChildFile (name + ".wzpreset");
        bundle.createDirectory();
        bundle.getChildFile ("metadata.json").replaceWithText (body);
        return bundle;
    }

    juce::String metadataOf (const juce::File& parent, const juce::String& name)
    {
        return parent.getChildFile (name + ".wzpreset").getChildFile ("metadata.json").loadFileAsString();
    }
}

int main()
{
    auto root = juce::File::getSpecialLocation (juce::File::tempDirectory)
                    .getChildFile ("wz_preset_migration_test_" + juce::String (juce::Random::getSystemRandom().nextInt (1 << 30)));
    root.deleteRecursively();
    const auto legacy = root.getChildFile ("ZQSFX/Worldizer/Presets");
    const auto fresh  = root.getChildFile ("ZQ SFX/Worldizer/Presets");

    std::cout << "No legacy folder:\n";
    check (PresetMigration::migrateLegacyUserPresets (legacy, fresh) == 0, "copies nothing");
    check (! fresh.exists(), "does not create the new folder (no writes on a fresh install)");

    std::cout << "Legacy folder with presets, one name collision, one shared category:\n";
    legacy.createDirectory();
    makePreset (legacy, "Cathedral", "legacy-cathedral");
    makePreset (legacy, "Stairwell", "legacy-stairwell");
    makePreset (legacy.getChildFile ("Caves"), "Grotto", "legacy-grotto");
    makePreset (fresh, "Stairwell", "new-stairwell");
    makePreset (fresh.getChildFile ("Caves"), "Cavern", "new-cavern");

    check (PresetMigration::migrateLegacyUserPresets (legacy, fresh) == 2, "copies the 2 missing presets");
    check (metadataOf (fresh, "Cathedral") == "legacy-cathedral", "top-level preset copied intact");
    check (metadataOf (fresh, "Stairwell") == "new-stairwell", "existing preset in new folder is not overwritten");
    check (metadataOf (fresh.getChildFile ("Caves"), "Grotto") == "legacy-grotto", "preset merged into an existing category");
    check (metadataOf (fresh.getChildFile ("Caves"), "Cavern") == "new-cavern", "category's own preset untouched");
    check (metadataOf (legacy, "Cathedral") == "legacy-cathedral", "legacy folder left untouched");
    check (fresh.getChildFile (PresetMigration::kMarkerFileName).existsAsFile(), "marker written");

    std::cout << "Second run:\n";
    fresh.getChildFile ("Cathedral.wzpreset").deleteRecursively();
    check (PresetMigration::migrateLegacyUserPresets (legacy, fresh) == 0, "is a no-op");
    check (! fresh.getChildFile ("Cathedral.wzpreset").exists(), "a preset the user deleted is not resurrected");

    root.deleteRecursively();
    std::cout << (pass ? "PASS\n" : "FAIL\n");
    return pass ? 0 : 1;
}
