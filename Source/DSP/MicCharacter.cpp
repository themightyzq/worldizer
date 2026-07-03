#include "MicCharacter.h"

namespace Worldizer
{
namespace
{
    juce::AudioBuffer<float> makeUnitImpulse()
    {
        juce::AudioBuffer<float> delta (1, 1);
        delta.setSample (0, 0, 1.0f);
        return delta;
    }

    constexpr float kCharacterCrossfadeMs = 60.0f;
}

void MicCharacter::prepare (const juce::dsp::ProcessSpec& spec)
{
    stopTimer();      // no pending-IR flush may land while the engine re-prepares
    pending.reset();

    sampleRate = spec.sampleRate;
    engine.prepare (spec.sampleRate, (int) spec.maximumBlockSize, (int) spec.numChannels);

    noiseSmoothed.reset (spec.sampleRate, 0.05);
    noiseSmoothed.setCurrentAndTargetValue (noiseTarget.load());

    // 2 s pink-ish noise loop (Paul Kellet's economy filter over white), unit RMS,
    // seam crossfade-blended at generation so the runtime loop is a plain wrap.
    // The filter coefficients are fixed (not re-derived per rate), so the spectral
    // tilt shifts slightly at 96/192 kHz; RMS-normalisation holds the LEVEL constant
    // and this is a mic self-noise FLOOR, so the tiny timbre drift is inaudible.
    const int n = juce::jmax (1, (int) (spec.sampleRate * 2.0));
    noiseLoop.setSize (1, n);
    auto* d = noiseLoop.getWritePointer (0);
    juce::Random rng (0x0DEC1BE1); // deterministic
    float b0 = 0.0f, b1 = 0.0f, b2 = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        const float white = rng.nextFloat() * 2.0f - 1.0f;
        b0 = 0.99765f * b0 + white * 0.0990460f;
        b1 = 0.96300f * b1 + white * 0.2965164f;
        b2 = 0.57000f * b2 + white * 1.0526913f;
        d[i] = b0 + b1 + b2 + white * 0.1848f;
    }

    // Seam blend (last 50 ms into the first) then trim, exactly as the baked
    // beds do — equal-power, since head and tail are uncorrelated noise (a
    // linear blend would dip -3 dB at the seam midpoint every ~2 s).
    const int xf = juce::jmin (n / 4, (int) (spec.sampleRate * 0.05));
    for (int i = 0; i < xf; ++i)
    {
        const float t = (float) i / (float) juce::jmax (1, xf);
        d[i] = d[i] * std::sqrt (t) + d[n - xf + i] * std::sqrt (1.0f - t);
    }
    noiseLoop.setSize (1, n - xf, true);

    // Normalise to unit RMS so the level mapping in setSelfNoise is honest.
    double sumSq = 0.0;
    for (int i = 0; i < noiseLoop.getNumSamples(); ++i)
        sumSq += (double) noiseLoop.getSample (0, i) * (double) noiseLoop.getSample (0, i);
    const auto rms = (float) std::sqrt (sumSq / (double) juce::jmax (1, noiseLoop.getNumSamples()));
    if (rms > 0.0f)
        noiseLoop.applyGain (1.0f / rms);

    noisePos = 0;
    prepared = true;

    setNone(); // identity until a character is chosen (caller re-loads from params)
}

void MicCharacter::reset()
{
    engine.reset();
    noisePos = 0;
}

void MicCharacter::setIR (juce::AudioBuffer<float>&& ir, double irSampleRate)
{
    if (ir.getNumSamples() <= 0)
    {
        setNone();
        return;
    }
    requestIR (std::move (ir), irSampleRate);
}

void MicCharacter::setNone()
{
    requestIR (makeUnitImpulse(), sampleRate);
}

void MicCharacter::requestIR (juce::AudioBuffer<float>&& ir, double irSampleRate)
{
    pending = PendingIR { std::move (ir), irSampleRate };
    flushPendingIfIdle();
    if (pending.has_value())
        startTimer (15);
}

void MicCharacter::flushPendingIfIdle()
{
    if (! prepared || ! pending.has_value() || engine.isIRPending())
        return;
    // normalise=false: character IRs are unit-energy at bake (the "none" delta's
    // energy is exactly 1), so they pass at unity loudness — see ConvolutionEngine.
    engine.loadIR (pending->buffer, pending->sampleRate, kCharacterCrossfadeMs, false);
    pending.reset();
}

void MicCharacter::timerCallback()
{
    flushPendingIfIdle();
    if (! pending.has_value())
        stopTimer();
}

void MicCharacter::setSelfNoise (float amount)
{
    amount = juce::jlimit (0.0f, 1.0f, amount);
    const float gain = amount <= 0.0f
                           ? 0.0f
                           : juce::Decibels::decibelsToGain (juce::jmap (amount, -70.0f, -35.0f));
    noiseTarget.store (gain);
}

void MicCharacter::process (juce::dsp::AudioBlock<float> block)
{
    if (! prepared)
        return;

    // 1. Mic IR (double-convolver crossfade engine).
    engine.process (block);

    // 2. Self-noise floor (post-IR: the noise is the capsule/electronics, not the
    //    room). Same mono loop into every channel; 0 = bit-exact skip.
    noiseSmoothed.setTargetValue (noiseTarget.load());
    const auto numSamples  = (int) block.getNumSamples();
    const auto numChannels = (int) block.getNumChannels();
    const int loopLen = noiseLoop.getNumSamples();

    if (loopLen > 0 && (noiseSmoothed.getCurrentValue() > 0.0f || noiseSmoothed.isSmoothing()))
    {
        const auto* noise = noiseLoop.getReadPointer (0);
        for (int i = 0; i < numSamples; ++i)
        {
            const float g = noiseSmoothed.getNextValue();
            const float sample = g * noise[noisePos];
            for (int ch = 0; ch < numChannels; ++ch)
                block.getChannelPointer ((size_t) ch)[i] += sample;
            if (++noisePos >= loopLen)
                noisePos = 0;
        }
    }
    else
    {
        noiseSmoothed.skip (numSamples);
    }
}
} // namespace Worldizer
