#pragma once

#include <JuceHeader.h>
#include "../Model/Scene.h"

namespace wz
{
/**
    Closed-form image-source method for low-order specular reflections. Faster and
    more accurate than ray tracing for the first few bounces off nearby surfaces.

    Used in hybrid mode: ISM handles reflection orders 1-3, the RayTracer handles
    everything beyond, and a statistical model synthesises the tail (~80 ms+).

    Slice 1 implements the image-source enumeration and visibility checks.
*/
class ImageSourceSolver
{
public:
    /** A single image source: a virtual source position plus accumulated
        per-band reflection gain. Defined in Slice 1. */
    struct ImageSource;

    ImageSourceSolver() = default;

    /** Enumerate image sources up to maxOrder for the given scene. */
    std::vector<ImageSource> solve (const Scene& scene, int maxOrder);

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ImageSourceSolver)
};
} // namespace wz
