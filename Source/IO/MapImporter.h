#pragma once

#include <JuceHeader.h>
#include "../Model/Scene.h"

namespace Worldizer
{
/**
    Imports TrenchBroom-authored Quake .map files. Parses brush geometry, maps
    texture names to materials, and recognises entity types worldizer_source /
    worldizer_mic (with sensible fallback defaults). Produces a Scene ready for
    ray tracing; the caller then bakes it and saves a .wzpreset.

    The companion TrenchBroom config (Resources/TrenchBroom/) defines the
    Worldizer texture palette and entity types. Slice 8 implements the parser.
*/
class MapImporter
{
public:
    struct Options
    {
        juce::File textureToMaterialMap; // optional override table
    };

    /** Parse a .map file into a Scene. Returns false on parse failure. */
    static bool importMap (const juce::File& mapFile, Scene& out, const Options& options = {});
};
} // namespace Worldizer
