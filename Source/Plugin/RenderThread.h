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

    ALL room-engine IR loads must flow through this thread (including pre-baked
    preset IRs via requestIRLoad): juce::dsp::Convolution's background command
    queue is single-producer, so concurrent loadIR calls from the message thread
    and this thread would race it (bad_function_call — the same class of crash
    pluginval caught in the character stages). This thread also owns the
    wait-until-idle discipline that keeps loads off a mid-crossfade convolver.
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
        the thread is idle. Call before re-preparing the engine (prepareToPlay):
        the engine must not be mutated while this thread might be loading into it.
        The abort also covers the case where audio is stopped (a gated wait on
        isIRPending() would otherwise never clear). Worst case blocks for the
        remainder of an in-flight trace (~0.2 s full quality). */
    void drain();

private:
    void run() override;

    ConvolutionEngine& engine;

    mutable juce::CriticalSection jobLock;
    std::unique_ptr<Job>  pendingJob;
    juce::WaitableEvent   wakeup;
    std::atomic<bool>     rendering { false };  // trace in progress (UI indicator)
    std::atomic<bool>     busy      { false };  // ANY job in progress (drain gate)
    std::atomic<bool>     abortWait { false };  // drain(): bail out of gated waits

    mutable juce::CriticalSection lastRenderLock;
    RenderedIR lastFullRender;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RenderThread)
};
} // namespace Worldizer
