#pragma once

#include <JuceHeader.h>
#include "RayTracer.h"
#include "../Shared/Constants.h"

namespace Worldizer
{
/**
    Builds a time-domain mono impulse response from ray-tracer output.

    The direct sound is written as a deterministic impulse (amplitude 1/distance);
    reflections are reconstructed from the per-band energy histogram as a
    noise-shaped decay, calibrated to the direct sound's scale. Air absorption is
    baked in. The output is peak-normalised to -1 dBFS.

    Slice 1 uses the pragmatic broadband reconstruction (one shaped sample per bin
    from the summed, air-weighted band energies); proper per-band noise filtering
    is a documented follow-up if the result sounds too coloured.
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
    };

    IRBuilder() = default;

    /** Builds a mono IR from a ray-tracer result. */
    juce::AudioBuffer<float> build (const RayTracer::Result& traceResult,
                                    const Settings& settings) const;
};
} // namespace Worldizer
