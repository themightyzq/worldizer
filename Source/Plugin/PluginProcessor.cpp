#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "WorldizerBinaryData.h"
#include <cmath>

using namespace Worldizer;

namespace
{
    // Normalises a mono buffer to a target RMS measured over its ACTIVE region
    // (samples above -26 dB of the peak). This matches perceived loudness across
    // signals of different density — sparse clicks vs. a sustained sweep/noise —
    // far better than peak normalisation would. A peak ceiling prevents clipping.
    void normalizeActiveRms (juce::AudioBuffer<float>& b, float targetRms, float peakCeiling)
    {
        if (b.getNumSamples() <= 0)
            return;

        auto* d = b.getWritePointer (0);
        const int n = b.getNumSamples();

        float peak = 0.0f;
        for (int i = 0; i < n; ++i)
            peak = juce::jmax (peak, std::abs (d[i]));
        if (peak <= 0.0f)
            return;

        const float threshold = peak * 0.05f; // -26 dB of peak counts as "active"
        double sumSq = 0.0;
        int count = 0;
        for (int i = 0; i < n; ++i)
            if (std::abs (d[i]) > threshold) { sumSq += (double) d[i] * (double) d[i]; ++count; }

        if (count == 0)
            return;

        const float rms = (float) std::sqrt (sumSq / (double) count);
        if (rms <= 0.0f)
            return;

        float gain = targetRms / rms;
        if (peak * gain > peakCeiling) // safety: never let a transient clip
            gain = peakCeiling / peak;

        b.applyGain (gain);
    }
}

//==============================================================================
WorldizerAudioProcessor::WorldizerAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    bypassParam = dynamic_cast<juce::AudioParameterBool*> (apvts.getParameter ("bypass"));
    jassert (bypassParam != nullptr);

    renderThread = std::make_unique<RenderThread> (convolution);
}

WorldizerAudioProcessor::~WorldizerAudioProcessor() = default;

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout WorldizerAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "bypass", 1 }, "Bypass", false));

    auto dbString = [] (float v, int) { return juce::String (v, 1) + " dB"; };

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "inputGain", 1 }, "Input Gain",
        juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction (dbString)));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "outputGain", 1 }, "Output Gain",
        juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction (dbString)));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "mix", 1 }, "Mix",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction (
            [] (float v, int) { return juce::String (v, 1) + " %"; })));

    return layout;
}

//==============================================================================
juce::StringArray WorldizerAudioProcessor::getAvailableSceneNames()
{
    return { "smallConcreteRoom", "hallway", "forestClearing", "gymnasium", "anechoic" };
}

juce::String WorldizerAudioProcessor::getCurrentSceneName() const
{
    const juce::ScopedLock sl (sceneNameLock);
    return currentSceneName;
}

void WorldizerAudioProcessor::setCurrentSceneName (const juce::String& sceneName)
{
    {
        const juce::ScopedLock sl (sceneNameLock);
        if (sceneName == currentSceneName)
            return;
        currentSceneName = sceneName;
    }

    if (prepared.load())
        requestSceneRender (sceneName, 80.0f);
}

bool WorldizerAudioProcessor::isRendering() const noexcept
{
    return renderThread != nullptr && renderThread->isRendering();
}

void WorldizerAudioProcessor::requestSceneRender (const juce::String& sceneName, float crossfadeMs)
{
    if (renderThread == nullptr)
        return;

    RenderThread::Job job;
    job.sceneName   = sceneName;
    job.numRays     = 50000;
    job.maxBounces  = 32;
    job.randomSeed  = 12345;
    job.crossfadeMs = crossfadeMs;
    renderThread->requestRender (job);
}

//==============================================================================
juce::StringArray WorldizerAudioProcessor::getTestSignalNames()
{
    return { "Click", "Sweep", "Noise" };
}

void WorldizerAudioProcessor::triggerTestSignal (int index)
{
    if (index >= 0 && index < kNumTestSignals)
        testSignalRequested.store (index);
}

