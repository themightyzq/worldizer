#pragma once

#include <JuceHeader.h>
#include "../Shared/Constants.h"

namespace wz
{
/**
    A surface material: frequency-dependent absorption plus a scattering
    coefficient. The canonical material library lives in
    Resources/Materials/materials.json (see Docs/materials_reference.md).

    Slice 1 fills in band centre frequencies and JSON (de)serialisation.
*/
struct Material
{
    juce::String name;

    /** Per-band absorption, 0.0 (perfectly reflective) .. 1.0 (perfectly absorptive). */
    std::array<float, (size_t) Worldizer::kIRBands> absorption {};

    /** Scattering: 0.0 = fully specular, 1.0 = fully diffuse. */
    float scattering = 0.0f;
};
} // namespace wz
