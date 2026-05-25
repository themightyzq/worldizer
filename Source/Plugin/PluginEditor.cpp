#include "PluginEditor.h"
#include "../Shared/Constants.h"

//==============================================================================
WorldizerAudioProcessorEditor::WorldizerAudioProcessorEditor (WorldizerAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    setSize (Worldizer::kDefaultWindowWidth, Worldizer::kDefaultWindowHeight);
    setResizable (true, true);
    setResizeLimits (Worldizer::kMinWindowWidth, Worldizer::kMinWindowHeight,
                     Worldizer::kMaxWindowWidth, Worldizer::kMaxWindowHeight);

    // processorRef is held for Slice 4 (parameter attachments, scene access).
    // Intentionally unused in the scaffold.
    juce::ignoreUnused (processorRef);
}

WorldizerAudioProcessorEditor::~WorldizerAudioProcessorEditor() = default;

//==============================================================================
void WorldizerAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1a1a1d));

    g.setColour (juce::Colour (0xffe0c068)); // placeholder warm-amber accent (final palette TBD)
    g.setFont (juce::Font (juce::FontOptions (22.0f).withStyle ("Bold")));

    const auto text = juce::String (Worldizer::kProductName)
                        + " v" + Worldizer::kVersionString
                        + juce::String::fromUTF8 (" \xe2\x80\x94 scaffold");

    g.drawText (text, getLocalBounds(), juce::Justification::centred, false);
}

void WorldizerAudioProcessorEditor::resized()
{
    // Slice 0: no child components to lay out yet.
}
