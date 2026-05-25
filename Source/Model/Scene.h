#pragma once

#include <JuceHeader.h>
#include "Brush.h"
#include "SourceNode.h"
#include "MicNode.h"

namespace wz
{
/**
    Top-level geometry container: brushes, sources, mic(s), ambient bed
    reference, and room bounds. Serialises to/from the geometry.json portion of
    a .wzpreset bundle (see Docs/architecture.md §4 and WzPresetIO).

    Slice 1/3 implement the geometry math and JSON (de)serialisation.
*/
struct Scene
{
    struct Bounds
    {
        juce::Vector3D<float> min {}; // metres
        juce::Vector3D<float> max {}; // metres
    };

    Bounds bounds;
    std::vector<Brush>      brushes;
    std::vector<SourceNode> sources;
    std::vector<MicNode>    mics;
    juce::String            ambientBed; // room-tone file reference (see metadata.json)

    /** Serialise to a juce::var matching the geometry.json schema. */
    juce::var toVar() const;

    /** Parse from a juce::var produced by reading geometry.json. */
    static Scene fromVar (const juce::var& v);
};
} // namespace wz
