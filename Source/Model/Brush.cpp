#include "Brush.h"
#include <limits>
#include <utility>
#include <cmath>

namespace Worldizer
{
//==============================================================================
Brush::Brush (juce::String id_, Vec3 minCorner_, Vec3 maxCorner_, Kind kind_)
    : id (std::move (id_)),
      kind (kind_),
      type (Type::Box),
      aabbMin (componentMin (minCorner_, maxCorner_)),
      aabbMax (componentMax (minCorner_, maxCorner_))
{
}

//==============================================================================
Brush::Brush (juce::String id_, const OrientedWallParams& p, Kind kind_)
    : id (std::move (id_)),
      kind (kind_),
      type (Type::OrientedWall)
{
    // Local frame: length axis along v1->v2 in XY; thickness axis perpendicular in XY;
    // height axis straight up. Degenerate (v1 == v2 in XY) collapses the wall — its
    // half-extents go to zero on the length axis and intersection rejects.
    const Vec3 lengthDelta { p.v2.x - p.v1.x, p.v2.y - p.v1.y, 0.0f };
    const float length = lengthDelta.length();

    obbAxisLength    = length > 1.0e-6f ? lengthDelta / length : Vec3 { 1.0f, 0.0f, 0.0f };
    obbAxisThickness = Vec3 { -obbAxisLength.y, obbAxisLength.x, 0.0f };  // 90° CCW in XY
    obbAxisHeight    = Vec3 { 0.0f, 0.0f, 1.0f };

    const float thickness = juce::jmax (0.0f, p.thickness);
    const float floorZ    = juce::jmin (p.floorZ, p.ceilingZ);
    const float ceilingZ  = juce::jmax (p.floorZ, p.ceilingZ);
    const float height    = ceilingZ - floorZ;

    obbHalfExtents = { 0.5f * length, 0.5f * thickness, 0.5f * height };
    obbCenter      = { 0.5f * (p.v1.x + p.v2.x),
                       0.5f * (p.v1.y + p.v2.y),
                       0.5f * (floorZ + ceilingZ) };

    // AABB of the rotated rectangle for Scene::getBounds() consumers.
    const Vec3 e0 = obbAxisLength    * obbHalfExtents.x;
    const Vec3 e1 = obbAxisThickness * obbHalfExtents.y;
    const Vec3 e2 = obbAxisHeight    * obbHalfExtents.z;
    const Vec3 extentAbs { std::abs (e0.x) + std::abs (e1.x) + std::abs (e2.x),
                           std::abs (e0.y) + std::abs (e1.y) + std::abs (e2.y),
                           std::abs (e0.z) + std::abs (e1.z) + std::abs (e2.z) };
    aabbMin = obbCenter - extentAbs;
    aabbMax = obbCenter + extentAbs;
}

//==============================================================================
Vec3 Brush::getCenter() const noexcept
{
    return type == Type::OrientedWall ? obbCenter : (aabbMin + aabbMax) * 0.5f;
}

void Brush::setFaceMaterial (Face face, Material m)
{
    faceMaterials[(size_t) face] = std::move (m);
}

void Brush::setAllFaceMaterials (Material m)
{
    for (auto& f : faceMaterials)
        f = m;
}

//==============================================================================
bool Brush::intersect (Vec3 origin, Vec3 direction, float tMin,
                       float& t, Vec3& hitPoint, Vec3& normal, Face& face) const noexcept
{
    return type == Type::OrientedWall
            ? intersectOrientedWall (origin, direction, tMin, t, hitPoint, normal, face)
            : intersectBox          (origin, direction, tMin, t, hitPoint, normal, face);
}

bool Brush::contains (Vec3 point) const noexcept
{
    if (type == Type::OrientedWall)
    {
        const Vec3 rel = point - obbCenter;
        const float lp = dot (rel, obbAxisLength);
        const float tp = dot (rel, obbAxisThickness);
        const float hp = dot (rel, obbAxisHeight);
        return std::abs (lp) <= obbHalfExtents.x
            && std::abs (tp) <= obbHalfExtents.y
            && std::abs (hp) <= obbHalfExtents.z;
    }
    return point.x >= aabbMin.x && point.x <= aabbMax.x
        && point.y >= aabbMin.y && point.y <= aabbMax.y
        && point.z >= aabbMin.z && point.z <= aabbMax.z;
}

//==============================================================================
namespace
{
    Vec3 axisUnit (int axis) noexcept
    {
        return axis == 0 ? Vec3 { 1.0f, 0.0f, 0.0f }
             : axis == 1 ? Vec3 { 0.0f, 1.0f, 0.0f }
                         : Vec3 { 0.0f, 0.0f, 1.0f };
    }

    void makeFaceNormal (int axis, bool positiveFace, Brush::Face& face, Vec3& normal) noexcept
    {
        face   = (Brush::Face) (2 * axis + (positiveFace ? 1 : 0));
        normal = positiveFace ? axisUnit (axis) : -axisUnit (axis);
    }

