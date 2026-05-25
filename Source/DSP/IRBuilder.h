#pragma once

#include <JuceHeader.h>
#include "RayTracer.h"

namespace wz
{
/**
    Converts ray-tracer / image-source output (per-band energy histograms plus
    direct-sound info) into a stereo (later multi-mic) impulse response.

    Synthesises direct sound + early reflections + late statistical tail, applies
    air absorption, and writes a float buffer suitable for ConvolutionEngine or
    for saving as rendered.wav (48 kHz, capped at kMaxIRLengthSeconds).

    Slice 1 implements the synthesis.
*/
class IRBuilder
{
public:
    IRBuilder() = default;

    /** Build a stereo IR at sampleRate from a ray-trace result. */
    juce::AudioBuffer<float> build (const RayTracer::Result& result, double sampleRate);

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (IRBuilder)
};
} // namespace wz
