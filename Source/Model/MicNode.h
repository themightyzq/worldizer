#pragma once

#include <JuceHeader.h>
#include "../Shared/Vec3.h"

namespace Worldizer
{
/** Polar pattern for a microphone.
    MVP supports Omnidirectional and Shotgun; the enum admits future patterns
    (cardioid, supercardioid, figure-8, ...) without ray-tracer / IR rework. */
enum class MicPattern
{
    Omnidirectional,
    Shotgun
    // Future: Cardioid, Supercardioid, Hypercardioid, Figure8, WideCardioid
};

/**
    A microphone in the scene. A point receiver with a sphere-of-acceptance radius
    for ray-hit detection, plus a polar pattern that weights how strongly a ray is
    received as a function of its incoming angle relative to the mic's orientation.
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

    MicPattern getPattern() const noexcept  { return pattern; }
    void setPattern (MicPattern p) noexcept { pattern = p; }

    /** Mic facing direction (unit vector). Default -X (toward a typical source at
        the origin). Irrelevant for omni; required for directional patterns. */
    Vec3 getOrientation() const noexcept    { return orientation; }
    void setOrientation (Vec3 dir) noexcept { const auto n = dir.normalised(); orientation = (n.lengthSquared() > 0.0f) ? n : orientation; }

    /** True if the pattern's response varies with angle (i.e. not omni). */
    bool isDirectional() const noexcept     { return pattern != MicPattern::Omnidirectional; }

    /** Pattern amplitude gain (0..1+) for a ray arriving from `incomingRayDirection`.
        Omni: always 1.0. Shotgun: narrow forward lobe, soft sides, rear rejection.
        (Energy weighting squares this — see RayTracer / IRBuilder.)

        @param incomingRayDirection  Unit vector pointing FROM the mic TO the source
                                      of the ray (i.e. the negation of the ray's
                                      travel direction at impact). Zero-length input
                                      (source exactly at the mic) returns 1.0. */
    float getReceptionGain (Vec3 incomingRayDirection) const noexcept;

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
    MicPattern pattern = MicPattern::Omnidirectional;
    Vec3 orientation { -1.0f, 0.0f, 0.0f };  // default facing -X (toward a source at the origin)
};
} // namespace Worldizer