    /** Slab-method ray-AABB intersection over [lo, hi] half-extents centred at the
        origin (local frame). Returns true on hit and fills tHit, the axis (0..2) of
        the crossing, and whether the positive face of that axis was crossed. */
    bool localSlabIntersect (Vec3 o, Vec3 d, Vec3 lo, Vec3 hi, float tMin,
                             float& tHit, int& axisOut, bool& positiveFaceOut) noexcept
    {
        constexpr float kParallelEps = 1.0e-9f;
        float tNear = -std::numeric_limits<float>::infinity();
        float tFar  =  std::numeric_limits<float>::infinity();
        int nearAxis = -1;
        int farAxis  = -1;

        for (int a = 0; a < 3; ++a)
        {
            const float od = (a == 0) ? d.x : (a == 1) ? d.y : d.z;
            const float oo = (a == 0) ? o.x : (a == 1) ? o.y : o.z;
            const float mn = (a == 0) ? lo.x : (a == 1) ? lo.y : lo.z;
            const float mx = (a == 0) ? hi.x : (a == 1) ? hi.y : hi.z;

            if (std::abs (od) < kParallelEps)
            {
                if (oo < mn || oo > mx) return false;
                continue;
            }

            const float inv = 1.0f / od;
            float t1 = (mn - oo) * inv;
            float t2 = (mx - oo) * inv;
            if (t1 > t2) std::swap (t1, t2);
            if (t1 > tNear) { tNear = t1; nearAxis = a; }
            if (t2 < tFar)  { tFar  = t2; farAxis  = a; }
            if (tNear > tFar) return false;
        }

        if (nearAxis < 0 || tFar < tMin) return false;

        const bool usingNear = tNear >= tMin;
        const int  axis = usingNear ? nearAxis : farAxis;
        const float tH  = usingNear ? tNear : tFar;
        if (tH < tMin || axis < 0) return false;

        const float od = (axis == 0) ? d.x : (axis == 1) ? d.y : d.z;
        positiveFaceOut = usingNear ? (od < 0.0f) : (od > 0.0f);
        axisOut = axis;
        tHit = tH;
        return true;
    }
}

bool Brush::intersectBox (Vec3 origin, Vec3 direction, float tMin,
                          float& t, Vec3& hitPoint, Vec3& normal, Face& face) const noexcept
{
    // The original Slice 1 implementation (slab method on aabbMin/aabbMax). The
    // OrientedWall path uses the same kernel via localSlabIntersect after transforming
    // the ray into the wall's local frame.
    constexpr float kParallelEps = 1.0e-9f;
    float tNear = -std::numeric_limits<float>::infinity();
    float tFar  =  std::numeric_limits<float>::infinity();
    int nearAxis = -1;
    int farAxis  = -1;

    for (int a = 0; a < 3; ++a)
    {
        const float o  = origin[a];
        const float d  = direction[a];
        const float mn = aabbMin[a];
        const float mx = aabbMax[a];

        if (std::abs (d) < kParallelEps)
        {
            if (o < mn || o > mx) return false;
            continue;
        }

        const float inv = 1.0f / d;
        float t1 = (mn - o) * inv;
        float t2 = (mx - o) * inv;
        if (t1 > t2) std::swap (t1, t2);
        if (t1 > tNear) { tNear = t1; nearAxis = a; }
        if (t2 < tFar)  { tFar  = t2; farAxis  = a; }
        if (tNear > tFar) return false;
    }

    if (nearAxis < 0 || tFar < tMin) return false;

    const bool usingNear = tNear >= tMin;
    const int   axis = usingNear ? nearAxis : farAxis;
    const float tHit = usingNear ? tNear : tFar;
    if (tHit < tMin || axis < 0) return false;

    const bool positiveFace = usingNear ? (direction[axis] < 0.0f)
                                        : (direction[axis] > 0.0f);

    makeFaceNormal (axis, positiveFace, face, normal);
    t = tHit;
    hitPoint = origin + direction * tHit;
    return true;
}

bool Brush::intersectOrientedWall (Vec3 origin, Vec3 direction, float tMin,
                                   float& t, Vec3& hitPoint, Vec3& normal, Face& face) const noexcept
{
    // Reject zero-volume walls (degenerate v1 == v2, zero thickness, or zero height).
    if (obbHalfExtents.x <= 0.0f || obbHalfExtents.y <= 0.0f || obbHalfExtents.z <= 0.0f)
        return false;

    // Transform the ray into the wall's local (length, thickness, height) frame:
    // the rotation columns are the three local axes, so WORLD->LOCAL is given by
    // dotting with each axis (R^T row form).
    const Vec3 rel = origin - obbCenter;
    const Vec3 localO { dot (rel,       obbAxisLength),
                        dot (rel,       obbAxisThickness),
                        dot (rel,       obbAxisHeight) };
    const Vec3 localD { dot (direction, obbAxisLength),
                        dot (direction, obbAxisThickness),
                        dot (direction, obbAxisHeight) };

    int axis = -1;
    bool positiveFace = false;
    float tHit = 0.0f;
    const Vec3 lo = -obbHalfExtents;
    const Vec3 hi =  obbHalfExtents;
    if (! localSlabIntersect (localO, localD, lo, hi, tMin, tHit, axis, positiveFace))
        return false;

    // Map the local-frame face to the public Face enum:
    //   axis 0 (length)    -> NegX/PosX (the two narrow ends — v1, v2)
    //   axis 1 (thickness) -> NegY/PosY (the two broad faces — back/front)
    //   axis 2 (height)    -> NegZ/PosZ (bottom/top)
    face = (Face) (2 * axis + (positiveFace ? 1 : 0));

    // Local-frame normal -> world: world = R * local (R columns are the local axes).
    const Vec3 nLocal = positiveFace ? axisUnit (axis) : -axisUnit (axis);
    normal = obbAxisLength    * nLocal.x
           + obbAxisThickness * nLocal.y
           + obbAxisHeight    * nLocal.z;

    t = tHit;
    hitPoint = origin + direction * tHit;
    return true;
}
} // namespace Worldizer
