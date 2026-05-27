#pragma once

#include <JuceHeader.h>
#include "RayTracer.h"
#include "../Shared/Constants.h"

namespace Worldizer
{
/**
    Builds a time-domain mono impulse response from ray-tracer output.

    Reconstruction (standard auralization method): for each octave band, the
    per-bin energy histogram is smoothed into an energy-vs-time envelope; a shared
    white-noise carrier is band-pass filtered to that octave, normalised to unit
    RMS, then amplitude-modulated by sqrt(energy density). Summing the bands gives
    a dense, spectrally-shaped decay where HF dies away faster than LF (each band
    carries its own decay), instead of a flat broadband noise burst. Air absorption
    is folded into each band's energy.

    Slice 4.5 — the IR is a *timing- and level-normalised room response*:
      - The direct sound is an impulse at sample 0; its amplitude keeps the 1/distance
        scaling RELATIVE to the reflections, so the direct-to-reverberant ratio (cue
        #3, distance-dependent) is preserved. Peak-normalisation afterwards removes the
        absolute level (DistanceModel re-applies it), leaving only that ratio.
      - Reflection times are stored relative to the direct (arrival - directArrival),
        so propagation delay is out of the IR (DistanceModel re-applies it as pre-delay).
        (Air absorption still uses absolute path lengths — farther paths stay darker.)
      - A statistical late tail is synthesised beyond the ray-traced echogram: each
        band's measured decay rate is continued as exponentially-decaying band noise,
        crossfaded in over the sparse late portion so the tail decays smoothly to
        silence instead of truncating at the trace limit.
    Distance cues (time-of-flight pre-delay, inverse-distance level) are applied on
    the wet path at runtime by DistanceModel, NOT baked into the IR.

    Output is peak-normalised to -1 dBFS. Early reflections are currently part of
    the diffuse envelope; discrete early reflections (image-source method) are the
    next refinement.
*/
class IRBuilder
{
public:
    struct Settings
    {
        int   sampleRate = 48000;
        int   maxLengthSamples = kMaxIRLengthSeconds * 48000;
        bool  applyAirAbsorption = true;
        float temperatureCelsius = 20.0f;
        float relativeHumidity = 50.0f;
        bool  includeDirect = true;
        float directGainCompensation = 1.0f;
        float envelopeMs = 6.0f; // energy-envelope smoothing window (diffuse-field time resolution)

        // === Statistical late-tail synthesis (Slice 4.5) ===
        bool  synthesizeLateTail = true;  // false => IR ends at the ray-traced echogram (raw trace)
        float tailExtensionSeconds = 4.0f; // max synthesised extension beyond the trace cutoff
        float crossfadeSeconds = 0.2f;     // ray-traced -> synthesised crossfade window
        float finalFadeSeconds = 0.05f;    // last-N-ms linear fade guaranteeing clean silence
    };

    IRBuilder() = default;

    /** Builds a mono IR from a ray-tracer result. */
    juce::AudioBuffer<float> build (const RayTracer::Result& traceResult,
                                    const Settings& settings) const;
};
} // namespace Worldizer
