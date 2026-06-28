#pragma once

#include <JuceHeader.h>
#include <array>
#include <vector>
#include "../Model/Scene.h"
#include "../Model/MicArray.h"
#include "../Model/Material.h"
#include "../Shared/Constants.h"

namespace Worldizer
{
/**
    Monte Carlo ray tracer for acoustic IR generation. Emits rays from the source,
    traces specular and diffuse bounces, and accumulates energy at each mic in the
    array in time-binned per-band histograms. Pure C++ core (uses juce::Random as a
    utility only) — safe on a worker thread and deterministic for a given seed.

    Directional acoustics (Slice 5): each emitted ray is weighted by the source's
    emission pattern, and each mic deposit is weighted by that mic's reception
    pattern in the incoming direction. Both weightings are amplitude gains applied
    in the ENERGY domain (squared), so after IRBuilder's sqrt() the IR amplitude
    scales linearly with pattern gain — consistent with the direct sound's scaling.
    For omni source + omni mic both gains are 1.0, so output is bit-identical to
    Slice 4.5.

    A single ray can be received by BOTH mics in a 2-mic array (mics receive, they do
    not absorb), so rays are never terminated by a mic hit. The direct sound is
    handled separately (deterministic) per mic; the histogram holds reflections only.
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

    /** Per-mic energy histograms at the mics, plus per-mic direct-sound info.
        histogramsPerMic[mic][band][bin] = reflected energy at that mic/band/time.

        numRays and micRadius are recorded so IRBuilder can normalise reflection
        amplitudes consistently with the direct sound's 1/d law. */
    struct Result
    {
        int sampleRate = 48000;
        int numBins = 0;        // = ceil(maxTraceTimeSeconds * sampleRate)
        int numRays = 0;        // total rays cast (for normalisation)
        float micRadius = 0.1f; // acceptance radius used (for normalisation)
        bool anyMicDirectional = false; // any mic is non-omni => IRBuilder smooths the
                                        // (effectively sparser) envelope more (anti-pump)

        // Per-mic reflected-energy histograms. Size() == number of mics.
        std::vector<std::array<std::vector<float>, Material::kNumBands>> histogramsPerMic;

        struct DirectInfo
        {
            float distance = 0.0f;
            float arrivalTime = 0.0f;
            bool  visible = false;
            // Amplitude gain for the direct: the mic's reception pattern in the
            // source->mic direction times the source's emission pattern in the
            // mic direction (both 1.0 for omni source + omni mic).
            float receptionGain = 1.0f;
        };
        std::vector<DirectInfo> directPerMic;
        std::vector<Vec3>       micPositions;   // for IRBuilder coincidence detection

        std::vector<int> hitCountPerMic;

        int getNumMics() const noexcept { return (int) histogramsPerMic.size(); }
    };

    RayTracer() = default;

    /** Runs the trace. Pure: same scene + settings + seed => identical result. */
    Result trace (const Scene& scene, const Settings& settings, int sampleRate = 48000) const;

private:
    void traceRay (const std::vector<Brush>& brushes,
                   const MicArray& micArray,
                   Vec3 origin,
                   Vec3 direction,
                   float emissionGain,
                   const Settings& settings,
                   Result& result,
                   juce::Random& random) const;
};
} // namespace Worldizer
