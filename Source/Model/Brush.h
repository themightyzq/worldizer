#pragma once

#include <JuceHeader.h>
#include <array>
#include "../Shared/Vec3.h"
#include "Material.h"

namespace Worldizer
{
/**
    A convex 3D volume defined by axis-aligned bounds + per-face material.

    For MVP, only axis-aligned boxes are supported. Test-scene rooms are built
    from thin slab brushes (one per surface); rays travel the interior and
    reflect off the room-facing face, whose outward normal points into the room.
    Future versions will support arbitrary convex polyhedra (the .map importer).
*/
class Brush
{
public:
    enum class Face { NegX, PosX, NegY, PosY, NegZ, PosZ };
    static constexpr int kNumFaces = 6;

    enum class Kind { Additive, Subtractive };

    Brush() = default;
    Brush (juce::String id,
           Vec3 minCorner,
           Vec3 maxCorner,
           Kind kind = Kind::Additive);

    const juce::String& getId() const noexcept   { return id; }
    Kind getKind() const noexcept                { return kind; }
    Vec3 getMin() const noexcept                 { return minCorner; }
    Vec3 getMax() const noexcept                 { return maxCorner; }

    /** Returns the centre point of the brush. */
    Vec3 getCenter() const noexcept;

    /** Returns the material assigned to a specific face. */
    const Material& getFaceMaterial (Face face) const noexcept { return faceMaterials[(size_t) face]; }

    /** Assign a material to a specific face. */
    void setFaceMaterial (Face face, Material m);

    /** Assign the same material to all faces. */
    void setAllFaceMaterials (Material m);

    // === Ray intersection ===

    /** Slab-method ray-box intersection. Returns true if the ray hits the brush.
        On hit, sets t (distance along ray, direction assumed unit), hitPoint,
        normal (pointing out of the brush), and face. Only returns hits where
        t >= tMin. */
    bool intersect (Vec3 origin,
                    Vec3 direction,
                    float tMin,
                    float& t,
                    Vec3& hitPoint,
                    Vec3& normal,
                    Face& face) const noexcept;

    /** Returns true if the given point is inside the brush volume. */
    bool contains (Vec3 point) const noexcept;

private:
    juce::String id;
    Vec3 minCorner;
    Vec3 maxCorner;
    Kind kind = Kind::Additive;
    std::array<Material, kNumFaces> faceMaterials;
};
} // namespace Worldizer
