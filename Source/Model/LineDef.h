#pragma once

#include <JuceHeader.h>

namespace Worldizer
{
/**
    A wall segment between two vertices in a `Sector`'s perimeter.

    A linedef has up to two sides:
      - **Front** (right of walking v1 -> v2) — required; faces into the sector that
        owns this linedef.
      - **Back**  — optional, used when this linedef separates two sectors. Two-sided
        linedefs arrive in Slice 6b alongside multi-sector geometry.

    For Slice 6a every linedef is single-sided and bounds the outside of its sector.
*/
struct LineDef
{
    int v1Index = -1;     // index into the owning Sector's `vertices` array
    int v2Index = -1;

    juce::String frontMaterial = "drywall";
    juce::String backMaterial  = "drywall";  // unused while isTwoSided is false

    bool isTwoSided = false;                 // Slice 6b

    LineDef() = default;
    LineDef (int v1, int v2, juce::String material = "drywall")
        : v1Index (v1), v2Index (v2), frontMaterial (std::move (material)) {}
};
} // namespace Worldizer
