#include "Scene.h"
#include <limits>

namespace Worldizer
{
void Scene::addBrush (Brush brush)
{
    brushes.push_back (std::move (brush));
}

std::vector<Brush> Scene::getAllBrushesForTracing() const
{
    // Manually-added brushes (Test scenes / legacy presets) FIRST, then the compiled
    // sector geometry (user-authored). Order matters only for stable brush indices in
    // the trace result — early indices stay the same when sectors are added.
    auto out = brushes;
    if (! sectorGeometry.isEmpty())
    {
        const auto compiled = sectorGeometry.compileToBrushes();
        out.reserve (out.size() + compiled.size());
        for (const auto& b : compiled)
            out.push_back (b);
    }
    return out;
}

std::pair<Vec3, Vec3> Scene::getBounds() const noexcept
{
    Vec3 lo, hi;
    bool any = false;

    auto include = [&] (Vec3 mn, Vec3 mx)
    {
        if (! any) { lo = mn; hi = mx; any = true; return; }
        lo = componentMin (lo, mn);
        hi = componentMax (hi, mx);
    };

    for (const auto& b : brushes)
        include (b.getMin(), b.getMax());

    if (! sectorGeometry.isEmpty())
    {
        const auto sb = sectorGeometry.getBounds();
        include (sb.min, sb.max);
    }

    return any ? std::pair<Vec3, Vec3> { lo, hi } : std::pair<Vec3, Vec3> { Vec3 {}, Vec3 {} };
}

bool Scene::intersectBrushList (const std::vector<Brush>& brushList,
                                Vec3 origin, Vec3 direction, float tMin,
                                int& brushIndex, float& t, Vec3& hitPoint,
                                Vec3& normal, Brush::Face& face) noexcept
{
    bool found = false;
    float bestT = std::numeric_limits<float>::infinity();

    for (int i = 0; i < (int) brushList.size(); ++i)
    {
        float bt;
        Vec3 bhp, bn;
        Brush::Face bf;

        if (brushList[(size_t) i].intersect (origin, direction, tMin, bt, bhp, bn, bf)
            && bt < bestT)
        {
            bestT = bt;
            brushIndex = i;
            t = bt;
            hitPoint = bhp;
            normal = bn;
            face = bf;
            found = true;
        }
    }

    return found;
}

bool Scene::intersect (Vec3 origin, Vec3 direction, float tMin,
                       int& brushIndex, float& t, Vec3& hitPoint,
                       Vec3& normal, Brush::Face& face) const noexcept
{
    return intersectBrushList (brushes, origin, direction, tMin,
                               brushIndex, t, hitPoint, normal, face);
}
} // namespace Worldizer
