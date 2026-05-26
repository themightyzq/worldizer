#include "PluginEditor.h"
#include "../Shared/Constants.h"

using LnF = WorldizerLookAndFeel;

//==============================================================================
void WorldizerAudioProcessorEditor::populatePresetCombo()
{
    presetSelector.clear (juce::dontSendNotification);
    presetIds.clear();

    const auto all = processorRef.getAvailablePresetMetadata();
    for (int i = 0; i < all.size(); ++i)
    {
        presetSelector.addItem (all.getReference (i).name, i + 1);
        presetIds.add (all.getReference (i).presetId);
    }

    const int cur = presetIds.indexOf (processorRef.getCurrentPresetId());
    presetSelector.setSelectedId (juce::jmax (0, cur) + 1, juce::dontSendNotification);
}

//==============================================================================
WorldizerAudioProcessorEditor::WorldizerAudioProcessorEditor (WorldizerAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    setLookAndFeel (&lookAndFeel);

    // --- Preset selector ---
    populatePresetCombo();
    presetSelector.setTooltip ("Choose the acoustic space (preset) to convolve through.");
    presetSelector.onChange = [this]
    {
        const int idx = presetSelector.getSelectedId() - 1;
        if (idx >= 0 && idx < presetIds.size())
            processorRef.setCurrentPresetId (presetIds[idx]);
    };
    addAndMakeVisible (presetSelector);

    presetLabel.setText ("Preset", juce::dontSendNotification);
    presetLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (presetLabel);

    renderingIndicator.setText ("rendering...", juce::dontSendNotification);
    renderingIndicator.setJustificationType (juce::Justification::centredLeft);
    renderingIndicator.setColour (juce::Label::textColourId, LnF::Colors::accent);
    renderingIndicator.setVisible (false);
    addAndMakeVisible (renderingIndicator);

    // --- Rotary controls ---
    auto setupKnob = [this] (juce::Slider& s, juce::Label& l, const juce::String& name, const juce::String& tip)
    {
        s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 72, 18);
        s.setTooltip (tip);
        addAndMakeVisible (s);

        l.setText (name, juce::dontSendNotification);
        l.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (l);
    };

    setupKnob (inputGainSlider,  inputGainLabel,  "Input Gain",  "Gain applied before the worldizing chain.");
    setupKnob (mixSlider,        mixLabel,        "Mix",         "Blend between dry input (0%) and worldized output (100%).");
    setupKnob (outputGainSlider, outputGainLabel, "Output Gain", "Gain applied after the worldizing chain.");

    // --- Bypass ---
    bypassButton.setClickingTogglesState (true);
    bypassButton.setTooltip ("Pass audio through unchanged.");
    addAndMakeVisible (bypassButton);

    // --- Audition test signals ---
    auditionLabel.setText ("Audition", juce::dontSendNotification);
    auditionLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (auditionLabel);

    const auto testNames = WorldizerAudioProcessor::getTestSignalNames();
    for (int i = 0; i < (int) testButtons.size(); ++i)
    {
        auto& btn = testButtons[(size_t) i];
        btn.setButtonText (i < testNames.size() ? testNames[i] : juce::String (i + 1));
        btn.setTooltip ("Play a built-in " + btn.getButtonText().toLowerCase()
                        + " test signal through the current scene (no host content needed).");
        btn.onClick = [this, i] { processorRef.triggerTestSignal (i); };
        addAndMakeVisible (btn);
    }

    // --- Parameter attachments ---
    inputGainAttach  = std::make_unique<juce::SliderParameterAttachment> (*processorRef.apvts.getParameter ("inputGain"),  inputGainSlider);
    mixAttach        = std::make_unique<juce::SliderParameterAttachment> (*processorRef.apvts.getParameter ("mix"),        mixSlider);
    outputGainAttach = std::make_unique<juce::SliderParameterAttachment> (*processorRef.apvts.getParameter ("outputGain"), outputGainSlider);
    bypassAttach     = std::make_unique<juce::ButtonParameterAttachment> (*processorRef.apvts.getParameter ("bypass"),     bypassButton);

    setSize (600, 320);
    setResizable (false, false);

    startTimerHz (10);
}

