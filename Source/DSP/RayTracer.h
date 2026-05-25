#pragma once

#include <JuceHeader.h>
#include <array>
#include <vector>
#include "../Model/Scene.h"
#include "../Model/Material.h"
#include "../Shared/Constants.h"

namespace Worldizer
{
/**
    Monte Carlo ray tracer for acoustic IR generation. Emits rays from the source,
    traces specular and diffuse bounces, and accumulates energy at the mic in
    time-binned per-band histograms. Pure C++ core (uses juce::Random as a utility
    only) — safe to run on a worker thread and deterministic for a given seed.

    The direct sound is handled separately (deterministic), so the histogram holds
    reflections only: mic hits are deposited from the first reflection onward.
*/
class RayTracer
{
public:
    struct Settings
    {
        int   numRays            = kRaysFull;
        int   maxBounces         = kMaxBouncesFull;
        float maxTraceTimeSeconds = 4.0f;   // ignore reflections arriving later than this
        float speedOfSound       = 343.0f;  // m/s at 20C
        int   randomSeed         = 12345;   // deterministic for testing
    };

    /** Per-band energy histogram at the mic, plus direct-sound info.
        histogram[band][bin] = accumulated reflected energy at that band and time bin.

        Note: numRays and micRadius are recorded here so IRBuilder can normalise
        reflection amplitudes consistently with the direct sound's 1/d law. */
    struct Result
    {
        int sampleRate = 48000;
        int numBins = 0;  // = ceil(maxTraceTimeSeconds * sampleRate)
        std::array<std::vector<float>, Material::kNumBands> histogram;

        float directDistance = 0.0f;
        float directArrivalTime = 0.0f;
        bool  directVisible = false;  // false if the direct path is occluded

        int   hitCount = 0;     // reflected rays deposited into the histogram
        int   numRays = 0;      // total rays cast (for normalisation)
        float micRadius = 0.1f; // acceptance radius used (for normalisation)
    };

    RayTracer() = default;

    /** Runs the trace. Pure: same scene + settings + seed => identical result. */
    Result trace (const Scene& scene, const Settings& settings, int sampleRate = 48000) const;

private:
    void traceRay (const Scene& scene,
                   Vec3 origin,
                   Vec3 direction,
                   const Settings& settings,
                   Result& result,
                   juce::Random& random) const;
};
} // namespace Worldizer
