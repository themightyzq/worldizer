#include "AmbientBed.h"

namespace Worldizer
{
void AmbientBed::prepare (const juce::dsp::ProcessSpec& spec)
{
    prepared.store (false); // audio thread backs off while we reset the pool

    sampleRate = spec.sampleRate;
    levelSmoothed.reset (spec.sampleRate, 0.05);
    levelSmoothed.setCurrentAndTargetValue (levelTarget.load());

    for (auto& slot : slots)
        slot.state.store (SlotState::Free);
    pendingSlot.store (-1);

    activeSlot = fadingSlot = -1;
    activePos = fadingPos = 0;
    fadeRemaining = fadeTotal = 0;

    prepared.store (true);
}

void AmbientBed::reset()
{
    // Playback state only — pool contents stay valid (they are message-thread-owned).
    activePos = fadingPos = 0;
}

int AmbientBed::claimFreeSlot()
{
    for (int i = 0; i < kNumSlots; ++i)
    {
        auto expected = SlotState::Free;
        if (slots[(size_t) i].state.compare_exchange_strong (expected, SlotState::Loading))
            return i;
    }
    return -1; // pool exhausted (needs 4+ pending swaps inside one 250 ms fade)
}

void AmbientBed::publishSlot (int slotIndex)
{
    slots[(size_t) slotIndex].state.store (SlotState::Pending);
    const int previous = pendingSlot.exchange (slotIndex);
    if (previous >= 0) // an unconsumed older pending bed — release it
        slots[(size_t) previous].state.store (SlotState::Free);
}

void AmbientBed::setSample (const juce::AudioBuffer<float>& sample, double sourceSampleRate)
{
    if (sample.getNumSamples() <= 0 || sourceSampleRate <= 0.0)
    {
        clearSample();
        return;
    }

    const int slotIndex = claimFreeSlot();
    if (slotIndex < 0)
    {
        juce::Logger::writeToLog ("AmbientBed: slot pool exhausted, bed change dropped");
        return;
    }

    auto& dst = slots[(size_t) slotIndex].buffer;
    const int numCh = sample.getNumChannels();

    if (std::abs (sourceSampleRate - sampleRate) < 0.5)
    {
        dst.makeCopyOf (sample);
    }
    else
    {
        // Resample to the host rate (message thread; allocation fine here).
        const double ratio  = sourceSampleRate / sampleRate;
        const int    outLen = juce::jmax (1, (int) std::floor ((double) sample.getNumSamples() / ratio));
        dst.setSize (numCh, outLen, false, true);
        for (int ch = 0; ch < numCh; ++ch)
        {
            juce::LagrangeInterpolator interp;
            interp.process (ratio, sample.getReadPointer (ch), dst.getWritePointer (ch), outLen);
        }
    }

    publishSlot (slotIndex);
}

void AmbientBed::clearSample()
{
    const int slotIndex = claimFreeSlot();
    if (slotIndex < 0)
        return;
    slots[(size_t) slotIndex].buffer.setSize (0, 0); // empty buffer == silence
    publishSlot (slotIndex);
}

void AmbientBed::setLevel (float gain)
{
    levelTarget.store (juce::jlimit (0.0f, 1.0f, gain));
}

void AmbientBed::addToBuffer (juce::AudioBuffer<float>& buffer)
{
    if (! prepared.load())
        return;

    // Pick up a pending bed when not already mid-fade (one-deep queue: rapid
    // switches converge to the latest published bed).
    if (fadeRemaining <= 0)
    {
        const int incoming = pendingSlot.exchange (-1);
        if (incoming >= 0)
        {
            if (fadingSlot >= 0) // finished fade bookkeeping (defensive)
                slots[(size_t) fadingSlot].state.store (SlotState::Free);

            fadingSlot = activeSlot;
            fadingPos  = activePos;
            if (fadingSlot >= 0)
                slots[(size_t) fadingSlot].state.store (SlotState::Fading);

            activeSlot = incoming;
            activePos  = 0;
            slots[(size_t) activeSlot].state.store (SlotState::Active);

            fadeTotal = fadeRemaining = juce::jmax (1, (int) (sampleRate * 0.25));
        }
    }

    levelSmoothed.setTargetValue (levelTarget.load());

    const bool activeSilent = activeSlot < 0 || slots[(size_t) activeSlot].buffer.getNumSamples() == 0;
    const bool fadingSilent = fadingSlot < 0 || slots[(size_t) fadingSlot].buffer.getNumSamples() == 0;

    // Fast path: nothing audible and level settled — release any faded-out slot.
    if (activeSilent && fadeRemaining <= 0)
    {
        if (fadingSlot >= 0)
        {
            slots[(size_t) fadingSlot].state.store (SlotState::Free);
            fadingSlot = -1;
        }
        levelSmoothed.skip (buffer.getNumSamples());
        return;
    }

    const int numSamples = buffer.getNumSamples();
    const int numCh      = buffer.getNumChannels();

    const auto* activeData = activeSilent ? nullptr : slots[(size_t) activeSlot].buffer.getReadPointer (0);
    const int   activeLen  = activeSilent ? 0 : slots[(size_t) activeSlot].buffer.getNumSamples();
    const auto* fadingData = fadingSilent ? nullptr : slots[(size_t) fadingSlot].buffer.getReadPointer (0);
    const int   fadingLen  = fadingSilent ? 0 : slots[(size_t) fadingSlot].buffer.getNumSamples();

    for (int i = 0; i < numSamples; ++i)
    {
        float s = 0.0f;

        if (fadeRemaining > 0)
        {
            const float t = 1.0f - (float) fadeRemaining / (float) fadeTotal; // 0 -> 1
            const float a = activeLen > 0 ? activeData[activePos] : 0.0f;
            const float f = fadingLen > 0 ? fadingData[fadingPos] : 0.0f;
            s = a * t + f * (1.0f - t);

            if (--fadeRemaining == 0 && fadingSlot >= 0)
            {
                slots[(size_t) fadingSlot].state.store (SlotState::Free);
                fadingSlot = -1;
            }
        }
        else if (activeLen > 0)
        {
            s = activeData[activePos];
        }

        s *= levelSmoothed.getNextValue();

        for (int ch = 0; ch < numCh; ++ch)
            buffer.getWritePointer (ch)[i] += s;

        if (activeLen > 0 && ++activePos >= activeLen) activePos = 0;
        if (fadingLen > 0 && ++fadingPos >= fadingLen) fadingPos = 0;
    }
}
} // namespace Worldizer
