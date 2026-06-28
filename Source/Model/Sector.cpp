#include "Sector.h"
#include <algorithm>
#include <limits>

namespace Worldizer
{
bool Sector::isClosed() const noexcept
{
    if (vertices.size() < 3 || lineDefs.size() != vertices.size())
        return false;

    const int n = (int) vertices.size();
    for (int i = 0; i < n; ++i)
    {
        const auto& ld = lineDefs[(size_t) i];
        if (ld.v1Index != i || ld.v2Index != ((i + 1) % n))
            return false;
    }
    return true;
}

float Sector::getSignedArea() const noexcept
{
    // Shoelace formula. Positive => CCW winding in a standard Y-up coordinate frame.
    const int n = (int) vertices.size();
    if (n < 3) return 0.0f;
    double a = 0.0;
    for (int i = 0; i < n; ++i)
    {
        const auto& p = vertices[(size_t) i];
        const auto& q = vertices[(size_t) ((i + 1) % n)];
        a += (double) p.x * q.y - (double) q.x * p.y;
    }
    return (float) (0.5 * a);
}

bool Sector::isCounterClockwise() const noexcept
{
    return getSignedArea() > 0.0f;
}

void Sector::reverseWinding()
{
    std::reverse (vertices.begin(), vertices.end());
    // Rewrite linedef vertex refs to wrap around the reversed order so each linedef
    // still connects perimeter edge i to edge (i+1) mod N.
    const int n = (int) vertices.size();
    std::reverse (lineDefs.begin(), lineDefs.end());
    for (int i = 0; i < n; ++i)
    {
        lineDefs[(size_t) i].v1Index = i;
        lineDefs[(size_t) i].v2Index = (i + 1) % n;
    }
}

bool Sector::containsPoint (Vertex p) const noexcept
{
    // Standard ray-casting / even-odd test. Counts edge crossings of a +X ray from p.
    const int n = (int) vertices.size();
    if (n < 3) return false;
    bool inside = false;
    for (int i = 0, j = n - 1; i < n; j = i++)
    {
        const auto& vi = vertices[(size_t) i];
        const auto& vj = vertices[(size_t) j];
        const bool crosses = ((vi.y > p.y) != (vj.y > p.y))
            && (p.x < (vj.x - vi.x) * (p.y - vi.y) / (vj.y - vi.y + 1.0e-12f) + vi.x);
        if (crosses) inside = ! inside;
    }
    return inside;
}

Sector::AABB Sector::getAABB() const noexcept
{
    AABB box {};
    if (vertices.empty()) return box;
    box.minX = box.maxX = vertices[0].x;
    box.minY = box.maxY = vertices[0].y;
    for (const auto& v : vertices)
    {
        box.minX = std::min (box.minX, v.x); box.maxX = std::max (box.maxX, v.x);
        box.minY = std::min (box.minY, v.y); box.maxY = std::max (box.maxY, v.y);
    }
    return box;
}

void Sector::appendVertex (Vertex v, juce::String wallMaterial)
{
    // The new linedef connects the just-added vertex (newIdx-1) to this one (newIdx).
    // The very first vertex creates no linedef — the editor's "close" step is what
    // creates the wrap-around linedef from the last vertex back to vertex 0.
    const int newIdx = (int) vertices.size();
    vertices.push_back (v);
    if (newIdx >= 1)
        lineDefs.emplace_back (newIdx - 1, newIdx, std::move (wallMaterial));
}

void Sector::splitLineDef (int lineDefIndex, Vertex newVertex)
{
    if (lineDefIndex < 0 || (size_t) lineDefIndex >= lineDefs.size()) return;

    const auto original = lineDefs[(size_t) lineDefIndex];
    const int  insertedVertexIndex = (int) vertices.size();
    vertices.push_back (newVertex);

    // Replace [v1 -> v2] with [v1 -> new] + [new -> v2]; preserve material on both.
    LineDef a (original.v1Index, insertedVertexIndex, original.frontMaterial);
    LineDef b (insertedVertexIndex, original.v2Index, original.frontMaterial);
    a.backMaterial = original.backMaterial; b.backMaterial = original.backMaterial;
    a.isTwoSided   = original.isTwoSided;   b.isTwoSided   = original.isTwoSided;

    lineDefs[(size_t) lineDefIndex] = a;
    lineDefs.insert (lineDefs.begin() + lineDefIndex + 1, b);
}

void Sector::removeVertex (int vertexIndex)
{
    const int n = (int) vertices.size();
    if (vertexIndex < 0 || vertexIndex >= n) return;

    // Find the two linedefs that touch this vertex and merge them. In a closed
    // sector these are (prev->this) and (this->next). The merged linedef goes
    // prev->next and inherits the *previous* linedef's front material.
    int prevIdx = -1, nextIdx = -1;
    for (int i = 0; i < (int) lineDefs.size(); ++i)
    {
        if (lineDefs[(size_t) i].v2Index == vertexIndex) prevIdx = i;
        if (lineDefs[(size_t) i].v1Index == vertexIndex) nextIdx = i;
    }

    vertices.erase (vertices.begin() + vertexIndex);
    // Renumber vertex refs above the removed index.
    for (auto& ld : lineDefs)
    {
        if (ld.v1Index > vertexIndex) --ld.v1Index;
        if (ld.v2Index > vertexIndex) --ld.v2Index;
    }

    if (prevIdx >= 0 && nextIdx >= 0)
    {
        // Splice: prev now goes to whatever next used to point to.
        lineDefs[(size_t) prevIdx].v2Index = lineDefs[(size_t) nextIdx].v2Index;
        // Drop the now-redundant next linedef. (Account for shift if nextIdx > prevIdx.)
        lineDefs.erase (lineDefs.begin() + nextIdx);
    }
}

void Sector::moveVertex (int vertexIndex, Vertex newPos)
{
    if (vertexIndex < 0 || (size_t) vertexIndex >= vertices.size()) return;
    vertices[(size_t) vertexIndex] = newPos;
}
} // namespace Worldizer
