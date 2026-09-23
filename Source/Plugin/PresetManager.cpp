#include "PresetManager.h"
#include "PresetMigration.h"
#include "../Model/MaterialResolver.h"
#include <algorithm>
#include <mutex>

#ifndef WORLDIZER_HAS_SHIPPED_PRESETS
 #define WORLDIZER_HAS_SHIPPED_PRESETS 0
#endif

#if WORLDIZER_HAS_SHIPPED_PRESETS
 #include "WorldizerPresets.h"
#endif

namespace Worldizer
{
juce::File PresetManager::getUserPresetsFolder()
{
    auto base = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory);
   #if JUCE_MAC
    // On macOS userApplicationDataDirectory is ~/Library; the convention is
    // ~/Library/Application Support. (Windows %APPDATA% / Linux ~/.config are correct as-is.)
    base = base.getChildFile ("Application Support");
   #endif

    // Pure path — does NOT create the directory. Creating it here made the
    // browser's 1 Hz poll do a filesystem WRITE every second (pathological on a
    // slow/network mount). The folder is created lazily by Save-As before writing.
    return base.getChildFile ("ZQ SFX").getChildFile ("Worldizer").getChildFile ("Presets");
}

juce::File PresetManager::getLegacyUserPresetsFolder()
{
    // Company folder used before the "ZQ SFX" rename. Same parent, so derive it
    // from the current path rather than repeating the per-platform base logic.
    return getUserPresetsFolder().getParentDirectory().getParentDirectory()
             .getSiblingFile ("ZQSFX").getChildFile ("Worldizer").getChildFile ("Presets");
}

void PresetManager::rescan()
{
    // Once per process, before the first user scan. Lives here rather than in
    // getUserPresetsFolder(), which must stay a pure path (it is polled at 1 Hz).
    static std::once_flag migrateOnce;
    std::call_once (migrateOnce, []
    {
        PresetMigration::migrateLegacyUserPresets (getLegacyUserPresetsFolder(), getUserPresetsFolder());
    });

    const juce::ScopedLock sl (lock);
    entries.clear();
    scanShippedPresets();
    scanUserPresets();

    std::sort (entries.begin(), entries.end(),
               [] (const PresetEntry& a, const PresetEntry& b)
               { return a.metadata.name.compareIgnoreCase (b.metadata.name) < 0; });
}

void PresetManager::scanShippedPresets()
{
   #if WORLDIZER_HAS_SHIPPED_PRESETS
    for (int i = 0; i < WorldizerPresets::namedResourceListSize; ++i)
    {
        const juce::String fname (WorldizerPresets::originalFilenames[i]);
        if (! fname.endsWithIgnoreCase (".wzpkg"))
            continue;

        int size = 0;
        const char* d = WorldizerPresets::getNamedResource (WorldizerPresets::namedResourceList[i], size);
        if (d == nullptr || size <= 0)
            continue;

        juce::String err;
        // metadataOnly: the scan discards the IR anyway (withoutIR below). Skipping
        // the WAV decode keeps cold start well under the 500 ms Soundminer budget
        // even with the full shipped library embedded.
        auto loaded = WzPresetIO::readFromBinaryData (d, (size_t) size, err, /*metadataOnly*/ true);
        if (! loaded.has_value())
        {
            juce::Logger::writeToLog ("Shipped preset failed: " + fname + " — " + err);
            continue;
        }

        PresetEntry e;
        e.id = fname.dropLastCharacters ((int) juce::String (".wzpkg").length());
        e.source = "embedded";
        loaded->presetId = e.id;
        if (loaded->name.isEmpty()) loaded->name = e.id;
        e.metadata = loaded->withoutIR();
        entries.push_back (std::move (e));
    }
   #endif
}

void PresetManager::scanUserPresets()
{
    const auto folder = getUserPresetsFolder();

    juce::Array<juce::File> dirs = WzPresetIO::findPresetsIn (folder);
    // One level of category subfolders.
    for (auto& sub : folder.findChildFiles (juce::File::findDirectories, false))
        if (! sub.getFileName().endsWithIgnoreCase (".wzpreset"))
            for (auto& p : WzPresetIO::findPresetsIn (sub))
                dirs.add (p);

    for (auto& d : dirs)
    {
        juce::String err;
        auto meta = WzPresetIO::readMetadataOnly (d, err);
        if (! meta.has_value())
        {
            juce::Logger::writeToLog ("User preset metadata failed: " + d.getFullPathName() + " — " + err);
            continue;
        }

        const juce::String id = d.getFileNameWithoutExtension();
        meta->presetId = id;
        if (meta->name.isEmpty()) meta->name = id;

        PresetEntry e { id, d.getFullPathName(), meta->withoutIR() };

        auto it = std::find_if (entries.begin(), entries.end(),
                                [&] (const PresetEntry& x) { return x.id == id; });
        if (it != entries.end())
        {
            juce::Logger::writeToLog ("User preset overrides shipped: " + id);
            *it = std::move (e);
        }
        else
        {
            entries.push_back (std::move (e));
        }
    }
}

