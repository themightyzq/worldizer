#pragma once

#include <JuceHeader.h>
#include "../Model/Scene.h"

namespace Worldizer
{
/**
    Reads and writes .wzpreset bundles (see Docs/architecture.md §4):

        preset_name.wzpreset/
        ├── geometry.json     (brushes, materials, default source/mic positions)
        ├── rendered.wav      (pre-baked IR)
        ├── thumbnail.png     (top-down visualisation)
        └── metadata.json     (name, category, description, author, ...)

    Browse mode only reads rendered.wav + metadata.json (instant load). Edit mode
    loads everything. Slice 3 implements the bundle I/O.
*/
class WzPresetIO
{
public:
    struct Metadata
    {
        juce::String name, category, description, author;
        juce::String ambientBed, defaultSourceCharacter, defaultMicCharacter;
        juce::StringArray tags;
    };

    struct Preset
    {
        Scene scene;
        Metadata metadata;
        juce::AudioBuffer<float> renderedIR;
        double irSampleRate = 0.0;
    };

    /** Load a full preset bundle from disk (edit mode). */
    static bool load (const juce::File& bundle, Preset& out);

    /** Fast path: read only metadata.json + rendered.wav (browse mode). */
    static bool loadForBrowse (const juce::File& bundle,
                               Metadata& metaOut,
                               juce::AudioBuffer<float>& irOut,
                               double& irSampleRateOut);

    /** Write a preset bundle to disk. */
    static bool save (const juce::File& bundle, const Preset& preset);
};
} // namespace Worldizer
