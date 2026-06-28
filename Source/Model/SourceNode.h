#pragma once

#include <JuceHeader.h>
#include "../Shared/Vec3.h"

namespace Worldizer
{
/** Polar pattern for a sound source.
    MVP supports Omnidirectional only; the enum admits future patterns so the ray
    tracer / serialisation never need rework when they ship. */
enum class SourcePattern
{
    Omnidirectional
    // Future: Cardioid, WideCardioid, NarrowCone
};

/**
    A sound source in the scene. For MVP, sources are omnidirectional; directivity
    patterns come post-MVP. Orientation is stored even for omni so serialisation is
    lossless (the UI just doesn't expose rotation for an omni source).
*/
class SourceNode
{
public:
    SourceNode() = default;
    explicit SourceNode (Vec3 position);

    Vec3 getPosition() const noexcept       { return position; }
    void setPosition (Vec3 p) noexcept      { position = p; }

    SourcePattern getPattern() const noexcept  { return pattern; }
    void setPattern (SourcePattern p) noexcept { pattern = p; }

    /** Source facing direction (unit vector). Irrelevant for omni but stored so the
        serialisation round-trip is lossless and directional patterns drop in later. */
    Vec3 getOrientation() const noexcept    { return orientation; }
    void setOrientation (Vec3 dir) noexcept { const auto n = dir.normalised(); orientation = (n.lengthSquared() > 0.0f) ? n : orientation; }

    /** Returns a unit emission direction with weighting appropriate to the pattern.
        Omni: uniform on the sphere. Directional patterns would importance-sample
        toward the lobe — overriding this without touching the ray tracer.
        `random` is the RNG used for stochastic sampling. */
    Vec3 sampleEmissionDirection (juce::Random& random) const noexcept;

    /** Pattern amplitude gain (0..1+) for an emission in the given (unit) direction.
        Omni: always 1.0. (Energy weighting squares this — see RayTracer.) */
    float getEmissionGain (Vec3 emissionDirection) const noexcept;

private:
    Vec3 position { 0.0f, 0.0f, 1.5f };  // 1.5 m off the ground by default
    SourcePattern pattern = SourcePattern::Omnidirectional;
    Vec3 orientation { 1.0f, 0.0f, 0.0f };  // default facing +X
};
} // namespace Worldizer
