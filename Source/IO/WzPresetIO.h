#pragma once

#include <JuceHeader.h>
#include <optional>
#include "../Model/Scene.h"

namespace Worldizer
{
/**
    Reads and writes .wzpreset bundles (see Docs/wzpreset_format.md):

        <id>.wzpreset/
        ├── geometry.json   (scene structure)
        ├── rendered.wav    (baked IR)
        ├── thumbnail.png   (top-down visualisation; placeholder for now)
        └── metadata.json   (descriptive info)

    For Slice 3 a .wzpreset is a directory. Shipped presets are additionally packed
    into a single .wzpkg blob (a simple length-prefixed concatenation of the four
    files) for binary-data embedding — this gives one unique symbol per preset and
    avoids the symbol collisions that embedding the raw files (all named
    geometry.json, etc.) would cause.
*/
class WzPresetIO
{
public:
    /** A loaded preset. The `ir` field is empty for metadata-only loads. */
    struct Loaded
    {
        juce::String      presetId;       // bundle name without extension, e.g. "small_concrete_room"
        juce::String      name;           // human-readable (metadata.json)
        juce::String      category;
        juce::String      description;
        juce::String      author;
        juce::StringArray tags;
        juce::String      ambientBed;             // may be empty
        float             ambientLevelDb = -20.0f; // default bed level when ambientBed set
        juce::String      defaultSourceCharacter; // may be empty
        juce::String      defaultMicCharacter;    // may be empty

        // Diagnostic metadata.
        juce::String      renderedAt;
        int               renderNumRays    = 0;
        int               renderMaxBounces = 0;
        int               renderSampleRate = 0;
        int               renderSeed       = 0;

        Scene                    scene;
        juce::AudioBuffer<float>  ir;
        double                    irSampleRate = 48000.0;
        juce::Image               thumbnail;

        /** Returns a copy with the (large) IR buffer cleared — for lightweight caches. */
        Loaded withoutIR() const
        {
            Loaded copy = *this;
            copy.ir.setSize (0, 0);
            return copy;
        }
    };

    /** Read a full .wzpreset directory (geometry + IR + thumbnail + metadata). */
    static std::optional<Loaded> readBundle (const juce::File& bundleDir, juce::String& errorOut);

    /** Read just metadata.json (no IR/scene/thumbnail) — fast, for browser listing. */
    static std::optional<Loaded> readMetadataOnly (const juce::File& bundleDir, juce::String& errorOut);

    /** Derives a filesystem-safe preset id (lowercase alnum + underscores) from
        a display name — the same scheme Save As uses to name a new .wzpreset
        bundle, shared here so preset rename generates identical ids for
        identical names. Never returns an empty string ("untitled" fallback). */
    static juce::String makeSafeId (const juce::String& displayName);

    /** Rewrites just the `name` field of an existing bundle's metadata.json,
        leaving geometry.json / rendered.wav / thumbnail.png untouched — used by
        preset rename, which only changes the display name (and, separately,
        the bundle directory itself). Returns false with errorOut set if
        metadata.json is missing/invalid or the write fails. */
    static bool renameMetadata (const juce::File& bundleDir, const juce::String& newName, juce::String& errorOut);

    /** Write a .wzpreset directory (creates it; overwrites existing files). */
    static bool writeBundle (const juce::File& bundleDir,
                             const Scene& scene,
                             const juce::AudioBuffer<float>& ir,
                             double irSampleRate,
                             const Loaded& metadata,
                             juce::String& errorOut);

    /** Find all .wzpreset directories directly in `directory`. */
    static juce::Array<juce::File> findPresetsIn (const juce::File& directory);

    /** Pack a .wzpreset directory into a single .wzpkg blob for embedding. */
    static bool packBundle (const juce::File& bundleDir, const juce::File& outPkgFile, juce::String& errorOut);

    /** Read a preset from a .wzpkg blob (the embedded shipped-library format).
        metadataOnly=true skips decoding rendered.wav (used by the scan, which
        discards the IR) — saves decoding every shipped preset's audio at startup. */
    static std::optional<Loaded> readFromBinaryData (const void* data, size_t size, juce::String& errorOut,
                                                     bool metadataOnly = false);

    /** Max rendered.wav size we will load into memory (a user .wzpreset could hold
        a multi-GB WAV; refuse rather than attempt a giant allocation). */
    static constexpr int kMaxIRBytes = 128 * 1024 * 1024;

private:
    static juce::var sceneToJson (const Scene& scene);
    static Scene     sceneFromJson (const juce::var& v, juce::String& errorOut);
    static juce::var metadataToJson (const Loaded& m);
    static void      metadataFromJson (const juce::var& v, Loaded& outMeta);

    static juce::Image makePlaceholderThumbnail (const juce::String& name);
    static bool        writeWavFile (const juce::File& file, const juce::AudioBuffer<float>& ir, double sr, juce::String& errorOut);
};
} // namespace Worldizer
