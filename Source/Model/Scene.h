#pragma once

#include <JuceHeader.h>
#include <vector>
#include <utility>
#include "../Shared/Vec3.h"
#include "Brush.h"
#include "SourceNode.h"
#include "MicNode.h"
#include "MicArray.h"
#include "SectorGeometry.h"

namespace Worldizer
{
/**
    Top-level container for a worldizing scene: brushes + a source + a mic array +
    user-authored sector geometry. The five Test presets and any legacy preset use
    the manually-added brush path (`addBrush` / `getBrushes`). User-authored presets
    use the sector path (`getSectorGeometry`). Both can coexist; `getAllBrushesForTracing()`
    unions them at trace time and is what the ray tracer reads. Slice 6a introduced
    the sector path; Slice 5 introduced the `MicArray`.
*/
class Scene
{
public:
    Scene() = default;

    // === Brushes (manual / legacy path) ===
    void addBrush (Brush brush);
    const std::vector<Brush>& getBrushes() const noexcept   { return brushes; }
    size_t getNumBrushes() const noexcept                   { return brushes.size(); }

    /** Removes the first brush with this id. Returns true if one was removed.
        Used by the room-shell -> sector conversion (the sector supersedes the
        shell brushes). */
    bool removeBrushById (const juce::String& brushId);

    // === Sector geometry (user-authored path, Slice 6a) ===
    SectorGeometry&       getSectorGeometry() noexcept       { return sectorGeometry; }
    const SectorGeometry& getSectorGeometry() const noexcept { return sectorGeometry; }

    /** Returns the brush list the ray tracer should iterate: the manually-added
        brushes (Test scenes / legacy presets) UNION the compiled sector geometry
        (user-authored presets). Recomputed per call — geometry edits are reflected
        next trace without a separate invalidation call. */
    std::vector<Brush> getAllBrushesForTracing() const;

    // === Source & mic array ===
    SourceNode& getSource() noexcept                        { return source; }
    const SourceNode& getSource() const noexcept            { return source; }

    MicArray& getMicArray() noexcept                        { return micArray; }
    const MicArray& getMicArray() const noexcept            { return micArray; }

    /** Legacy convenience — the primary mic (mic 0). Prefer getMicArray() for new
        code; this exists so single-mic call sites from earlier slices still compile. */
    MicNode& getMic() noexcept                              { return micArray.getPrimary(); }
    const MicNode& getMic() const noexcept                  { return micArray.getPrimary(); }

    // === Bounds ===
    /** Returns the axis-aligned bounding box (min, max) of all brushes. */
    std::pair<Vec3, Vec3> getBounds() const noexcept;

    // === Ray testing against a brush list ===
    /** Closest-hit ray-vs-brush-list search. Iterates the given list, keeping the
        nearest hit at distance >= tMin. Static so the ray tracer can run it against
        a pre-built `getAllBrushesForTracing()` snapshot without re-compiling sector
        geometry on every bounce. */
    static bool intersectBrushList (const std::vector<Brush>& brushes,
                                    Vec3 origin,
                                    Vec3 direction,
                                    float tMin,
                                    int& brushIndex,
                                    float& t,
                                    Vec3& hitPoint,
                                    Vec3& normal,
                                    Brush::Face& face) noexcept;

    /** Convenience: ray-vs-internal-brushes only (no compiled sector geometry).
        For ray tracing prefer `intersectBrushList (getAllBrushesForTracing(), ...)`. */
    bool intersect (Vec3 origin,
                    Vec3 direction,
                    float tMin,
                    int& brushIndex,
                    float& t,
                    Vec3& hitPoint,
                    Vec3& normal,
                    Brush::Face& face) const noexcept;

private:
    std::vector<Brush> brushes;
    SourceNode source;
    MicArray micArray;
    SectorGeometry sectorGeometry;
};
} // namespace Worldizer
