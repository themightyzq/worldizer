#pragma once

#include <JuceHeader.h>
#include <optional>
#include <vector>
#include "../IO/WzPresetIO.h"

namespace Worldizer
{
/**
    Manages the preset library: shipped presets (embedded as .wzpkg binary data)
    and user presets (.wzpreset directories under the user data folder). Metadata
    is cached after rescan(); the (large) IR is loaded fresh on each loadPreset().

    A user preset with the same id as a shipped preset overrides it.
*/
class PresetManager
{
public:
    PresetManager() = default;
    ~PresetManager() = default;

    /** Scans shipped + user presets, populating the cached metadata list. */
    void rescan();

    /** The user presets folder (pure path; created lazily on save). e.g. on macOS:
        ~/Library/Application Support/ZQ SFX/Worldizer/Presets/ */
    static juce::File getUserPresetsFolder();

    /** Where user presets lived before the company folder was renamed from
        "ZQSFX". The first rescan() copies anything found there across. */
    static juce::File getLegacyUserPresetsFolder();

    juce::StringArray getAvailablePresetIds() const;
    juce::Array<WzPresetIO::Loaded> getAvailablePresetMetadata() const;

    /** Full load (geometry + IR + thumbnail). nullopt if not found / failed. */
    std::optional<WzPresetIO::Loaded> loadPreset (const juce::String& presetId, juce::String& errorOut);

    bool hasPreset (const juce::String& presetId) const;

    /** True if presetId is a USER preset (a .wzpreset directory under the user
        presets folder) rather than an embedded factory preset. Factory presets
        cannot be renamed or deleted. False if presetId is unknown. */
    bool isUserPreset (const juce::String& presetId) const;

    /** Renames a user preset in place: renames its `.wzpreset` directory on disk
        (the directory name IS the preset's id — see PresetEntry::id below) and
        rewrites the `name` field of its metadata.json, then rescans. Message
        thread; does file I/O.

        Validates newDisplayName (non-empty, no path separators, no clash with
        any other preset's display name or the id it would generate —
        case-insensitive) before touching disk. Fails (false + errorOut) for an
        unknown id or a factory preset, without touching disk.

        On success, newIdOut is the renamed preset's new id (== new directory
        name; unchanged from presetId if only the display name's casing/
        cosmetics changed and the generated id is identical). Callers must
        treat this as the preset's new identity — e.g. update a processor's
        currently-loaded-preset id if it was tracking presetId. */
    bool renameUserPreset (const juce::String& presetId, const juce::String& newDisplayName,
                           juce::String& newIdOut, juce::String& errorOut);

private:
    struct PresetEntry
    {
        juce::String id;
        juce::String source;            // "embedded" or an absolute .wzpreset path
        WzPresetIO::Loaded metadata;    // metadata only (no IR)
    };

    void scanShippedPresets();          // assumes `lock` held
    void scanUserPresets();             // assumes `lock` held

    std::vector<PresetEntry> entries;
    mutable juce::CriticalSection lock;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetManager)
};
} // namespace Worldizer
