#pragma once

#include <JuceHeader.h>

/**
    Worldizer — main audio processor.

    Slice 0 (scaffold): this is a stereo pass-through with a single `bypass`
    parameter and complete APVTS state save/restore. No DSP is performed yet.
    The convolution engine, character chains, ambient bed, and the background
    ray-tracing thread are introduced in later slices (see TODO.md).
*/
class WorldizerAudioProcessor : public juce::AudioProcessor
{
public:
    WorldizerAudioProcessor();
    ~WorldizerAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override                        { return true; }

    //==============================================================================
    const juce::String getName() const override            { return JucePlugin_Name; }
    bool acceptsMidi() const override                      { return false; }
    bool producesMidi() const override                     { return false; }
    bool isMidiEffect() const override                     { return false; }
    double getTailLengthSeconds() const override           { return 0.0; }

    //==============================================================================
    int getNumPrograms() override                          { return 1; }
    int getCurrentProgram() override                       { return 0; }
    void setCurrentProgram (int) override                  {}
    const juce::String getProgramName (int) override       { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    // Exposes the bypass parameter to the host so native bypass works correctly.
    juce::AudioProcessorParameter* getBypassParameter() const override { return bypassParam; }

    // Public so the editor (and future components) can attach to parameters.
    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    juce::AudioParameterBool* bypassParam = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WorldizerAudioProcessor)
};
