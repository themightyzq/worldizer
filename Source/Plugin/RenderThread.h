#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <memory>
#include "../Model/Scene.h"

namespace Worldizer
{
class ConvolutionEngine;

/**
    Background thread that ray-traces a Scene snapshot, builds an IR, and hands it
    to the ConvolutionEngine. One-deep replacement queue: rapid requests (e.g. a
    source/mic drag) collapse to the most recent. Low priority; never blocks audio.

    Room-engine IR loads normally flow through this thread (including pre-baked
    preset IRs via requestIRLoad) so they stay off the message thread and keep
    their submission order. ConvolutionEngine serialises its own loaders, so a
    direct engine load from another thread (offline prepareToPlay) is safe too.

    Hand-off: the engine refuses a load only while the audio thread is mid-
    crossfade. This thread then retries, but gives up on its IR as soon as a newer
    job is queued (latest wins) or drain() is called. A host that stops calling
    processBlock therefore never leaves the "rendering..." indicator stuck: the
    indicator covers the trace only, and an unconsumed swap is simply replaced.
*/
class RenderThread : private juce::Thread
{
public:
    struct Job
    {
        enum class Quality { Preview, Full };

        Scene   scene;                  // geometry + source/mic positions to render
        Quality quality = Quality::Full;
        float   crossfadeMs = 80.0f;

        // Pre-baked IR: when non-empty, the scene is NOT traced — the IR is
        // handed to the engine directly (preset loads are file reads, not renders).
        juce::AudioBuffer<float> bakedIR;
        double bakedIRSampleRate = 48000.0;
    };

    explicit RenderThread (ConvolutionEngine& engineToUse);
    ~RenderThread() override;

    /** Submits a job, replacing any pending one. Thread-safe. */
    void requestRender (const Job& job);

    /** Submits a pre-baked IR load (replaces any pending job). Thread-safe. */
    void requestIRLoad (const juce::AudioBuffer<float>& ir, double irSampleRate, float crossfadeMs);

    bool isRendering() const noexcept { return rendering.load(); }

    /** Copy of the most recent FULL-quality scene render (empty if none yet).
        Used to cache the edited-scene IR in plugin state so a session restore
        never re-renders (Soundminer requirement). Thread-safe. */
    struct RenderedIR
    {
        juce::AudioBuffer<float> ir;
        double sampleRate = 48000.0;
        juce::String sceneSignature;   // sceneSignature() of the rendered scene
    };
    RenderedIR getLastFullRender() const;

    /** Deterministic signature of a scene's render-relevant interactive state
        (source/mic transforms + patterns + sector geometry). Two scenes with
        equal signatures produce the same full-quality render, so a cached IR
        tagged with the signature can safely stand in for a re-render. */
    static juce::String sceneSignature (const Scene& scene);

    /** Discards any pending job, aborts a gated engine hand-off, and BLOCKS until
        the thread is idle. Call before re-preparing the engine (prepareToPlay) so
        no job aimed at the old configuration lands afterwards (the engine's own
        lock already keeps a load from overlapping prepare/reset).
        The abort also covers a hand-off retrying against a crossfade that
        cannot finish because audio is stopped. Worst case blocks for the
        remainder of an in-flight trace (~0.2 s full quality). */
    void drain();

private:
    void run() override;

    ConvolutionEngine& engine;

    mutable juce::CriticalSection jobLock;
    std::unique_ptr<Job>  pendingJob;
    juce::WaitableEvent   wakeup;
    void handOff (const juce::AudioBuffer<float>& ir, double irSampleRate, float crossfadeMs);

    std::atomic<bool>     rendering { false };  // trace in progress (UI indicator)
    std::atomic<bool>     busy      { false };  // ANY job in progress (drain gate);
                                                // set under jobLock with the job pop
    std::atomic<bool>     abortWait { false };  // drain(): bail out of gated waits

    mutable juce::CriticalSection lastRenderLock;
    RenderedIR lastFullRender;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RenderThread)
};
} // namespace Worldizer
