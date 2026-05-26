#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <memory>

namespace Worldizer
{
/**
    Real-time convolution runtime with click-free IR hot-swap.

    Owns two juce::dsp::Convolution instances. New IRs are loaded into the idle
    convolver from a background thread (juce's loadImpulseResponse is wait-free and
    prepares the IR on its own loader thread). On the next process() call the audio
    thread runs both convolvers in parallel and crossfades old -> new over a linear
    ramp, then swaps roles. The audio thread never allocates, locks, or does I/O.

    The mono IR is applied to every channel independently (pseudo-stereo); true
    stereo IRs are a later concern.
*/
class ConvolutionEngine
{
public:
    ConvolutionEngine();
    ~ConvolutionEngine();

    // === Lifecycle (message thread) ===
    void prepare (double sampleRate, int maximumBlockSize, int numChannels);
    void reset();

    // === IR loading (background / message thread; never the audio thread) ===
    /** Loads a new IR and triggers a crossfade on subsequent process() calls.
        The buffer is copied, so the caller may release it after this returns. */
    void loadIR (const juce::AudioBuffer<float>& ir, double irSampleRate, float crossfadeMs = 80.0f);

    /** True if a swap is queued or a crossfade is in progress. Callers loading a
        new IR should wait until this is false so they don't clobber the idle
        convolver mid-crossfade. */
    bool isIRPending() const noexcept;

    // === Real-time processing (audio thread only) ===
    void process (juce::dsp::AudioBlock<float> block);

    // === Latency (message thread) ===
    int getLatencySamples() const noexcept;

private:
    std::unique_ptr<juce::dsp::Convolution> convolverA; // current
    std::unique_ptr<juce::dsp::Convolution> convolverB; // incoming during crossfade

    juce::AudioBuffer<float> scratchA, scratchB;

    std::atomic<bool> swapRequested      { false };
    std::atomic<int>  crossfadeSamples   { 0 };
    std::atomic<bool> crossfadeInProgress { false };

    // Audio-thread-only crossfade state.
    int  crossfadeTotal = 0;
    int  crossfadeRemaining = 0;
    bool crossfadeActive = false;

    double sampleRate   = 48000.0;
    int    maxBlockSize = 512;
    int    numChannels  = 2;
    bool   prepared     = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ConvolutionEngine)
};
} // namespace Worldizer
