#include "MicNode.h"

namespace Worldizer
{
MicNode::MicNode (Vec3 p, float r)
    : position (p), radius (juce::jmax (0.01f, r))
{
}

bool MicNode::intersectRay (Vec3 origin,
                            Vec3 direction,
                            float maxDistance,
                            float& hitDistance) const noexcept
{
    // Project the mic centre onto the ray. tca is the distance along the (unit)
    // ray to the closest-approach point; d2 is the squared perpendicular distance.
    const Vec3  oc  = position - origin;
    const float tca = dot (oc, direction);

    if (tca < 0.0f || tca > maxDistance)
        return false;

    const float d2 = oc.lengthSquared() - tca * tca;
    if (d2 > radius * radius)
        return false;

    hitDistance = tca;
    return true;
}
} // namespace Worldizer
