#include "RenderThread.h"
#include "../DSP/ConvolutionEngine.h"
#include "../DSP/RayTracer.h"
#include "../DSP/IRBuilder.h"
#include "../Model/TestScenes.h"

namespace Worldizer
{
RenderThread::RenderThread (ConvolutionEngine& engineToUse)
    : juce::Thread ("Worldizer Render"), engine (engineToUse)
{
    startThread (juce::Thread::Priority::low);
}

RenderThread::~RenderThread()
{
    signalThreadShouldExit();
    wakeup.signal();
    stopThread (2000);
}

void RenderThread::requestRender (const Job& job)
{
    {
        const juce::ScopedLock sl (jobLock);
        pendingJob = std::make_unique<Job> (job);
    }
    wakeup.signal();
}

juce::String RenderThread::getLastRenderedScene() const
{
    const juce::ScopedLock sl (lastSceneLock);
    return lastRenderedScene;
}

void RenderThread::run()
{
    while (! threadShouldExit())
    {
        wakeup.wait(); // blocks until a job is queued (or shutdown)

        if (threadShouldExit())
            break;

        // Take the most recent pending job.
        std::unique_ptr<Job> job;
        {
            const juce::ScopedLock sl (jobLock);
            job = std::move (pendingJob);
        }

        if (job == nullptr)
            continue;

        rendering.store (true);

        bool ok = false;
        Scene scene = TestScenes::byName (job->sceneName, ok);

        if (ok)
        {
            RayTracer tracer;
            RayTracer::Settings traceSettings;
            traceSettings.numRays    = job->numRays;
            traceSettings.maxBounces = job->maxBounces;
            traceSettings.randomSeed = job->randomSeed;

            const auto result = tracer.trace (scene, traceSettings, 48000);

            IRBuilder builder;
            IRBuilder::Settings irSettings;
            irSettings.sampleRate = 48000;
            const auto ir = builder.build (result, irSettings);

            // Don't clobber the idle convolver while a crossfade is still running.
            while (engine.isIRPending() && ! threadShouldExit())
                juce::Thread::sleep (2);

            if (! threadShouldExit())
            {
                engine.loadIR (ir, 48000.0, job->crossfadeMs);
                const juce::ScopedLock sl (lastSceneLock);
                lastRenderedScene = job->sceneName;
            }
        }

        rendering.store (false);
    }
}
} // namespace Worldizer
