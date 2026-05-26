#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "../Shared/WorldizerLookAndFeel.h"

/**
    Worldizer — minimal Slice 2 editor: scene selector, input/mix/output controls,
    bypass, and a "rendering..." indicator. Fixed size; the full layout (RoomView2D
    etc.) arrives in Slice 4.
*/
class WorldizerAudioProcessorEditor : public juce::AudioProcessorEditor,
                                      private juce::Timer
{
public:
    explicit WorldizerAudioProcessorEditor (WorldizerAudioProcessor&);
    ~WorldizerAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    static juce::String displayName (const juce::String& internalSceneName);

    WorldizerAudioProcessor& processorRef;
    WorldizerLookAndFeel lookAndFeel;

    juce::StringArray sceneNames;

    juce::ComboBox  sceneSelector;
    juce::Label     sceneLabel, renderingIndicator;

    juce::Slider    inputGainSlider, mixSlider, outputGainSlider;
    juce::Label     inputGainLabel, mixLabel, outputGainLabel;

    juce::TextButton bypassButton { "Bypass" };

    juce::Label auditionLabel;
    std::array<juce::TextButton, 3> testButtons;

    std::unique_ptr<juce::SliderParameterAttachment> inputGainAttach, mixAttach, outputGainAttach;
    std::unique_ptr<juce::ButtonParameterAttachment> bypassAttach;

    juce::TooltipWindow tooltipWindow { this, 500 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WorldizerAudioProcessorEditor)
};
