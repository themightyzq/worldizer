#include "SourceCharacter.h"

namespace Worldizer
{
namespace
{
    // A 1-sample unit impulse = identity convolution (the "none" character).
    juce::AudioBuffer<float> makeUnitImpulse()
    {
        juce::AudioBuffer<float> delta (1, 1);
        delta.setSample (0, 0, 1.0f);
        return delta;
    }

    constexpr float kCharacterCrossfadeMs = 60.0f;
}

void SourceCharacter::prepare (const juce::dsp::ProcessSpec& spec)
{
    // prepareToPlay can run on a host thread while this object's timer fires on
    // the message thread (pluginval does exactly that). stopTimer() does not wait
    // for a callback already in flight, so the lock is what keeps a flush from
    // landing while the engine re-prepares.
    const juce::ScopedLock sl (pendingLock);
    stopTimer();
    pending.reset();

    sampleRate = spec.sampleRate;
    engine.prepare (spec.sampleRate, (int) spec.maximumBlockSize, (int) spec.numChannels);

    driveSmoothed.reset (spec.sampleRate, 0.05);
    driveSmoothed.setCurrentAndTargetValue (driveTarget.load());
    prepared = true;

    setNone(); // identity until a character is chosen (caller re-loads from params)
}

void SourceCharacter::reset()
{
    engine.reset();
}

void SourceCharacter::setIR (juce::AudioBuffer<float>&& ir, double irSampleRate)
{
    const juce::ScopedLock sl (pendingLock);
    if (ir.getNumSamples() <= 0)
    {
        setNone();
        return;
    }
    requestIR (std::move (ir), irSampleRate);
}

void SourceCharacter::setNone()
{
    const juce::ScopedLock sl (pendingLock);
    requestIR (makeUnitImpulse(), sampleRate);
}

void SourceCharacter::requestIR (juce::AudioBuffer<float>&& ir, double irSampleRate)
{
    // Caller holds pendingLock.
    pending = PendingIR { std::move (ir), irSampleRate };
    flushPendingIfIdle();
    if (pending.has_value())
        startTimer (15); // engine is mid-crossfade; poll until it is safe to load
}

void SourceCharacter::flushPendingIfIdle()
{
    // Caller holds pendingLock.
    if (! prepared || ! pending.has_value())
        return;
    // normalise=false: character IRs are unit-energy at bake (the "none" delta's
    // energy is exactly 1), so they pass at unity loudness — see ConvolutionEngine.
    // busy = a crossfade is running on the audio thread: keep the IR pending and
    // let the timer retry. Anything else consumes it.
    if (engine.loadIR (pending->buffer, pending->sampleRate, kCharacterCrossfadeMs, false)
            != ConvolutionEngine::LoadResult::busy)
        pending.reset();
}

void SourceCharacter::timerCallback()
{
    const juce::ScopedLock sl (pendingLock);
    flushPendingIfIdle();
    if (! pending.has_value())
        stopTimer();
}

void SourceCharacter::setDrive (float drive)
{
    driveTarget.store (juce::jlimit (0.0f, 1.0f, drive));
}

void SourceCharacter::process (juce::dsp::AudioBlock<float> block)
{
    if (! prepared)
        return;

    // 1. Light nonlinearity (speaker cone/amp softness), BEFORE the speaker IR —
    //    the driver distorts, then the cabinet filters. Dry/saturated blend with a
    //    unity small-signal slope: bit-exact passthrough at drive 0, gently
    //    compressed peaks at drive 1. Stateless => cannot pump.
    driveSmoothed.setTargetValue (driveTarget.load());
    const auto numSamples  = (int) block.getNumSamples();
    const auto numChannels = (int) block.getNumChannels();

    if (driveSmoothed.getCurrentValue() > 0.0f || driveSmoothed.isSmoothing())
    {
        for (int i = 0; i < numSamples; ++i)
        {
            const float d = driveSmoothed.getNextValue();
            if (d <= 0.0f)
                continue;
            for (int ch = 0; ch < numChannels; ++ch)
            {
                auto* s = block.getChannelPointer ((size_t) ch) + i;
                *s = (1.0f - d) * *s + d * (std::tanh (3.0f * *s) * (1.0f / 3.0f));
            }
        }
    }
    else
    {
        driveSmoothed.skip (numSamples);
    }

    // 2. Speaker IR (double-convolver crossfade engine).
    engine.process (block);
}
} // namespace Worldizer
