#pragma once

#include <JuceHeader.h>
#include <vector>

namespace Worldizer
{
/**
    The registry of shipped source (speaker) characters, mic characters, and
    ambient room-tone beds. Each entry maps a stable id to a display name and a
    baked WAV asset (Resources/Speakers|Mics|RoomTones/<id>.wav, embedded as
    binary data by CMake when present — the same optional-embedding pattern as
    the shipped presets).

    The shipped WAVs are SYNTHESIZED placeholders (see Tools/bake_assets) with
    plausible filter-chain characters; Slice 9 replaces them with real recordings
    of the same ids. The registry is code-defined so the bake tool, the plugin,
    and the tests share one source of truth.

    Index 0 of the speaker and mic lists is always the "none" (bypass) entry,
    which has no WAV. Room tones have no "none" entry — a preset simply omits
    its ambient_bed field.
*/
struct CharacterDef
{
    const char* id;           // filename stem, binary-data symbol stem, metadata.json value
    const char* name;         // UI display name
    const char* description;  // UI tooltip / browser subtitle
};

class CharacterLibrary
{
public:
    static const std::vector<CharacterDef>& speakers();   // [0] = none
    static const std::vector<CharacterDef>& mics();       // [0] = none
    static const std::vector<CharacterDef>& roomTones();  // no none entry

    static juce::StringArray speakerNames();
    static juce::StringArray micNames();

    /** Index into the list for an id; -1 if unknown. */
    static int speakerIndexForId (const juce::String& id);
    static int micIndexForId (const juce::String& id);
    static int roomToneIndexForId (const juce::String& id);

    /** Loads the baked IR / bed for an id from the embedded binary data.
        Returns false for "none", an unknown id, or a build without embedded
        character data (WORLDIZER_HAS_CHARACTERS=0). */
    static bool loadSpeakerIR (const juce::String& id, juce::AudioBuffer<float>& out, double& sampleRateOut);
    static bool loadMicIR     (const juce::String& id, juce::AudioBuffer<float>& out, double& sampleRateOut);
    static bool loadRoomTone  (const juce::String& id, juce::AudioBuffer<float>& out, double& sampleRateOut);

private:
    static bool loadEmbeddedWav (const juce::String& resourceStem,
                                 juce::AudioBuffer<float>& out, double& sampleRateOut);
};
} // namespace Worldizer
