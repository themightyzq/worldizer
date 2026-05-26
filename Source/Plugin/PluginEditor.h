#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "../Shared/WorldizerLookAndFeel.h"
#include "../UI/RoomView2D.h"
#include "../UI/PresetBrowser.h"

/**
    Worldizer — Slice 4 editor. Top-down RoomView2D centrepiece, collapsible preset
    browser sidebar, polished control row, header, and footer. 900x650 default,
    resizable 700x550..1400x1000.
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
    void onPresetSelected (const juce::String& presetId);
    void updateSubtitle();

    WorldizerAudioProcessor& processorRef;
    WorldizerLookAndFeel lookAndFeel;

    // Header
    juce::TextButton editButton { "Edit" };
    juce::TextButton bypassButton { "Bypass" };
    juce::String subtitleText;

    // Sidebar + main view
    Worldizer::PresetBrowser presetBrowser;
    Worldizer::RoomView2D    roomView;

    // Control row
    juce::Slider inputGainSlider, mixSlider, outputGainSlider;
    juce::Label  inputGainLabel, mixLabel, outputGainLabel;
    juce::TextButton clickButton { "Click" }, sweepButton { "Sweep" }, noiseButton { "Noise" };
    juce::Label  renderingIndicator;

    // Footer
    juce::HyperlinkButton githubLink;

    juce::Rectangle<int> controlRowBounds;
    bool positionsModified = false;

    std::unique_ptr<juce::SliderParameterAttachment> inputGainAttach, mixAttach, outputGainAttach;
    std::unique_ptr<juce::ButtonParameterAttachment> bypassAttach;

    juce::TooltipWindow tooltipWindow { this, 500 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WorldizerAudioProcessorEditor)
};
