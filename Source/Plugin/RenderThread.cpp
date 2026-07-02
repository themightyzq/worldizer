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

void RenderThread::requestIRLoad (const juce::AudioBuffer<float>& ir, double irSampleRate, float crossfadeMs)
{
    Job job;
    job.bakedIR           = ir;
    job.bakedIRSampleRate = irSampleRate;
    job.crossfadeMs       = crossfadeMs;
    requestRender (job);
}

bool RenderThread::isBusy() const
{
    const juce::ScopedLock sl (jobLock);
    return rendering.load() || pendingJob != nullptr;
}

RenderThread::RenderedIR RenderThread::getLastFullRender() const
{
    const juce::ScopedLock sl (lastRenderLock);
    return lastFullRender;
}

juce::String RenderThread::sceneSignature (const Scene& scene)
{
    juce::String sig;
    auto vec = [] (Vec3 v) { return juce::String (v.x, 4) + "," + juce::String (v.y, 4) + "," + juce::String (v.z, 4) + ";"; };

    const auto& src = scene.getSource();
    sig << vec (src.getPosition()) << vec (src.getOrientation()) << (int) src.getPattern() << ";";

    const auto& arr = scene.getMicArray();
    sig << (int) arr.getConfiguration() << ";" << (int) arr.getPattern() << ";"
        << juce::String (arr.getXYAngleDegrees(), 2) << ";";
    for (int m = 0; m < arr.getNumMics(); ++m)
        sig << vec (arr.getMic (m).getPosition()) << vec (arr.getMic (m).getOrientation());

    if (! scene.getSectorGeometry().isEmpty())
        sig << juce::JSON::toString (scene.getSectorGeometry().toJson(), true);
    return sig;
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

        // Pre-baked IR: no trace, just the gated hand-off (fast, so no
        // "rendering..." indicator).
        if (job->bakedIR.getNumSamples() > 0)
        {
            while (engine.isIRPending() && ! threadShouldExit())
                juce::Thread::sleep (2);
            if (! threadShouldExit())
                engine.loadIR (job->bakedIR, job->bakedIRSampleRate, job->crossfadeMs);
            continue;
        }

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

        if (job->quality == Job::Quality::Full)
        {
            const auto signature = sceneSignature (job->scene);
            const juce::ScopedLock sl (lastRenderLock);
            lastFullRender.ir.makeCopyOf (ir);
            lastFullRender.sampleRate = 48000.0;
            lastFullRender.sceneSignature = signature;
        }

        // Don't clobber the idle convolver mid-crossfade.
        while (engine.isIRPending() && ! threadShouldExit())
            juce::Thread::sleep (2);

        if (! threadShouldExit())
            engine.loadIR (ir, 48000.0, job->crossfadeMs);

        rendering.store (false);
    }
}
} // namespace Worldizer
