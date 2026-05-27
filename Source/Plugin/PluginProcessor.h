#pragma once

#include <JuceHeader.h>
#include <array>
#include <optional>
#include "../DSP/ConvolutionEngine.h"
#include "../DSP/DistanceModel.h"
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
    Worldizer::PresetManager& getPresetManager() noexcept { return *presetManager; }
    std::optional<Worldizer::WzPresetIO::Loaded> getCurrentPresetMetadata() const;
    bool isRendering() const noexcept;

    // === Live source/mic drag (Slice 4) ===
    /** Updates the current scene's source/mic positions and re-renders the IR
        (preview quality during drag, full on release). Ephemeral — not written
        back to the preset on disk. */
    void setSourceAndMicPositions (Worldizer::Vec3 sourcePos, Worldizer::Vec3 micPos, bool fullQuality);

    // === Sidebar UI state (persisted in plugin state) ===
    bool getSidebarCollapsed() const noexcept { return sidebarCollapsed.load(); }
    void setSidebarCollapsed (bool shouldBeCollapsed) { sidebarCollapsed.store (shouldBeCollapsed); }

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
    void renderCurrentScene (bool fullQuality, float crossfadeMs);
    void loadEmbeddedDefaultIR();
    void updateDryDelayToMatchConvolutionLatency();
    static juce::String sceneNameToPresetId (const juce::String& sceneName);

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
    std::atomic<bool> sidebarCollapsed { false };

    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::None> dryDelay { 8192 };
    int currentDryDelaySamples = 0;

    // === Distance model (Slice 4.5) — wet-path only ===
    // The room IR is distance-independent; these add the time-of-flight pre-delay
    // and the inverse-distance level to the WET signal, driven by source/mic spacing.
    Worldizer::DistanceModel distanceModel;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Lagrange3rd> wetPreDelay { 1 << 16 };
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>         preDelaySmoothed;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> attenuationSmoothed;
    std::atomic<float> currentDistanceMeters { 1.0f };

    juce::AudioBuffer<float> dryScratch;
    std::vector<float>       mixRamp;
    std::vector<float>       attenRamp;

    // Built-in audition test signals (generated in prepareToPlay).
    void generateTestSignals (double sampleRate);
    static constexpr int kNumTestSignals = 4;
    std::array<juce::AudioBuffer<float>, (size_t) kNumTestSignals> testSignals;
    std::atomic<int> testSignalRequested { -1 };
    int activeTestSignal = -1;  // audio-thread only
    int testSignalPos    = 0;   // audio-thread only

    std::atomic<bool> prepared { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WorldizerAudioProcessor)
};
