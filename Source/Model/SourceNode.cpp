#include "SourceNode.h"

namespace Worldizer
{
SourceNode::SourceNode (Vec3 p) : position (p) {}

Vec3 SourceNode::sampleEmissionDirection (juce::Random& random) const noexcept
{
    // Uniform sampling on the unit sphere (Archimedes / cylinder method).
    const float z     = 2.0f * random.nextFloat() - 1.0f;            // [-1, 1]
    const float theta = juce::MathConstants<float>::twoPi * random.nextFloat();
    const float r     = std::sqrt (juce::jmax (0.0f, 1.0f - z * z));

    return { r * std::cos (theta), r * std::sin (theta), z };
}
} // namespace Worldizer
