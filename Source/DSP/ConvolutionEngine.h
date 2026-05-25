#pragma once

#include <JuceHeader.h>

namespace wz
{
/**
    Wraps juce::dsp::Convolution for the audio thread. Supports loading a new IR
    from the background thread with a smooth crossfade in processBlock — the
    audio thread never blocks; new IRs arrive via a lock-free hand-off.

    Real-time rules: no allocations, no locks, no file I/O in process().

    Slice 2 implements the convolver, the lock-free IR hand-off, and crossfade.
*/
class ConvolutionEngine
{
public:
    ConvolutionEngine() = default;

    /** Allocate processing buffers. Called from prepareToPlay (not real-time). */
    void prepare (const juce::dsp::ProcessSpec& spec);

    /** Release resources. */
    void reset();

    /** Queue a new IR for crossfade. Safe to call from the background thread. */
    void loadIR (juce::AudioBuffer<float>&& ir, double irSampleRate);

    /** Process a block in place. Real-time safe. */
    void process (juce::AudioBuffer<float>& buffer);

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ConvolutionEngine)
};
} // namespace wz
