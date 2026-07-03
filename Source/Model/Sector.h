#pragma once

#include <JuceHeader.h>
#include <vector>
#include "Vertex.h"
#include "LineDef.h"

namespace Worldizer
{
/**
    An enclosed floor-plan region with a flat floor and ceiling — the unit of
    Doom-style geometry. The boundary is a closed polygon of `vertices`, with
    `lineDefs[i]` connecting `vertices[i]` to `vertices[(i+1) % N]` (always one
    linedef per edge, in perimeter order).

    For Slice 6a:
      - A scene contains at most one sector.
      - Polygons must be **simple** (no self-intersection) and **closed** (>= 3
        vertices). Convex or concave is fine — the compiler doesn't care.
      - Sectors do not share linedefs with other sectors (no nested sectors).
*/
class Sector
{
public:
    Sector() = default;

    // === Perimeter ===
    std::vector<Vertex>  vertices;
    std::vector<LineDef> lineDefs;       // size == vertices.size() for a closed sector

    size_t getNumWalls() const noexcept  { return lineDefs.size(); }

    // === Heights ===
    float floorHeight   = 0.0f;
    float ceilingHeight = 3.0f;

    float getHeight() const noexcept     { return ceilingHeight - floorHeight; }

    /** Minimum floor→ceiling gap a sector is allowed to have. Below this the room
        has no interior volume and the compiler drops the walls. */
    static constexpr float kMinRoomHeight = 0.5f;

    /** Sets the floor height, keeping the ceiling at least kMinRoomHeight above it
        (raises the ceiling if the new floor would cross it). */
    void setFloorHeight (float h) noexcept
    {
        floorHeight = h;
        if (ceilingHeight < floorHeight + kMinRoomHeight)
            ceilingHeight = floorHeight + kMinRoomHeight;
    }

    /** Sets the ceiling height, keeping it at least kMinRoomHeight above the floor
        (clamps up if the user enters a value at/below the floor). */
    void setCeilingHeight (float h) noexcept
    {
        ceilingHeight = juce::jmax (h, floorHeight + kMinRoomHeight);
    }

    // === Materials ===
    juce::String floorMaterial   = "wood_floor";
    juce::String ceilingMaterial = "drywall";

    // === Geometric queries ===

    /** True when the boundary forms a closed loop (>= 3 vertices and one linedef
        per edge). The editor's drawing state machine flips this true on "close-the-
        sector" click. */
    bool isClosed() const noexcept;

    /** Signed polygon area (positive = counter-clockwise winding). */
    float getSignedArea() const noexcept;

    /** True if the boundary is a SIMPLE polygon (no non-adjacent edges cross) and
        encloses at least `minArea` m². Used to reject self-intersecting bowties
        and degenerate/zero-area sectors before they become incoherent geometry. */
    bool isSimpleWithArea (float minArea = 0.25f) const noexcept;

    /** True if the polygon winds counter-clockwise. */
    bool isCounterClockwise() const noexcept;

    /** Reverses the vertex order so the winding flips; linedef refs are remapped
        so each linedef still connects the same two perimeter edges. */
    void reverseWinding();

    /** True if the 2D point lies inside the polygon (even-odd ray-casting test). */
    bool containsPoint (Vertex point) const noexcept;

    struct AABB { float minX = 0.0f, minY = 0.0f, maxX = 0.0f, maxY = 0.0f; };
    AABB getAABB() const noexcept;

    // === Mutation (editor entry points) ===

    /** Appends a vertex and a linedef. The new linedef connects the previously-last
        vertex to this one. To "close" the sector, the editor instead reuses vertex 0
        as the endpoint of the last linedef (so vertices.size() stays == lineDefs.size()). */
    void appendVertex (Vertex v, juce::String wallMaterial = "drywall");

    /** Inserts a vertex on an existing linedef, splitting it in two with the same
        material. Used to add detail to a wall. */
    void splitLineDef (int lineDefIndex, Vertex newVertex);

    /** Removes a vertex and merges its two adjacent linedefs into one (keeping the
        previous linedef's front material). If the sector falls below 3 vertices the
        caller is expected to delete the sector entirely. */
    void removeVertex (int vertexIndex);

    /** Moves a vertex to a new position (linedef topology unchanged). */
    void moveVertex (int vertexIndex, Vertex newPos);
};
} // namespace Worldizer
