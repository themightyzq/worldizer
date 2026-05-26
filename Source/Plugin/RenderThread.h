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
    };

    explicit RenderThread (ConvolutionEngine& engineToUse);
    ~RenderThread() override;

    /** Submits a job, replacing any pending one. Thread-safe. */
    void requestRender (const Job& job);

    bool isRendering() const noexcept { return rendering.load(); }

private:
    void run() override;

    ConvolutionEngine& engine;

    juce::CriticalSection jobLock;
    std::unique_ptr<Job>  pendingJob;
    juce::WaitableEvent   wakeup;
    std::atomic<bool>     rendering { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RenderThread)
};
} // namespace Worldizer
