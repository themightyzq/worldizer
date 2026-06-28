#include "SourceNode.h"

namespace Worldizer
{
SourceNode::SourceNode (Vec3 p) : position (p) {}

Vec3 SourceNode::sampleEmissionDirection (juce::Random& random) const noexcept
{
    // MVP: omnidirectional — uniform sampling on the unit sphere (Archimedes /
    // cylinder method). Directional patterns would importance-sample here.
    const float z     = 2.0f * random.nextFloat() - 1.0f;            // [-1, 1]
    const float theta = juce::MathConstants<float>::twoPi * random.nextFloat();
    const float r     = std::sqrt (juce::jmax (0.0f, 1.0f - z * z));

    return { r * std::cos (theta), r * std::sin (theta), z };
}

float SourceNode::getEmissionGain (Vec3 /*emissionDirection*/) const noexcept
{
    switch (pattern)
    {
        case SourcePattern::Omnidirectional:
        default:
            return 1.0f;
        // Future: Cardioid = 0.5*(1+cos(angle)); NarrowCone = smooth cone falloff.
    }
}
} // namespace Worldizer