void WorldizerAudioProcessor::generateTestSignals (double sampleRate)
{
    const int sr = (int) sampleRate;
    juce::Random rng (1234);

    // 0: Click — three short decaying-noise transients (reveals reflections).
    {
        const int len = juce::jmax (1, (int) (1.0 * sr));
        auto& b = testSignals[0]; b.setSize (1, len); b.clear();
        auto* d = b.getWritePointer (0);
        auto click = [&] (double at)
        {
            const int start = (int) (at * sr);
            const int n     = (int) (0.004 * sr);
            for (int i = 0; i < n && start + i < len; ++i)
                d[start + i] += 0.8f * std::exp (-(float) i / (0.0015f * (float) sr)) * (rng.nextFloat() * 2.0f - 1.0f);
        };
        click (0.05); click (0.40); click (0.75);
    }

    // 1: Sweep — 3 s exponential 20 Hz -> 20 kHz (reveals frequency response).
    {
        const double T = 3.0, f1 = 20.0, f2 = 20000.0;
        const int len = juce::jmax (1, (int) (T * sr));
        auto& b = testSignals[1]; b.setSize (1, len); b.clear();
        auto* d = b.getWritePointer (0);
        const double w1 = 2.0 * juce::MathConstants<double>::pi * f1;
        const double L  = std::log (f2 / f1);
        const double K  = w1 * T / L;
        for (int i = 0; i < len; ++i)
        {
            const double t = (double) i / sr;
            d[i] = 0.5f * (float) std::sin (K * (std::exp (t / T * L) - 1.0));
        }
        const int fi = (int) (0.02 * sr), fo = (int) (0.03 * sr);
        for (int i = 0; i < fi && i < len; ++i) d[i]           *= (float) i / (float) fi;
        for (int i = 0; i < fo && i < len; ++i) d[len - 1 - i] *= (float) i / (float) fo;
    }

    // 2: Noise — 0.6 s broadband burst, smooth in, hard stop (exposes the tail).
    {
        const int len = juce::jmax (1, (int) (0.6 * sr));
        auto& b = testSignals[2]; b.setSize (1, len); b.clear();
        auto* d = b.getWritePointer (0);
        for (int i = 0; i < len; ++i) d[i] = 0.4f * (rng.nextFloat() * 2.0f - 1.0f);
        const int fi = (int) (0.02 * sr);
        for (int i = 0; i < fi && i < len; ++i) d[i] *= (float) i / (float) fi;
        const int fo = juce::jmax (1, (int) (0.001 * sr));
        for (int i = 0; i < fo && i < len; ++i) d[len - 1 - i] *= (float) i / (float) fo;
    }

    // Match perceived loudness across all three (active-region RMS, peak-capped).
    for (auto& b : testSignals)
        normalizeActiveRms (b, 0.18f, 0.89f); // ~ -15 dBFS RMS, -1 dBFS peak ceiling
}

//==============================================================================
void WorldizerAudioProcessor::loadEmbeddedDefaultIR()
{
    juce::WavAudioFormat wav;
    auto* rawStream = new juce::MemoryInputStream (WorldizerBinaryData::default_ir_wav,
                                                   (size_t) WorldizerBinaryData::default_ir_wavSize,
                                                   false);
    std::unique_ptr<juce::AudioFormatReader> reader (wav.createReaderFor (rawStream, true));
    if (reader != nullptr && reader->lengthInSamples > 0)
    {
        juce::AudioBuffer<float> ir ((int) reader->numChannels, (int) reader->lengthInSamples);
        reader->read (&ir, 0, (int) reader->lengthInSamples, 0, true, true);
        convolution.loadIR (ir, reader->sampleRate, 5.0f); // short crossfade — nothing audible to fade from
    }
}

//==============================================================================
void WorldizerAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const int numCh = getTotalNumOutputChannels();
    const juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) samplesPerBlock, (juce::uint32) numCh };

    convolution.prepare (sampleRate, samplesPerBlock, numCh);

    inputGain.prepare (spec);   inputGain.setRampDurationSeconds (0.02);
    outputGain.prepare (spec);  outputGain.setRampDurationSeconds (0.02);
    mixSmoothed.reset (sampleRate, 0.02);
    mixSmoothed.setCurrentAndTargetValue (apvts.getRawParameterValue ("mix")->load() * 0.01f);

    dryDelay.prepare (spec);
    dryDelay.setMaximumDelayInSamples (8192);
    dryDelay.reset();

    const int scratchLen = juce::jmax (samplesPerBlock, 8192);
    dryScratch.setSize (numCh, scratchLen, false, false, true);
    mixRamp.assign ((size_t) scratchLen, 0.0f);

    generateTestSignals (sampleRate);
    activeTestSignal = -1;
    testSignalRequested.store (-1);

    bypassValue     = apvts.getRawParameterValue ("bypass");
    inputGainValue  = apvts.getRawParameterValue ("inputGain");
    outputGainValue = apvts.getRawParameterValue ("outputGain");
    mixValue        = apvts.getRawParameterValue ("mix");

    // Instant first audio: load the embedded default IR synchronously.
    loadEmbeddedDefaultIR();
    updateDryDelayToMatchConvolutionLatency();

    prepared.store (true);

    // If the desired scene isn't the embedded default, render it in the background.
    juce::String desired;
    {
        const juce::ScopedLock sl (sceneNameLock);
        desired = currentSceneName;
    }
    if (desired != kDefaultScene)
        requestSceneRender (desired, 80.0f);
}

