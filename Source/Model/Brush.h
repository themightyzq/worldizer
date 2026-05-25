#pragma once

#include <JuceHeader.h>

namespace wz
{
/**
    The atomic unit of scene geometry: a convex volume with a per-brush material.
    MVP brushes are axis-aligned boxes (min/max corners in metres). Later slices
    may generalise to arbitrary convex hulls (the .map importer produces these).

    Additive brushes are solid and contribute reflections; subtractive brushes
    carve negative space out of additive brushes.
*/
struct Brush
{
    enum class Kind { additive, subtractive };

    juce::String id;
    Kind kind = Kind::additive;

    juce::Vector3D<float> min {}; // metres
    juce::Vector3D<float> max {}; // metres

    juce::String material;        // material id (see Material / materials.json)
};
} // namespace wz
