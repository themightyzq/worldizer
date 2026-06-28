#pragma once

#include <JuceHeader.h>
#include <array>
#include "../Shared/Vec3.h"
#include "Material.h"

namespace Worldizer
{
/**
    A volume in the scene defined by per-face materials and a geometric shape.

    Two shape types are supported (Slice 6a):
      - **Box**           : axis-aligned 3D box (the original Slice 1 brush).
                            Used for floors, ceilings, the legacy Test-scene rooms,
                            and any axis-aligned geometry.
      - **OrientedWall**  : a thin vertical rectangle defined by two horizontal endpoints
                            (v1, v2) in XY plus floor/ceiling Z and a thickness. The wall
                            is treated as an oriented bounding box (OBB) and intersected
                            in the wall's local frame. Used for Doom-style sector walls
                            at arbitrary angles.

    The ray tracer dispatches on type in `intersect()`; everything else (face materials,
    Additive/Subtractive `Kind`) is shared. Adding more shape types in the future is a
    matter of adding an enum value, a constructor, and an intersection routine.
*/
class Brush
{
public:
    enum class Face { NegX, PosX, NegY, PosY, NegZ, PosZ };
    static constexpr int kNumFaces = 6;

    enum class Kind { Additive, Subtractive };
    enum class Type { Box, OrientedWall };

    Brush() = default;

    /** Axis-aligned box constructor (original Slice 1 path). */
    Brush (juce::String id,
           Vec3 minCorner,
           Vec3 maxCorner,
           Kind kind = Kind::Additive);

    /** OrientedWall parameters: a wall between two horizontal endpoints, extruded
        vertically from floorZ to ceilingZ, with a small thickness perpendicular to
        the v1->v2 direction in the XY plane. */
    struct OrientedWallParams
    {
        Vec3  v1;            // first endpoint (z component ignored — uses floorZ/ceilingZ)
        Vec3  v2;            // second endpoint
        float floorZ;        // bottom of the wall
        float ceilingZ;      // top
        float thickness;     // wall thickness in metres (perpendicular to v1->v2 in XY)
    };

    /** OrientedWall constructor. v1 != v2 required; degenerate input produces a
        zero-volume wall (intersection returns false). */
    Brush (juce::String id,
           const OrientedWallParams& params,
           Kind kind = Kind::Additive);

    Type getType() const noexcept                { return type; }
    const juce::String& getId() const noexcept   { return id; }
    Kind getKind() const noexcept                { return kind; }

    /** AABB of the brush (the axis-aligned box bounds for Box; the rotated rectangle's
        AABB for OrientedWall). Used by `Scene::getBounds()` etc. */
    Vec3 getMin() const noexcept                 { return aabbMin; }
    Vec3 getMax() const noexcept                 { return aabbMax; }

    /** Returns the centre point of the brush. */
    Vec3 getCenter() const noexcept;

    /** Material assigned to a specific face. For OrientedWall the face indices map:
          NegX/PosX -> the two narrow ends (v1 and v2)
          NegY/PosY -> the two broad faces (back/front along the thickness axis)
          NegZ/PosZ -> bottom/top */
    const Material& getFaceMaterial (Face face) const noexcept { return faceMaterials[(size_t) face]; }
    void setFaceMaterial (Face face, Material m);
    void setAllFaceMaterials (Material m);

    // === Ray intersection ===

    /** Ray-brush intersection. Dispatches on `Type`. Returns true if the ray hits the
        brush at t >= tMin; on hit, sets t (distance along the unit-vector ray),
        hitPoint, the outward `normal`, and which face was hit. */
    bool intersect (Vec3 origin,
                    Vec3 direction,
                    float tMin,
                    float& t,
                    Vec3& hitPoint,
                    Vec3& normal,
                    Face& face) const noexcept;

    /** True if the point is inside the brush volume. */
    bool contains (Vec3 point) const noexcept;

private:
    juce::String id;
    Kind kind = Kind::Additive;
    Type type = Type::Box;
    std::array<Material, kNumFaces> faceMaterials;

    // For Box: the axis-aligned bounds.
    // For OrientedWall: the AABB of the rotated rectangle (used by getMin/getMax).
    Vec3 aabbMin;
    Vec3 aabbMax;

    // OrientedWall-only OBB representation.
    Vec3 obbCenter;
    Vec3 obbAxisLength;     // unit vector along the v1->v2 direction (length axis)
    Vec3 obbAxisThickness;  // unit vector perpendicular in XY (thickness axis)
    Vec3 obbAxisHeight;     // (0,0,1) (height axis)
    Vec3 obbHalfExtents;    // (halfLength, halfThickness, halfHeight)

    bool intersectBox (Vec3 origin, Vec3 direction, float tMin,
                       float& t, Vec3& hitPoint, Vec3& normal, Face& face) const noexcept;
    bool intersectOrientedWall (Vec3 origin, Vec3 direction, float tMin,
                                float& t, Vec3& hitPoint, Vec3& normal, Face& face) const noexcept;
};
} // namespace Worldizer
