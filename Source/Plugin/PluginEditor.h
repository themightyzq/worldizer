#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

/**
    Worldizer — main editor.

    Slice 0 (scaffold): a placeholder editor that draws a centered label.
    The real UI — RoomView2D, PresetBrowser, the standard controls panel, and
    WorldizerLookAndFeel — arrives in Slice 4 (see TODO.md).
*/
class WorldizerAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit WorldizerAudioProcessorEditor (WorldizerAudioProcessor&);
    ~WorldizerAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    WorldizerAudioProcessor& processorRef;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WorldizerAudioProcessorEditor)
};
