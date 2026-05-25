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
    is folded into each band's energy. The direct sound is added as a clean impulse
    (amplitude 1/distance). Output is peak-normalised to -1 dBFS.

    Early reflections are currently part of the diffuse envelope; discrete early
    reflections (image-source method) are the next refinement.
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
    };

    IRBuilder() = default;

    /** Builds a mono IR from a ray-tracer result. */
    juce::AudioBuffer<float> build (const RayTracer::Result& traceResult,
                                    const Settings& settings) const;
};
} // namespace Worldizer