WorldizerAudioProcessorEditor::~WorldizerAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

//==============================================================================
void WorldizerAudioProcessorEditor::timerCallback()
{
    const bool r = processorRef.isRendering();
    if (renderingIndicator.isVisible() != r)
        renderingIndicator.setVisible (r);

    // Keep the combo in sync if the preset changed outside the UI (e.g. state restore).
    const int idx = presetIds.indexOf (processorRef.getCurrentPresetId());
    if (idx >= 0 && presetSelector.getSelectedId() != idx + 1)
        presetSelector.setSelectedId (idx + 1, juce::dontSendNotification);
}

//==============================================================================
void WorldizerAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (LnF::Colors::background);

    // Header accent line
    g.setColour (LnF::Colors::accent.withAlpha (0.5f));
    g.fillRect (12, 6, getWidth() - 24, 2);

    // Title + version
    g.setColour (LnF::Colors::onSurface);
    g.setFont (juce::Font (juce::FontOptions (20.0f).withStyle ("Bold")));
    g.drawText ("WORLDIZER", 12, 16, 320, 24, juce::Justification::centredLeft, false);

    g.setColour (LnF::Colors::outline);
    g.setFont (juce::Font (juce::FontOptions (11.0f)));
    g.drawText ("v" + juce::String (Worldizer::kVersionString), 12, 40, 120, 14, juce::Justification::centredLeft, false);

    // Section dividers
    g.setColour (LnF::Colors::outline.withAlpha (0.3f));
    g.drawHorizontalLine (60,  12.0f, (float) getWidth() - 12.0f);
    g.drawHorizontalLine (158, 12.0f, (float) getWidth() - 12.0f);

    // Footer
    g.setColour (LnF::Colors::outline);
    g.setFont (juce::Font (juce::FontOptions (10.0f)));
    g.drawText ("ZQSFX  |  github.com/zqsfx/worldizer",
                getLocalBounds().removeFromBottom (28).reduced (12, 6),
                juce::Justification::centredRight, false);
}

void WorldizerAudioProcessorEditor::resized()
{
    auto area = getLocalBounds();

    auto header = area.removeFromTop (60);
    bypassButton.setBounds (header.getRight() - 90, 17, 78, 26);

    auto presetRow = area.removeFromTop (48).reduced (12, 9);
    presetLabel.setBounds (presetRow.removeFromLeft (50));
    presetSelector.setBounds (presetRow.removeFromLeft (240));
    presetRow.removeFromLeft (12);
    renderingIndicator.setBounds (presetRow);

    auto auditionRow = area.removeFromTop (50).reduced (12, 9);
    auditionLabel.setBounds (auditionRow.removeFromLeft (64));
    auditionRow.removeFromLeft (6);
    const int gap = 8;
    const int btnW = (auditionRow.getWidth() - 2 * gap) / 3;
    for (int i = 0; i < (int) testButtons.size(); ++i)
    {
        testButtons[(size_t) i].setBounds (auditionRow.removeFromLeft (btnW));
        if (i < 2) auditionRow.removeFromLeft (gap);
    }

    area.removeFromBottom (30); // footer

    auto controls = area.reduced (12, 8);
    const int colW = controls.getWidth() / 3;

    auto place = [] (juce::Slider& s, juce::Label& l, juce::Rectangle<int> c)
    {
        s.setBounds (c.removeFromTop (96));
        l.setBounds (c.removeFromTop (18));
    };

    place (inputGainSlider,  inputGainLabel,  controls.removeFromLeft (colW));
    place (mixSlider,        mixLabel,        controls.removeFromLeft (colW));
    place (outputGainSlider, outputGainLabel, controls);
}
