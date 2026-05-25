#pragma once

#include <JuceHeader.h>

namespace Worldizer
{
/**
    The microphone character: convolution with a selected mic IR plus an optional
    self-noise generator. The mic IR captures the microphone's frequency and
    transient character. Sits last in the signal chain, after the room convolution.

    Slice 5 implements IR loading, the convolver, and the self-noise stage.
*/
class MicCharacter
{
public:
    MicCharacter() = default;

    void prepare (const juce::dsp::ProcessSpec& spec);
    void reset();

    /** Select a mic IR (loaded off the audio thread). */
    void setIR (juce::AudioBuffer<float>&& ir, double irSampleRate);

    /** Self-noise level, 0 (off) .. 1. */
    void setSelfNoise (float level);

    /** Process in place. Real-time safe. */
    void process (juce::AudioBuffer<float>& buffer);

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MicCharacter)
};
} // namespace Worldizer
