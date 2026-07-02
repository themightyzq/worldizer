#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <optional>
#include "ConvolutionEngine.h"

namespace Worldizer
{
/**
    The reproducer (speaker) character: convolution with a selected speaker IR
    plus an optional light nonlinearity. The speaker IR captures the reproducer's
    frequency response and cabinet resonances. Sits first in the WET signal
    chain, before the room convolution (per architecture: the speaker plays INTO
    the room).

    Convolution uses the proven double-convolver ConvolutionEngine (same as the
    room IR), NOT a single juce::dsp::Convolution with its internal crossfade:
    a single instance's background command queue is pushed from BOTH the loading
    thread and the audio thread (engine retirement), which is a documented
    single-producer queue — the race corrupts the FIFO and throws
    bad_function_call (found by pluginval). The double-convolver never loads
    into an instance the audio thread is transitioning, so each queue keeps one
    producer. Loads are serialized by a one-deep pending slot + timer poll on
    isIRPending(), so hover-audition can request swaps as fast as the mouse
    moves and they converge to the latest.

    "None" loads a unit impulse (identity convolution) so switching to/from none
    rides the same click-free crossfade path.

    Threading: setIR/setNone message thread; setDrive atomic (any thread);
    process() audio-thread only — no allocation, locks, or I/O.
*/
class SourceCharacter : private juce::Timer
{
public:
    SourceCharacter() = default;
    ~SourceCharacter() override { stopTimer(); }

    void prepare (const juce::dsp::ProcessSpec& spec);
    void reset();

    /** Select a speaker IR (message thread). The buffer is moved. */
    void setIR (juce::AudioBuffer<float>&& ir, double irSampleRate);

    /** Select the "none" character (unit impulse; message thread). */
    void setNone();

    /** Drive of the optional light nonlinearity, 0 (clean, bit-exact) .. 1. */
    void setDrive (float drive);

    /** Process in place. Real-time safe. */
    void process (juce::dsp::AudioBlock<float> block);

private:
    void timerCallback() override;
    void requestIR (juce::AudioBuffer<float>&& ir, double irSampleRate);
    void flushPendingIfIdle();

    ConvolutionEngine engine;

    struct PendingIR { juce::AudioBuffer<float> buffer; double sampleRate; };
    std::optional<PendingIR> pending;   // message-thread only; one-deep, latest wins

    std::atomic<float> driveTarget { 0.0f };
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> driveSmoothed;
    double sampleRate = 48000.0;
    bool prepared = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SourceCharacter)
};
} // namespace Worldizer
