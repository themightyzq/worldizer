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
    if (ir.getNumSamples() <= 0)
    {
        setNone();
        return;
    }
    requestIR (std::move (ir), irSampleRate);
}

void SourceCharacter::setNone()
{
    requestIR (makeUnitImpulse(), sampleRate);
}

void SourceCharacter::requestIR (juce::AudioBuffer<float>&& ir, double irSampleRate)
{
    pending = PendingIR { std::move (ir), irSampleRate };
    flushPendingIfIdle();
    if (pending.has_value())
        startTimer (15); // engine is mid-crossfade; poll until it is safe to load
}

void SourceCharacter::flushPendingIfIdle()
{
    if (! prepared || ! pending.has_value() || engine.isIRPending())
        return;
    engine.loadIR (pending->buffer, pending->sampleRate, kCharacterCrossfadeMs);
    pending.reset();
}

void SourceCharacter::timerCallback()
{
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
