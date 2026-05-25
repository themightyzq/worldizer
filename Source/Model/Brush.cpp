#include "Brush.h"
#include <limits>
#include <utility>

namespace Worldizer
{
Brush::Brush (juce::String id_, Vec3 minCorner_, Vec3 maxCorner_, Kind kind_)
    : id (std::move (id_)),
      minCorner (componentMin (minCorner_, maxCorner_)),
      maxCorner (componentMax (minCorner_, maxCorner_)),
      kind (kind_)
{
}

Vec3 Brush::getCenter() const noexcept
{
    return (minCorner + maxCorner) * 0.5f;
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
}

bool Brush::intersect (Vec3 origin,
                       Vec3 direction,
                       float tMin,
                       float& t,
                       Vec3& hitPoint,
                       Vec3& normal,
                       Face& face) const noexcept
{
    constexpr float kParallelEps = 1.0e-9f;

    float tNear = -std::numeric_limits<float>::infinity();
    float tFar  =  std::numeric_limits<float>::infinity();
    int nearAxis = -1;
    int farAxis  = -1;

    for (int a = 0; a < 3; ++a)
    {
        const float o  = origin[a];
        const float d  = direction[a];
        const float mn = minCorner[a];
        const float mx = maxCorner[a];

        if (std::abs (d) < kParallelEps)
        {
            // Ray parallel to this slab's planes: miss unless the origin is between them.
            if (o < mn || o > mx)
                return false;
            continue;
        }

        const float inv = 1.0f / d;
        float t1 = (mn - o) * inv;
        float t2 = (mx - o) * inv;
        if (t1 > t2)
            std::swap (t1, t2);

        if (t1 > tNear) { tNear = t1; nearAxis = a; }
        if (t2 < tFar)  { tFar  = t2; farAxis  = a; }

        if (tNear > tFar)
            return false;
    }

    if (nearAxis < 0 || tFar < tMin)
        return false;

    bool usingNear = tNear >= tMin;
    const int   axis = usingNear ? nearAxis : farAxis;
    const float tHit = usingNear ? tNear : tFar;

    if (tHit < tMin || axis < 0)
        return false;

    // Near crossing (entering): outward normal opposes ray travel on this axis.
    // Far crossing (exiting, origin inside): outward normal follows ray travel.
    const bool positiveFace = usingNear ? (direction[axis] < 0.0f)
                                        : (direction[axis] > 0.0f);

    makeFaceNormal (axis, positiveFace, face, normal);

    t = tHit;
    hitPoint = origin + direction * tHit;
    return true;
}

bool Brush::contains (Vec3 point) const noexcept
{
    return point.x >= minCorner.x && point.x <= maxCorner.x
        && point.y >= minCorner.y && point.y <= maxCorner.y
        && point.z >= minCorner.z && point.z <= maxCorner.z;
}
} // namespace Worldizer
