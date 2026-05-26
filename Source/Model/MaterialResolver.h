#pragma once

#include <JuceHeader.h>
#include "Material.h"

namespace Worldizer
{
/**
    Resolves a material name string (as stored in geometry.json) to a Material.

    Slice 3: a hardcoded map over the Material::*() factories. Slice 9 replaces this
    with a JSON-loaded measured-material library. Unknown names resolve to
    Material::drywall() with a logged warning, so a preset never fails to load.
*/
class MaterialResolver
{
public:
    static Material resolve (const juce::String& name);
    static juce::StringArray getKnownNames();
};
} // namespace Worldizer
