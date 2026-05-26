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

    /** The user presets folder (created if missing). e.g. on macOS:
        ~/Library/Application Support/ZQSFX/Worldizer/Presets/ */
    static juce::File getUserPresetsFolder();

    juce::StringArray getAvailablePresetIds() const;
    juce::Array<WzPresetIO::Loaded> getAvailablePresetMetadata() const;

    /** Full load (geometry + IR + thumbnail). nullopt if not found / failed. */
    std::optional<WzPresetIO::Loaded> loadPreset (const juce::String& presetId, juce::String& errorOut);

    bool hasPreset (const juce::String& presetId) const;

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