void WorldizerAudioProcessor::releaseResources()
{
    prepared.store (false);
    convolution.reset();
}

bool WorldizerAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;

    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void WorldizerAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    if (! prepared.load())
        return; // not yet prepared — pass audio through untouched

    const int numSamples = buffer.getNumSamples();
    const int numCh      = buffer.getNumChannels();

    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, numSamples);

    if (bypassValue->load() >= 0.5f)
        return; // pass through unchanged

    // Built-in audition: replace the input with a generated test signal while one plays.
    const int req = testSignalRequested.exchange (-1);
    if (req >= 0)
    {
        activeTestSignal = req;
        testSignalPos    = 0;
    }
    if (activeTestSignal >= 0)
    {
        const auto& sig  = testSignals[(size_t) activeTestSignal];
        const int   sigLen = sig.getNumSamples();
        const auto* src  = sig.getReadPointer (0);
        for (int i = 0; i < numSamples; ++i)
        {
            const float s = (testSignalPos < sigLen) ? src[(size_t) testSignalPos] : 0.0f;
            for (int ch = 0; ch < numCh; ++ch)
                buffer.getWritePointer (ch)[i] = s;
            ++testSignalPos;
        }
        if (testSignalPos >= sigLen)
            activeTestSignal = -1;
    }

    inputGain.setGainDecibels (inputGainValue->load());
    outputGain.setGainDecibels (outputGainValue->load());
    mixSmoothed.setTargetValue (mixValue->load() * 0.01f);

    juce::dsp::AudioBlock<float> block (buffer);

    // 1. Input gain (smoothed, all channels).
    inputGain.process (juce::dsp::ProcessContextReplacing<float> (block));

    // 2. Capture the dry signal (post input-gain, pre-convolution).
    for (int ch = 0; ch < numCh; ++ch)
        dryScratch.copyFrom (ch, 0, buffer, ch, 0, numSamples);

    // 3. Wet path: convolution in place.
    convolution.process (block);

    // 4. Dry path: latency-match to the convolver (usually zero -> skip).
    if (currentDryDelaySamples > 0)
    {
        auto dryBlock = juce::dsp::AudioBlock<float> (dryScratch)
                            .getSubsetChannelBlock (0, (size_t) numCh)
                            .getSubBlock (0, (size_t) numSamples);
        dryDelay.process (juce::dsp::ProcessContextReplacing<float> (dryBlock));
    }

    // 5. Mix dry + wet with a smoothed blend.
    if ((int) mixRamp.size() < numSamples)
        mixRamp.resize ((size_t) numSamples);
    for (int i = 0; i < numSamples; ++i)
        mixRamp[(size_t) i] = mixSmoothed.getNextValue();

    for (int ch = 0; ch < numCh; ++ch)
    {
        auto* wet = buffer.getWritePointer (ch);
        const auto* dry = dryScratch.getReadPointer (ch);
        for (int i = 0; i < numSamples; ++i)
        {
            const float m = mixRamp[(size_t) i];
            wet[i] = wet[i] * m + dry[i] * (1.0f - m);
        }
    }

    // 6. Output gain.
    outputGain.process (juce::dsp::ProcessContextReplacing<float> (block));
}

//==============================================================================
void WorldizerAudioProcessor::updateDryDelayToMatchConvolutionLatency()
{
    const int latency = convolution.getLatencySamples();
    if (latency != currentDryDelaySamples)
    {
        currentDryDelaySamples = latency;
        dryDelay.setDelay (static_cast<float> (latency));
        setLatencySamples (latency);
    }
}

//==============================================================================
juce::AudioProcessorEditor* WorldizerAudioProcessor::createEditor()
{
    return new WorldizerAudioProcessorEditor (*this);
}

//==============================================================================
void WorldizerAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    {
        const juce::ScopedLock sl (sceneNameLock);
        state.setProperty ("currentScene", currentSceneName, nullptr);
    }
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void WorldizerAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        auto state = juce::ValueTree::fromXml (*xml);
        if (state.isValid() && state.hasType (apvts.state.getType()))
        {
            apvts.replaceState (state);
            if (state.hasProperty ("currentScene"))
                setCurrentSceneName (state["currentScene"].toString());
        }
    }
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new WorldizerAudioProcessor();
}
