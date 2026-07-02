#include "ConvolutionEngine.h"
#include <cmath>

namespace Worldizer
{
ConvolutionEngine::ConvolutionEngine()
    : convolverA (std::make_unique<juce::dsp::Convolution>()),
      convolverB (std::make_unique<juce::dsp::Convolution>())
{
}

ConvolutionEngine::~ConvolutionEngine() = default;

void ConvolutionEngine::prepare (double sr, int maximumBlockSize, int channels)
{
    sampleRate   = sr;
    maxBlockSize = maximumBlockSize;
    numChannels  = juce::jmax (1, channels);

    const juce::dsp::ProcessSpec spec { sr, (juce::uint32) maximumBlockSize, (juce::uint32) numChannels };
    convolverA->prepare (spec);
    convolverB->prepare (spec);

    // Generous scratch so an oversized host block never reallocates on the audio thread.
    const int scratchLen = juce::jmax (maximumBlockSize, 8192);
    scratchA.setSize (numChannels, scratchLen, false, false, true);
    scratchB.setSize (numChannels, scratchLen, false, false, true);

    crossfadeActive = false;
    crossfadeRemaining = 0;
    crossfadeInProgress.store (false);
    swapRequested.store (false);
    prepared = true;
}

void ConvolutionEngine::reset()
{
    if (convolverA != nullptr) convolverA->reset();
    if (convolverB != nullptr) convolverB->reset();
    crossfadeActive = false;
    crossfadeRemaining = 0;
    crossfadeInProgress.store (false);
    swapRequested.store (false);
}

void ConvolutionEngine::loadIR (const juce::AudioBuffer<float>& ir, double irSampleRate,
                                float crossfadeMs, bool normalise)
{
    if (! prepared)
        return;

    // Deep copy (this runs on the background thread, so allocation is fine), then
    // load into the idle convolver.
    //
    // Mono IR (1 channel, Stereo::no): the same response is applied independently to
    // L and R (pseudo-stereo) — single mic / shipped omni presets. Stereo IR (>= 2
    // channels, Stereo::yes): channel 0 -> L, channel 1 -> R — true stereo from a
    // mic array (XY / spaced pair). juce::dsp::Convolution reconfigures the loaded IR
    // either way; both convolverA/B were prepared with the output channel count, so
    // switching mono<->stereo across a crossfade just works.
    //
    // Trim::yes / Normalise::yes are deliberate (revisited in Slice 4.5): the IR is a
    // distance-INDEPENDENT room response with the direct near sample 0, so Trim strips
    // no meaningful lead (latency stays ~0, dry/wet aligned) and Normalise keeps the
    // wet level consistent. Distance cues are applied separately on the wet path by
    // DistanceModel, so they are NOT discarded by these flags. (For a stereo IR the
    // per-channel inter-mic level/time differences ARE in the IR and survive: Trim
    // crops a common lead from both channels, Normalise scales both by one factor.)
    const auto stereoFlag = ir.getNumChannels() >= 2
                                ? juce::dsp::Convolution::Stereo::yes
                                : juce::dsp::Convolution::Stereo::no;

    juce::AudioBuffer<float> copy (ir);
    convolverB->reset();
    convolverB->loadImpulseResponse (std::move (copy), irSampleRate,
                                     stereoFlag,
                                     juce::dsp::Convolution::Trim::yes,
                                     normalise ? juce::dsp::Convolution::Normalise::yes
                                               : juce::dsp::Convolution::Normalise::no);

    const int xf = juce::jmax (1, (int) std::round (crossfadeMs * 0.001 * sampleRate));
    crossfadeSamples.store (xf);
    swapRequested.store (true);
}

bool ConvolutionEngine::isIRPending() const noexcept
{
    return swapRequested.load() || crossfadeInProgress.load();
}

void ConvolutionEngine::process (juce::dsp::AudioBlock<float> block)
{
    if (! prepared)
        return;

    if (swapRequested.exchange (false))
    {
        crossfadeActive = true;
        crossfadeInProgress.store (true);
        crossfadeTotal = juce::jmax (1, crossfadeSamples.load());
        crossfadeRemaining = crossfadeTotal;
    }

    const int numSamples = (int) block.getNumSamples();
    const int chs        = (int) block.getNumChannels();

    // Contract: hosts never exceed the prepared maximum block size. If one does
    // anyway, degrade gracefully for this block (process A only, no crossfade
    // advance) rather than read/write past the fixed crossfade scratch — the
    // audio thread must not reallocate.
    jassert (numSamples <= scratchA.getNumSamples() && chs <= scratchA.getNumChannels());
    if (! crossfadeActive
        || numSamples > scratchA.getNumSamples() || chs > scratchA.getNumChannels())
    {
        convolverA->process (juce::dsp::ProcessContextReplacing<float> (block));
        return;
    }

    // Crossfade: run both convolvers on copies of the input, then mix with a ramp.
    auto aUse = juce::dsp::AudioBlock<float> (scratchA).getSubsetChannelBlock (0, (size_t) chs)
                    .getSubBlock (0, (size_t) numSamples);
    auto bUse = juce::dsp::AudioBlock<float> (scratchB).getSubsetChannelBlock (0, (size_t) chs)
                    .getSubBlock (0, (size_t) numSamples);
    aUse.copyFrom (block);
    bUse.copyFrom (block);

    convolverA->process (juce::dsp::ProcessContextReplacing<float> (aUse));
    convolverB->process (juce::dsp::ProcessContextReplacing<float> (bUse));

    for (int ch = 0; ch < chs; ++ch)
    {
        auto* out      = block.getChannelPointer ((size_t) ch);
        const auto* a  = aUse.getChannelPointer ((size_t) ch);
        const auto* b  = bUse.getChannelPointer ((size_t) ch);

        int rem = crossfadeRemaining;
        for (int i = 0; i < numSamples; ++i)
        {
            const float t = 1.0f - (float) rem / (float) crossfadeTotal; // 0 (all A) -> 1 (all B)
            out[i] = a[i] * (1.0f - t) + b[i] * t;
            if (rem > 0) --rem;
        }
    }

    crossfadeRemaining -= numSamples;
    if (crossfadeRemaining <= 0)
    {
        crossfadeActive = false;
        std::swap (convolverA, convolverB); // A now holds the new IR
        // Clear the loader gate ONLY AFTER the swap: loaders poll isIRPending()
        // and touch convolverB the moment it reads false — clearing first opens
        // a window where a load lands on the pre-swap pointer and the new IR is
        // silently faded back out.
        crossfadeInProgress.store (false);
    }
}

int ConvolutionEngine::getLatencySamples() const noexcept
{
    return convolverA != nullptr ? convolverA->getLatency() : 0;
}
} // namespace Worldizer
