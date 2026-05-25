#pragma once

#include <JuceHeader.h>
#include "../Shared/Vec3.h"

namespace Worldizer
{
/**
    A microphone in the scene. For MVP, mics are point receivers with a
    sphere-of-acceptance radius for ray-hit detection. Polar patterns come in v1.0.
*/
class MicNode
{
public:
    MicNode() = default;
    explicit MicNode (Vec3 position, float radius = 0.1f);

    Vec3 getPosition() const noexcept   { return position; }
    void setPosition (Vec3 p) noexcept  { position = p; }

    float getRadius() const noexcept    { return radius; }
    void setRadius (float r) noexcept   { radius = juce::jmax (0.01f, r); }

    /** Tests whether a ray segment passes within the mic's acceptance sphere.
        If so, returns the distance from the ray origin to the closest approach
        point in `hitDistance`. `direction` is assumed to be a unit vector and the
        segment runs from origin to origin + direction * maxDistance. */
    bool intersectRay (Vec3 origin,
                       Vec3 direction,
                       float maxDistance,
                       float& hitDistance) const noexcept;

private:
    Vec3 position { 3.0f, 0.0f, 1.5f };
    float radius = 0.1f;  // 10 cm acceptance sphere — hit count vs. spatial precision tradeoff
};
} // namespace Worldizer
