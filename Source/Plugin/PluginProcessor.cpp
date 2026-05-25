#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "../Shared/Constants.h"

//==============================================================================
WorldizerAudioProcessor::WorldizerAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    bypassParam = dynamic_cast<juce::AudioParameterBool*> (apvts.getParameter ("bypass"));
    jassert (bypassParam != nullptr);
}

WorldizerAudioProcessor::~WorldizerAudioProcessor() = default;

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout WorldizerAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "bypass", 1 }, "Bypass", false));

    return layout;
}

//==============================================================================
void WorldizerAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Slice 0: no DSP to prepare. Convolution engine / character chains arrive later.
    juce::ignoreUnused (sampleRate, samplesPerBlock);

    // Honest latency reporting: no added latency in the scaffold.
    setLatencySamples (0);
}

void WorldizerAudioProcessor::releaseResources()
{
    // Slice 0: nothing allocated to release.
}

bool WorldizerAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // Soundminer-compatible: stereo in, stereo out, input must match output.
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;

    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void WorldizerAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                            juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused (midiMessages);

    const auto totalNumInputChannels  = getTotalNumInputChannels();
    const auto totalNumOutputChannels = getTotalNumOutputChannels();

    // Clear any output channels that don't have a corresponding input.
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    // Slice 0: pass-through. With no DSP yet, bypassed and active are identical.
    // Host-level bypass is handled via getBypassParameter(); future slices will
    // branch on bypassParam->get() to skip the convolution / character chains.
}

//==============================================================================
juce::AudioProcessorEditor* WorldizerAudioProcessor::createEditor()
{
    return new WorldizerAudioProcessorEditor (*this);
}

//==============================================================================
void WorldizerAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // Persist the full APVTS tree. Future slices add the current preset ID,
    // source/mic positions, and a reference to the cached IR (never the IR itself).
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void WorldizerAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

//==============================================================================
// This creates new instances of the plugin.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new WorldizerAudioProcessor();
}
