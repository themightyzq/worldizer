#include "MicNode.h"
#include <cmath>

namespace Worldizer
{
MicNode::MicNode (Vec3 p, float r)
    : position (p), radius (juce::jmax (0.01f, r))
{
}

float MicNode::getReceptionGain (Vec3 incomingDir) const noexcept
{
    switch (pattern)
    {
        case MicPattern::Omnidirectional:
            return 1.0f;

        case MicPattern::Shotgun:
        {
            // Guard the degenerate case (source exactly at the mic): no defined
            // incoming angle, so fall back to omni rather than reading a garbage dot.
            if (incomingDir.lengthSquared() < 1.0e-12f)
                return 1.0f;

            // Shotgun: very narrow forward lobe, soft sides, significant rear
            // rejection. A cardioid term times a cos^... narrowing term:
            //   gain = (0.5 + 0.5*cos) * (0.5 + 0.5*cos)^3, floored at 0.05.
            // Verified at a few angles:
            //   0   deg: cos= 1   -> 1.000   (on-axis, full)
            //   45  deg: cos= 0.707 -> ~0.53 (~-5.5 dB)
            //   90  deg: cos= 0   -> 0.0625 -> floored 0.05 (~-26 dB)
            //   180 deg: cos=-1   -> 0      -> floored 0.05 (rear lobe / cabinet leak)
            const float cosAngle  = dot (orientation, incomingDir);
            const float cardioid  = 0.5f * (1.0f + cosAngle);
            const float narrowing = std::pow (juce::jmax (0.0f, 0.5f + 0.5f * cosAngle), 3.0f);
            return juce::jmax (0.05f, cardioid * narrowing);
        }
    }
    return 1.0f;
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
