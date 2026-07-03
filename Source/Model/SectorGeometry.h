#pragma once

#include <JuceHeader.h>
#include <vector>
#include "Sector.h"
#include "Brush.h"
#include "../Shared/Vec3.h"

namespace Worldizer
{
/**
    A scene's full set of user-authored sector geometry, plus the bridge from that
    higher-level model into the `Brush` list the ray tracer consumes.

    For Slice 6a a `SectorGeometry` holds **at most one** `Sector`. Multi-sector
    support arrives in Slice 6b.

    `compileToBrushes()` is the architectural seam: the editor never sees brushes,
    the ray tracer never sees sectors. If the ray tracer is ever swapped for a true
    triangle-mesh engine, only this compiler changes — the editor stays the same.
*/
class SectorGeometry
{
public:
    SectorGeometry() = default;

    std::vector<Sector> sectors;

    size_t getNumSectors() const noexcept { return sectors.size(); }
    bool   isEmpty()       const noexcept { return sectors.empty() || sectors[0].vertices.empty(); }

    /** Compiles to a brush list:
          - One axis-aligned box per sector floor   (over the sector's AABB)
          - One axis-aligned box per sector ceiling (over the sector's AABB)
          - One `Brush::Type::OrientedWall` per linedef (vertical extrusion floor->ceiling)

        The floor/ceiling AABB over-approximates non-rectangular sectors slightly — the
        floor extends beyond the polygon in concavity regions. The walls bound the
        useful interior, so in practice rays don't reach those over-approximated regions
        unless they escape through an open boundary. Acceptable for Slice 6a; the
        polygon-floor refinement is a Slice 6b candidate. */
    std::vector<Brush> compileToBrushes() const;

    struct AABB3D { Vec3 min, max; };

    /** 3D AABB of all sectors combined (uses floor / ceiling heights for Z). */
    AABB3D getBounds() const noexcept;

    // === Serialization (used by WzPresetIO v3) ===
    juce::var toJson() const;
    bool      fromJson (const juce::var& json, juce::String& errorOut);

    // === Room-shell conversion (Slice 6b-lite: "edit the preset's walls") ===

    /** Converts a brush-built room SHELL — the six Box brushes every shipped scene
        names "floor" / "ceiling" / "wall_xneg|xpos|yneg|ypos" — into an editable
        rectangular Sector at the interior bounds, with the shell's materials and
        heights, and removes those six brushes from the scene. Interior prop
        brushes (columns, crates, furniture...) are untouched and remain fixed
        obstacles. Returns false (scene unchanged) when the scene already has
        sector geometry or lacks a complete closed shell (open-air scenes).

        The compiled sector is acoustically CLOSE to the brush shell (same bounds,
        materials, heights; 10 cm oriented walls vs slabs) but not sample-identical
        — a re-render happens on the user's first actual edit anyway. */
    static bool convertRoomShell (class Scene& scene);

    /** Removes the six shell brushes without building a sector — used on state
        restore when a saved session had already converted (the restored sector
        supersedes the shell; leaving both would double the walls). */
    static bool removeRoomShell (class Scene& scene);
};
} // namespace Worldizer