juce::StringArray PresetManager::getAvailablePresetIds() const
{
    const juce::ScopedLock sl (lock);
    juce::StringArray ids;
    for (const auto& e : entries)
        ids.add (e.id);
    return ids;
}

juce::Array<WzPresetIO::Loaded> PresetManager::getAvailablePresetMetadata() const
{
    const juce::ScopedLock sl (lock);
    juce::Array<WzPresetIO::Loaded> out;
    for (const auto& e : entries)
        out.add (e.metadata);
    return out;
}

bool PresetManager::hasPreset (const juce::String& presetId) const
{
    const juce::ScopedLock sl (lock);
    for (const auto& e : entries)
        if (e.id == presetId)
            return true;
    return false;
}

bool PresetManager::isUserPreset (const juce::String& presetId) const
{
    const juce::ScopedLock sl (lock);
    for (const auto& e : entries)
        if (e.id == presetId)
            return e.source != "embedded";
    return false;
}

bool PresetManager::renameUserPreset (const juce::String& presetId, const juce::String& newDisplayName,
                                      juce::String& newIdOut, juce::String& errorOut)
{
    const auto trimmedName = newDisplayName.trim();
    if (trimmedName.isEmpty())
    {
        errorOut = "Name cannot be empty.";
        return false;
    }
    if (trimmedName.containsAnyOf ("/\\"))
    {
        errorOut = "Name cannot contain path separators.";
        return false;
    }

    const auto newId = WzPresetIO::makeSafeId (trimmedName);

    juce::File bundleDir;
    {
        const juce::ScopedLock sl (lock);
        auto it = std::find_if (entries.begin(), entries.end(),
                                [&] (const PresetEntry& e) { return e.id == presetId; });
        if (it == entries.end())
        {
            errorOut = "Preset not found: " + presetId;
            return false;
        }
        if (it->source == "embedded")
        {
            errorOut = "Factory presets cannot be renamed.";
            return false;
        }

        // Clash check against every OTHER preset (factory included): both by
        // display name (case-insensitive, what the user actually typed) and by
        // the id the new name would generate (two different display names can
        // still collide once sanitised to an id).
        for (const auto& e : entries)
        {
            if (e.id == presetId)
                continue;
            if (e.metadata.name.equalsIgnoreCase (trimmedName) || e.id == newId)
            {
                errorOut = "A preset named \"" + trimmedName + "\" already exists.";
                return false;
            }
        }

        bundleDir = juce::File (it->source);
    }

    if (! bundleDir.isDirectory())
    {
        errorOut = "Preset folder not found on disk: " + bundleDir.getFullPathName();
        return false;
    }

    // Rename the directory FIRST (it's the preset's identity everywhere else —
    // PresetManager::hasPreset/loadPreset, the processor's currentPresetId — so
    // this is the real point of no return); only then rewrite metadata.json in
    // its new location. If the id is unchanged (a purely cosmetic name edit,
    // e.g. capitalisation) there's nothing to move.
    auto targetDir = bundleDir;
    if (newId != presetId)
    {
        targetDir = bundleDir.getSiblingFile (newId + ".wzpreset");
        if (targetDir.exists())
        {
            errorOut = "A preset folder named \"" + newId + "\" already exists.";
            return false;
        }
        if (! bundleDir.moveFileTo (targetDir))
        {
            errorOut = "Could not rename the preset folder on disk.";
            return false;
        }
    }

    juce::String metaErr;
    if (! WzPresetIO::renameMetadata (targetDir, trimmedName, metaErr))
    {
        // The directory move (if any) already succeeded, so the id DID change —
        // report the new id so the caller's bookkeeping stays consistent even
        // though the display name inside metadata.json is now stale.
        errorOut = "Renamed the preset folder but could not update its metadata: " + metaErr;
        newIdOut = newId;
        rescan();
        return false;
    }

    newIdOut = newId;
    rescan();
    return true;
}

std::optional<WzPresetIO::Loaded> PresetManager::loadPreset (const juce::String& presetId, juce::String& errorOut)
{
    juce::String source;
    {
        const juce::ScopedLock sl (lock);
        bool found = false;
        for (const auto& e : entries)
            if (e.id == presetId) { source = e.source; found = true; break; }
        if (! found) { errorOut = "preset not found: " + presetId; return {}; }
    }

    if (source == "embedded")
    {
       #if WORLDIZER_HAS_SHIPPED_PRESETS
        const juce::String target = presetId + ".wzpkg";
        for (int i = 0; i < WorldizerPresets::namedResourceListSize; ++i)
        {
            if (target == WorldizerPresets::originalFilenames[i])
            {
                int size = 0;
                const char* d = WorldizerPresets::getNamedResource (WorldizerPresets::namedResourceList[i], size);
                if (d != nullptr)
                {
                    auto loaded = WzPresetIO::readFromBinaryData (d, (size_t) size, errorOut);
                    if (loaded.has_value()) loaded->presetId = presetId;
                    return loaded;
                }
            }
        }
       #endif
        errorOut = "embedded preset data not found: " + presetId;
        return {};
    }

    auto loaded = WzPresetIO::readBundle (juce::File (source), errorOut);
    if (loaded.has_value()) loaded->presetId = presetId;
    return loaded;
}
} // namespace Worldizer
