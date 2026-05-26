#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <memory>

namespace Worldizer
{
class ConvolutionEngine;

/**
    Background thread that runs the ray tracer and builds IRs, then hands the
    finished IR to the ConvolutionEngine. Pulls render jobs from a one-deep queue
    that always keeps the most recent request (rapid scene changes collapse to the
    last one). Owned by the processor; lower priority than the audio thread.
*/
class RenderThread : private juce::Thread
{
public:
    struct Job
    {
        juce::String sceneName;        // test-scene name (Slice 2); a Scene snapshot later
        int   numRays    = 50000;
        int   maxBounces = 32;
        int   randomSeed = 12345;
        float crossfadeMs = 80.0f;
    };

    explicit RenderThread (ConvolutionEngine& engineToUse);
    ~RenderThread() override;

    /** Submits a job, replacing any pending one. Thread-safe. */
    void requestRender (const Job& job);

    bool isRendering() const noexcept    { return rendering.load(); }

    juce::String getLastRenderedScene() const;

private:
    void run() override;

    ConvolutionEngine& engine;

    juce::CriticalSection jobLock;
    std::unique_ptr<Job>  pendingJob;
    juce::WaitableEvent   wakeup;

    std::atomic<bool>     rendering { false };

    juce::CriticalSection lastSceneLock;
    juce::String          lastRenderedScene;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RenderThread)
};
} // namespace Worldizer
