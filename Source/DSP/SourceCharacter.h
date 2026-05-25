#pragma once

#include <JuceHeader.h>

namespace wz
{
/**
    The reproducer (speaker) character: convolution with a selected speaker IR
    plus an optional light nonlinearity. The speaker IR captures the reproducer's
    frequency response and cabinet resonances. Sits first in the signal chain,
    before the room convolution.

    Slice 5 implements IR loading, the convolver, and the saturation stage.
*/
class SourceCharacter
{
public:
    SourceCharacter() = default;

    void prepare (const juce::dsp::ProcessSpec& spec);
    void reset();

    /** Select a speaker IR (loaded off the audio thread). */
    void setIR (juce::AudioBuffer<float>&& ir, double irSampleRate);

    /** Drive of the optional nonlinearity, 0 (clean) .. 1. */
    void setDrive (float drive);

    /** Process in place. Real-time safe. */
    void process (juce::AudioBuffer<float>& buffer);

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SourceCharacter)
};
} // namespace wz
