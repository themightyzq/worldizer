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

    Threading contract. A juce::dsp::Convolution must never have two of its
    methods running at once: its background command queue is single-producer,
    and loadImpulseResponse(), reset() and process() all push to it. Two
    concurrent pushes double-commit the FIFO, and JUCE's loader thread then calls
    an empty FixedSizeFunction, which throws std::bad_function_call and
    terminates the host. So each convolver has exactly one owner at a time:
      - loadIR(), prepare() and reset() may be called from ANY non-audio thread
        (render thread, message thread, a host thread running prepareToPlay).
        They serialise on loaderLock, which the audio thread never takes.
      - The audio thread owns convolverB only while `state` is Crossfading.
        loadIR() refuses (returns Busy) in that state, and the audio thread
        returns B to the loaders (state = Idle) only after the role swap.
      - A requested swap the audio thread has not started yet (SwapRequested) is
        retracted by a compare-exchange and replaced, so a host that stops
        calling process() can never wedge a loader behind an unconsumed swap.
      - prepare() and reset() still require that process() is not running
        (the host contract for prepareToPlay / releaseResources).
*/
class ConvolutionEngine
{
public:
    ConvolutionEngine();
    ~ConvolutionEngine();

    // === Lifecycle (message thread) ===
    void prepare (double sampleRate, int maximumBlockSize, int numChannels);
    void reset();

    // === IR loading (any non-audio thread; never the audio thread) ===
    enum class LoadResult
    {
        loaded,       // queued; the audio thread crossfades to it on its next block
        busy,         // a crossfade is running on the audio thread: retry later
        notPrepared   // dropped: prepare() has not run (it re-arms all IRs anyway)
    };

    /** Loads a new IR and triggers a crossfade on subsequent process() calls.
        Replaces a load that the audio thread has not started yet (latest wins).
        Returns busy, without loading, while a crossfade is in progress.
        The buffer is copied, so the caller may release it after this returns.

        normalise=true (room IRs): JUCE's energy normalisation (0.125/sqrt(E)) —
        consistent wet level across scenes, tuned by ear since Slice 2.
        normalise=false (character IRs): the IRs are pre-normalised to UNIT
        ENERGY at bake, so characters — including the unit-impulse "none", whose
        energy is exactly 1 — pass at unity loudness. (JUCE's normalisation
        scales even a unit delta to 0.125 = -18 dB, which would make each
        character stage a fixed 18 dB pad and "none" anything but a bypass.) */
    LoadResult loadIR (const juce::AudioBuffer<float>& ir, double irSampleRate,
                       float crossfadeMs = 80.0f, bool normalise = true);

    /** True if a swap is queued or a crossfade is in progress (informational;
        loadIR() does its own gating). */
    bool isIRPending() const noexcept;

    // === Real-time processing (audio thread only) ===
    void process (juce::dsp::AudioBlock<float> block);

    // === Latency (message thread) ===
    int getLatencySamples() const noexcept;

private:
    std::unique_ptr<juce::dsp::Convolution> convolverA; // current
    std::unique_ptr<juce::dsp::Convolution> convolverB; // incoming during crossfade

    juce::AudioBuffer<float> scratchA, scratchB;

    // Ownership of convolverB (see the class comment). One atomic, so there is no
    // instant at which a loader can see "idle" while the audio thread is taking B.
    enum State : int { idle = 0, swapRequested = 1, crossfading = 2 };
    std::atomic<int> state            { idle };
    std::atomic<int> crossfadeSamples { 0 };

    // Serialises every non-audio caller (loadIR / prepare / reset). Never taken
    // by process().
    juce::CriticalSection loaderLock;

    // Audio-thread-only crossfade state.
    int  crossfadeTotal = 0;
    int  crossfadeRemaining = 0;
    bool crossfadeActive = false;

    double sampleRate   = 48000.0;
    int    maxBlockSize = 512;
    int    numChannels  = 2;
    std::atomic<bool> prepared { false }; // written by prepare(), read by loader threads

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ConvolutionEngine)
};
} // namespace Worldizer
