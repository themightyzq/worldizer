#pragma once

#include <cmath>

namespace Worldizer
{
/**
    A 2D point in the floor plan (the world's XY plane). Z is not stored here —
    the containing Sector supplies floor/ceiling heights for any wall going through
    this vertex.
*/
struct Vertex
{
    float x = 0.0f;
    float y = 0.0f;

    Vertex() = default;
    constexpr Vertex (float xx, float yy) noexcept : x (xx), y (yy) {}

    // Use isCloseTo() for "are these the same point?" — floating-point exact compare
    // is intentionally not provided.

    /** Squared 2D distance to another vertex. Cheaper than `distanceTo` — fine for
        hit-tests where you only need to compare against a threshold (then square it). */
    float distanceSquaredTo (const Vertex& o) const noexcept
    {
        const float dx = x - o.x;
        const float dy = y - o.y;
        return dx * dx + dy * dy;
    }

    /** True if two vertices are within `epsilon` metres of each other. Used by the
        editor for "snap to existing vertex" and "close-the-sector" detection. */
    bool isCloseTo (const Vertex& o, float epsilon = 0.05f) const noexcept
    {
        return distanceSquaredTo (o) <= epsilon * epsilon;
    }
};
} // namespace Worldizer
