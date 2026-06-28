#pragma once

#include <vector>
#include "../Model/Vertex.h"
#include "../Model/Sector.h"

namespace Worldizer
{
/**
    State machine for the editor's "Draw sector" tool. Tracks a draft polygon of
    placed vertices; the editor's room view queries it for rubber-band rendering,
    snap-to-start detection, and the final Sector to commit on close.

    Lifecycle:
      idle  -- begin() -->  drawing
      drawing -- addVertex() --> drawing  (multiple times)
      drawing -- closeSector() --> idle (returns the built Sector)
      drawing -- cancel()      --> idle
*/
class DrawingState
{
public:
    void  begin()             noexcept { active = true; vertices.clear(); }
    void  cancel()            noexcept { active = false; vertices.clear(); }
    bool  isActive()    const noexcept { return active; }
    int   getNumVertices() const noexcept { return (int) vertices.size(); }

    const std::vector<Vertex>& getVertices() const noexcept { return vertices; }
    Vertex getFirstVertex() const noexcept { return vertices.empty() ? Vertex{} : vertices.front(); }
    Vertex getLastVertex()  const noexcept { return vertices.empty() ? Vertex{} : vertices.back(); }

    /** Append a vertex to the draft. */
    void addVertex (Vertex v) { if (active) vertices.push_back (v); }

    /** True if the cursor is close enough to the starting vertex to close (and we
        have at least 3 vertices placed — sectors must be a real polygon). */
    bool wouldCloseAt (Vertex cursorScenePos, float snapDistance) const noexcept
    {
        return active && vertices.size() >= 3
            && vertices.front().isCloseTo (cursorScenePos, snapDistance);
    }

    /** Build the finished Sector with default heights/materials. Caller commits it
        into the scene's `SectorGeometry`. Empties the draft and returns to idle. */
    Sector closeSector()
    {
        Sector s;
        s.vertices = vertices;
        const int n = (int) vertices.size();
        for (int i = 0; i < n; ++i)
            s.lineDefs.emplace_back (i, (i + 1) % n, juce::String ("drywall"));

        // Ensure CCW winding — the compiler / interior test expect it for `containsPoint`.
        if (! s.isCounterClockwise())
            s.reverseWinding();

        vertices.clear();
        active = false;
        return s;
    }

private:
    bool active = false;
    std::vector<Vertex> vertices;
};
} // namespace Worldizer
