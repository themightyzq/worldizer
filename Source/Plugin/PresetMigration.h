#pragma once

#include <JuceHeader.h>

namespace Worldizer
{
/**
    One-time copy of user presets from the pre-rename company folder
    (".../ZQSFX/Worldizer/Presets") into the current one (".../ZQ SFX/...").

    Header-only and juce_core-only so PresetMigrationTest can exercise it
    against temp directories without linking the preset stack.
*/
namespace PresetMigration
{
    /** Dropped into the new folder once migration has run, so a preset the user
        later deletes there is not resurrected from the legacy folder. */
    constexpr auto kMarkerFileName = ".migrated-from-ZQSFX";

    namespace detail
    {
        /** Copies the children of `from` that are missing in `to`. A .wzpreset
            bundle is copied whole; any other directory is a category folder and
            is merged, so presets land beside ones already saved in a same-named
            category. Returns false if any copy failed. */
        inline bool copyMissing (const juce::File& from, const juce::File& to, int& copied)
        {
            bool allOk = true;
            for (const auto& entry : from.findChildFiles (juce::File::findFilesAndDirectories, false))
            {
                const auto target = to.getChildFile (entry.getFileName());
                const bool isCategory = entry.isDirectory()
                                        && ! entry.getFileName().endsWithIgnoreCase (".wzpreset");

                if (isCategory && target.isDirectory())
                {
                    allOk = copyMissing (entry, target, copied) && allOk;
                    continue;
                }

                if (target.exists())
                    continue;   // same-named preset already in the new folder wins

                const bool ok = entry.isDirectory() ? entry.copyDirectoryTo (target)
                                                    : entry.copyFileTo (target);
                if (ok)
                {
                    ++copied;
                }
                else
                {
                    allOk = false;
                    juce::Logger::writeToLog ("Preset migration failed: " + entry.getFullPathName());
                }
            }
            return allOk;
        }
    }

    /** Copies everything in legacyDir that does not already exist in newDir.
        Copies, never moves: the legacy folder is left untouched so an older
        build keeps working. Returns the number of entries copied.

        Does nothing (and writes nothing) when legacyDir is absent, which is the
        normal case and keeps a fresh install free of filesystem writes. */
    inline int migrateLegacyUserPresets (const juce::File& legacyDir, const juce::File& newDir)
    {
        if (! legacyDir.isDirectory())
            return 0;

        const auto marker = newDir.getChildFile (kMarkerFileName);
        if (marker.existsAsFile())
            return 0;

        if (! newDir.createDirectory().wasOk())
            return 0;

        int copied = 0;
        const bool allOk = detail::copyMissing (legacyDir, newDir, copied);

        // No marker on a partial copy, so the next launch retries what failed.
        if (allOk)
            marker.replaceWithText ("Presets copied from " + legacyDir.getFullPathName() + "\n");

        return copied;
    }
}
} // namespace Worldizer
