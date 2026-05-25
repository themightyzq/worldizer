#include "Scene.h"
#include <limits>

namespace Worldizer
{
void Scene::addBrush (Brush brush)
{
    brushes.push_back (std::move (brush));
}

std::pair<Vec3, Vec3> Scene::getBounds() const noexcept
{
    if (brushes.empty())
        return { Vec3 {}, Vec3 {} };

    Vec3 lo = brushes.front().getMin();
    Vec3 hi = brushes.front().getMax();

    for (const auto& b : brushes)
    {
        lo = componentMin (lo, b.getMin());
        hi = componentMax (hi, b.getMax());
    }

    return { lo, hi };
}

bool Scene::intersect (Vec3 origin,
                       Vec3 direction,
                       float tMin,
                       int& brushIndex,
                       float& t,
                       Vec3& hitPoint,
                       Vec3& normal,
                       Brush::Face& face) const noexcept
{
    bool found = false;
    float bestT = std::numeric_limits<float>::infinity();

    for (int i = 0; i < (int) brushes.size(); ++i)
    {
        float bt;
        Vec3 bhp, bn;
        Brush::Face bf;

        if (brushes[(size_t) i].intersect (origin, direction, tMin, bt, bhp, bn, bf)
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
} // namespace Worldizer
