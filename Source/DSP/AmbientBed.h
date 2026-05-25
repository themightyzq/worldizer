#pragma once

#include <JuceHeader.h>

namespace wz
{
/**
    Looped playback of a recorded room-tone WAV with a seamless crossfade at the
    loop points. Mixed under the convolved (worldized) signal at a per-preset
    level. Real-time safe: the sample buffer is loaded off the audio thread.

    Slice 6 implements loading, looping, crossfade, and level control.
*/
class AmbientBed
{
public:
    AmbientBed() = default;

    void prepare (const juce::dsp::ProcessSpec& spec);
    void reset();

    /** Set the room-tone sample (loaded off the audio thread). */
    void setSample (juce::AudioBuffer<float>&& sample, double sampleRate);

    /** Linear mix level, 0..1. */
    void setLevel (float level);

    /** Add the bed into the buffer in place. Real-time safe. */
    void addToBuffer (juce::AudioBuffer<float>& buffer);

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AmbientBed)
};
} // namespace wz
