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

void RenderThread::drain()
{
    {
        // run() pops a job and raises `busy` under this same lock, so after it
        // there is either no job left or `busy` is already true: no job can slip
        // past the wait below and start loading while the caller re-prepares.
        const juce::ScopedLock sl (jobLock);
        pendingJob = nullptr;
        abortWait.store (true);
    }
    while (busy.load() && ! threadShouldExit())
        juce::Thread::sleep (1);
    abortWait.store (false);
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

    // Brush count disambiguates shell-converted scenes (shell brushes removed)
    // from the same preset's untouched default.
    sig << ";B" << (int) scene.getNumBrushes();
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
            if (job != nullptr)
                busy.store (true); // drain() gate — covers the WHOLE job, both kinds
        }

        if (job == nullptr)
            continue;

        // Pre-baked IR: no trace, just the gated hand-off (fast, so no
        // "rendering..." indicator).
        if (job->bakedIR.getNumSamples() > 0)
        {
            handOff (job->bakedIR, job->bakedIRSampleRate, job->crossfadeMs);
            busy.store (false);
            continue;
        }

        // Refuse pathological geometry (matches the message-thread cap) so a
        // malicious/absurd scene can't peg this thread for minutes.
        if ((int) job->scene.getAllBrushesForTracing().size() > Worldizer::kMaxTraceBrushes)
        {
            juce::Logger::writeToLog ("RenderThread: scene exceeds brush cap, skipping render");
            busy.store (false);
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

        // The trace is done: clear the indicator BEFORE the hand-off, which can
        // wait on the audio thread (a crossfade only advances while the host
        // calls processBlock).
        rendering.store (false);

        handOff (ir, 48000.0, job->crossfadeMs);
        busy.store (false);
    }
}

void RenderThread::handOff (const juce::AudioBuffer<float>& ir, double irSampleRate, float crossfadeMs)
{
    // The engine refuses only while the audio thread is mid-crossfade (a queued
    // but unstarted swap is replaced, never waited on). Retry until it accepts,
    // unless this IR is superseded by a newer job (latest wins: drop it), drain()
    // aborts, or the thread is stopping.
    for (;;)
    {
        if (threadShouldExit() || abortWait.load())
            return;

        if (engine.loadIR (ir, irSampleRate, crossfadeMs) != ConvolutionEngine::LoadResult::busy)
            return;

        {
            const juce::ScopedLock sl (jobLock);
            if (pendingJob != nullptr)
                return;
        }

        juce::Thread::sleep (2);
    }
}
} // namespace Worldizer
