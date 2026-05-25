#pragma once

#include <JuceHeader.h>
#include "../Shared/Vec3.h"

namespace Worldizer
{
/**
    A sound source in the scene. For MVP, sources are omnidirectional; directivity
    patterns come in v1.0.
*/
class SourceNode
{
public:
    SourceNode() = default;
    explicit SourceNode (Vec3 position);

    Vec3 getPosition() const noexcept       { return position; }
    void setPosition (Vec3 p) noexcept      { position = p; }

    /** Returns a unit direction for ray emission (omnidirectional for MVP).
        `random` is the RNG used for stochastic sampling. */
    Vec3 sampleEmissionDirection (juce::Random& random) const noexcept;

private:
    Vec3 position { 0.0f, 0.0f, 1.5f };  // 1.5 m off the ground by default
};
} // namespace Worldizer
