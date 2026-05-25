#pragma once

#include <JuceHeader.h>
#include "../Model/Scene.h"
#include "../Shared/Constants.h"

namespace wz
{
/**
    Monte Carlo ray tracer. Emits rays from each source, bounces them through the
    Scene with frequency-dependent absorption and scattering, and accumulates a
    per-mic, per-band energy histogram. Pure compute — no JUCE audio-thread
    dependencies — so it can run headless and on the background thread.

    Two quality modes (see Constants.h):
      - preview: ~kRaysPreview rays, no late tail, for real-time drag feedback.
      - full:    ~kRaysFull rays + statistical tail, for final renders.

    Slice 1 implements tracing; the result type is fleshed out alongside IRBuilder.
*/
class RayTracer
{
public:
    enum class Quality { preview, full };

    /** Per-mic energy histograms + direct-sound info. Defined in Slice 1. */
    struct Result;

    RayTracer() = default;

    /** Trace the scene at the requested quality, producing energy histograms. */
    Result trace (const Scene& scene, Quality quality);

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RayTracer)
};
} // namespace wz
