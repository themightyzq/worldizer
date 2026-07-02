#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <optional>
#include "ConvolutionEngine.h"

namespace Worldizer
{
/**
    The microphone character: convolution with a selected mic IR plus an optional
    self-noise floor. The mic IR captures the microphone's frequency and transient
    character. Sits last in the WET signal chain, after the room convolution (the
    mic hears the room).

    Same double-convolver + pending-slot scheme as SourceCharacter (see its header
    for why a single juce::dsp::Convolution's internal crossfade is NOT safe here).

    Self-noise is a looped pink-noise bed generated in prepare() (seam blended at
    generation => seamless), added post-IR at a smoothed user level. 0 = off
    (bit-exact skip).

    Threading: setIR/setNone message thread; setSelfNoise atomic (any thread);
    process() audio-thread only — no allocation, locks, or I/O.
*/
class MicCharacter : private juce::Timer
{
public:
    MicCharacter() = default;
    ~MicCharacter() override { stopTimer(); }

    void prepare (const juce::dsp::ProcessSpec& spec);
    void reset();

    /** Select a mic IR (message thread). The buffer is moved. */
    void setIR (juce::AudioBuffer<float>&& ir, double irSampleRate);

    /** Select the "none" character (unit impulse; message thread). */
    void setNone();

    /** Self-noise amount, 0 (off) .. 1 (mapped to -70..-35 dBFS). */
    void setSelfNoise (float amount);

    /** Process in place. Real-time safe. */
    void process (juce::dsp::AudioBlock<float> block);

private:
    void timerCallback() override;
    void requestIR (juce::AudioBuffer<float>&& ir, double irSampleRate);
    void flushPendingIfIdle();

    ConvolutionEngine engine;

    struct PendingIR { juce::AudioBuffer<float> buffer; double sampleRate; };
    std::optional<PendingIR> pending;   // message-thread only; one-deep, latest wins

    std::atomic<float> noiseTarget { 0.0f };  // linear gain
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> noiseSmoothed;
    juce::AudioBuffer<float> noiseLoop;       // mono pink loop, generated in prepare
    int noisePos = 0;                          // audio-thread only

    double sampleRate = 48000.0;
    bool prepared = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MicCharacter)
};
} // namespace Worldizer
