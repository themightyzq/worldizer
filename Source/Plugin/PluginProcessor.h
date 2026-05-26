#pragma once

#include <JuceHeader.h>
#include <array>
#include <optional>
#include "../DSP/ConvolutionEngine.h"
#include "RenderThread.h"
#include "PresetManager.h"
#include "../Shared/Constants.h"

/**
    Worldizer — main audio processor.

    Slice 2: a real-time convolution reverb. The input is convolved with the IR of
    the selected test scene (rendered on a background thread by the ray tracer and
    crossfaded in by the ConvolutionEngine), then blended with a latency-matched dry
    path and trimmed by input/output gain. A pre-baked default IR is embedded so the
    first audio is available immediately on cold start.
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
    double getTailLengthSeconds() const override           { return (double) Worldizer::kMaxIRLengthSeconds; }

    //==============================================================================
    int getNumPrograms() override                          { return 1; }
    int getCurrentProgram() override                       { return 0; }
    void setCurrentProgram (int) override                  {}
    const juce::String getProgramName (int) override       { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorParameter* getBypassParameter() const override { return bypassParam; }

    //==============================================================================
    // === Preset selection (presets are .wzpreset bundles loaded by PresetManager) ===
    juce::String getCurrentPresetId() const;
    void         setCurrentPresetId (const juce::String& presetId);
    juce::StringArray getAvailablePresetIds() const;
    juce::Array<Worldizer::WzPresetIO::Loaded> getAvailablePresetMetadata() const;
    std::optional<Worldizer::WzPresetIO::Loaded> getCurrentPresetMetadata() const;
    bool isRendering() const noexcept;

    // === Deprecated Slice 2 scene API (mapped onto presets for compatibility) ===
    [[deprecated ("Use getCurrentPresetId")]] juce::String getCurrentSceneName() const;
    [[deprecated ("Use setCurrentPresetId")]] void setCurrentSceneName (const juce::String& sceneName);

    // === Built-in audition test signals ===
    /** Triggers a built-in dry test signal to play once through the worldizing
        chain (so a scene can be auditioned without host content). Index matches
        getTestSignalNames(). RT-safe to call from the message thread. */
    void triggerTestSignal (int index);
    static juce::StringArray getTestSignalNames();

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void requestSceneRender (const juce::String& sceneName, float crossfadeMs);
    void loadEmbeddedDefaultIR();
    void updateDryDelayToMatchConvolutionLatency();
    static juce::String sceneNameToPresetId (const juce::String& sceneName);
    static juce::String presetIdToSceneName (const juce::String& presetId);

    static constexpr const char* kDefaultPreset = "small_concrete_room";

    Worldizer::ConvolutionEngine convolution;
    std::unique_ptr<Worldizer::PresetManager> presetManager;
    std::unique_ptr<Worldizer::RenderThread> renderThread;

    juce::dsp::Gain<float> inputGain, outputGain;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mixSmoothed;

    std::atomic<float>* bypassValue     = nullptr;
    std::atomic<float>* inputGainValue  = nullptr;
    std::atomic<float>* outputGainValue = nullptr;
    std::atomic<float>* mixValue        = nullptr;
    juce::AudioParameterBool* bypassParam = nullptr;

    mutable juce::CriticalSection presetLock;
    juce::String currentPresetId { kDefaultPreset };
    std::optional<Worldizer::WzPresetIO::Loaded> currentPresetMetadata;

    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::None> dryDelay { 8192 };
    int currentDryDelaySamples = 0;

    juce::AudioBuffer<float> dryScratch;
    std::vector<float>       mixRamp;

    // Built-in audition test signals (generated in prepareToPlay).
    void generateTestSignals (double sampleRate);
    static constexpr int kNumTestSignals = 3;
    std::array<juce::AudioBuffer<float>, (size_t) kNumTestSignals> testSignals;
    std::atomic<int> testSignalRequested { -1 };
    int activeTestSignal = -1;  // audio-thread only
    int testSignalPos    = 0;   // audio-thread only

    std::atomic<bool> prepared { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WorldizerAudioProcessor)
};
