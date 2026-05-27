#include "RenderThread.h"
#include "../DSP/ConvolutionEngine.h"
#include "../DSP/RayTracer.h"
#include "../DSP/IRBuilder.h"

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

void RenderThread::run()
{
    while (! threadShouldExit())
    {
        wakeup.wait();

        if (threadShouldExit())
            break;

        std::unique_ptr<Job> job;
        {
            const juce::ScopedLock sl (jobLock);
            job = std::move (pendingJob);
        }

        if (job == nullptr)
            continue;

        rendering.store (true);

        RayTracer tracer;
        RayTracer::Settings rt;
        if (job->quality == Job::Quality::Preview)
        {
            rt.numRays = 5000;
            rt.maxBounces = 12;
        }
        else
        {
            rt.numRays = 50000;
            rt.maxBounces = 32;
        }
        rt.randomSeed = 12345;

        const auto result = tracer.trace (job->scene, rt, 48000);

        IRBuilder builder;
        IRBuilder::Settings irSettings;
        irSettings.sampleRate = 48000;
        // Statistical late-tail synthesis only on the full (drag-release) render —
        // it keeps the preview render fast and snappy during a drag. The full IR that
        // replaces it on release carries the smooth, fade-to-silence tail.
        irSettings.synthesizeLateTail = (job->quality == Job::Quality::Full);
        const auto ir = builder.build (result, irSettings);

        // Don't clobber the idle convolver mid-crossfade.
        while (engine.isIRPending() && ! threadShouldExit())
            juce::Thread::sleep (2);

        if (! threadShouldExit())
            engine.loadIR (ir, 48000.0, job->crossfadeMs);

        rendering.store (false);
    }
}
} // namespace Worldizer
