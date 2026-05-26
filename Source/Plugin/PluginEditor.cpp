#include "PluginEditor.h"
#include "../Shared/Constants.h"

namespace Col = Worldizer::Colors;

//==============================================================================
WorldizerAudioProcessorEditor::WorldizerAudioProcessorEditor (WorldizerAudioProcessor& p)
    : AudioProcessorEditor (&p),
      processorRef (p),
      presetBrowser (p.getPresetManager())
{
    setLookAndFeel (&lookAndFeel);

    // --- Header buttons ---
    editButton.setEnabled (false);
    editButton.setTooltip ("Geometry editor coming in a future update.");
    addAndMakeVisible (editButton);

    bypassButton.setClickingTogglesState (true);
    bypassButton.setTooltip ("Pass audio through unchanged.");
    addAndMakeVisible (bypassButton);
    bypassAttach = std::make_unique<juce::ButtonParameterAttachment> (*p.apvts.getParameter ("bypass"), bypassButton);

    // --- Sidebar ---
    presetBrowser.setSelectedPresetId (p.getCurrentPresetId());
    presetBrowser.setCollapsed (p.getSidebarCollapsed());
    presetBrowser.onPresetSelected = [this] (auto id) { onPresetSelected (id); };
    presetBrowser.onCollapseChanged = [this]
    {
        processorRef.setSidebarCollapsed (presetBrowser.isCollapsed());
        resized();
    };
    addAndMakeVisible (presetBrowser);

    // --- Room view ---
    if (auto meta = p.getCurrentPresetMetadata())
        roomView.setScene (meta->scene);
    roomView.onPositionsChanged = [this] (Worldizer::Vec3 s, Worldizer::Vec3 m)
    {
        positionsModified = true;
        processorRef.setSourceAndMicPositions (s, m, false);
        updateSubtitle();
    };
    roomView.onPositionsFinalized = [this] (Worldizer::Vec3 s, Worldizer::Vec3 m)
    {
        processorRef.setSourceAndMicPositions (s, m, true);
    };
    addAndMakeVisible (roomView);

    // --- Knobs ---
    auto setupKnob = [this] (juce::Slider& s, juce::Label& l, const juce::String& name, const juce::String& tip)
    {
        s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 72, 18);
        s.setTooltip (tip);
        addAndMakeVisible (s);
        l.setText (name, juce::dontSendNotification);
        l.setJustificationType (juce::Justification::centred);
        l.setFont (juce::Font (juce::FontOptions (14.0f)));
        addAndMakeVisible (l);
    };
    setupKnob (inputGainSlider,  inputGainLabel,  "Input Gain",  "Gain applied before the worldizing chain.");
    setupKnob (mixSlider,        mixLabel,        "Mix",         "Blend between dry input (0%) and worldized output (100%).");
    setupKnob (outputGainSlider, outputGainLabel, "Output Gain", "Gain applied after the worldizing chain.");
    inputGainAttach  = std::make_unique<juce::SliderParameterAttachment> (*p.apvts.getParameter ("inputGain"),  inputGainSlider);
    mixAttach        = std::make_unique<juce::SliderParameterAttachment> (*p.apvts.getParameter ("mix"),        mixSlider);
    outputGainAttach = std::make_unique<juce::SliderParameterAttachment> (*p.apvts.getParameter ("outputGain"), outputGainSlider);

    // --- Audition buttons ---
    auto setupAudition = [this] (juce::TextButton& b, int index, const juce::String& sig)
    {
        b.setTooltip ("Play a built-in " + sig + " test signal through the current preset.");
        b.onClick = [this, index] { processorRef.triggerTestSignal (index); };
        addAndMakeVisible (b);
    };
    setupAudition (clickButton, 0, "click");
    setupAudition (sweepButton, 1, "sweep");
    setupAudition (noiseButton, 2, "noise");

    // --- Rendering indicator ---
    renderingIndicator.setText ("rendering...", juce::dontSendNotification);
    renderingIndicator.setJustificationType (juce::Justification::centred);
    renderingIndicator.setColour (juce::Label::textColourId, Col::primary);
    renderingIndicator.setVisible (false);
    addAndMakeVisible (renderingIndicator);

    // --- Footer ---
    githubLink.setButtonText ("github.com/zqsfx/worldizer");
    githubLink.setURL (juce::URL ("https://github.com/zqsfx/worldizer"));
    githubLink.setFont (juce::Font (juce::FontOptions (9.0f)), false, juce::Justification::centredRight);
    githubLink.setColour (juce::HyperlinkButton::textColourId, Col::onSurfaceMuted);
    addAndMakeVisible (githubLink);

    updateSubtitle();

    setSize (Worldizer::kDefaultWindowWidth, Worldizer::kDefaultWindowHeight);
    setResizable (true, true);
    setResizeLimits (Worldizer::kMinWindowWidth, Worldizer::kMinWindowHeight,
                     Worldizer::kMaxWindowWidth, Worldizer::kMaxWindowHeight);

    startTimerHz (10);
}

WorldizerAudioProcessorEditor::~WorldizerAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

//==============================================================================
void WorldizerAudioProcessorEditor::onPresetSelected (const juce::String& presetId)
{
    processorRef.setCurrentPresetId (presetId);
    positionsModified = false;
    if (auto meta = processorRef.getCurrentPresetMetadata())
        roomView.setScene (meta->scene);
    updateSubtitle();
}

void WorldizerAudioProcessorEditor::updateSubtitle()
{
    juce::String preset = "—";
    if (auto meta = processorRef.getCurrentPresetMetadata())
        preset = meta->name;
    subtitleText = "v" + juce::String (Worldizer::kVersionString) + "  \xe2\x80\xa2  " + preset
                 + (positionsModified ? "*" : "");
    repaint();
}

void WorldizerAudioProcessorEditor::timerCallback()
{
    const bool r = processorRef.isRendering();
    if (renderingIndicator.isVisible() != r)
        renderingIndicator.setVisible (r);

    // Sync the UI if the preset changed outside the browser (e.g. state restore).
    if (processorRef.getCurrentPresetId() != presetBrowser.getSelectedPresetId())
    {
        presetBrowser.setSelectedPresetId (processorRef.getCurrentPresetId());
        if (auto meta = processorRef.getCurrentPresetMetadata())
            roomView.setScene (meta->scene);
        positionsModified = false;
        updateSubtitle();
    }
}

//==============================================================================
void WorldizerAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (Col::background);

    // Accent line
    g.setColour (Col::primaryAlpha40);
    g.fillRect (0, 0, getWidth(), 6);

    // Title + subtitle
    g.setColour (Col::primary);
    g.setFont (juce::Font (juce::FontOptions (18.0f).withStyle ("Bold")));
    g.drawText ("WORLDIZER", 12, 14, 320, 24, juce::Justification::centredLeft);

    g.setColour (Col::onSurfaceVariant);
    g.setFont (juce::Font (juce::FontOptions (11.0f)));
    g.drawText (subtitleText, 12, 40, getWidth() - 200, 16, juce::Justification::centredLeft);

    // Control-row cluster dividers
    if (! controlRowBounds.isEmpty())
    {
        g.setColour (Col::outline);
        const int x1 = controlRowBounds.getX() + (int) (controlRowBounds.getWidth() * 0.48f);
        const int x2 = controlRowBounds.getX() + (int) (controlRowBounds.getWidth() * 0.78f);
        g.drawVerticalLine (x1, (float) controlRowBounds.getY() + 8, (float) controlRowBounds.getBottom() - 8);
        g.drawVerticalLine (x2, (float) controlRowBounds.getY() + 8, (float) controlRowBounds.getBottom() - 8);
        g.drawHorizontalLine (controlRowBounds.getY(), 0.0f, (float) getWidth());
    }
}

void WorldizerAudioProcessorEditor::resized()
{
    auto area = getLocalBounds();
    area.removeFromTop (6); // accent line

    auto header = area.removeFromTop (60);
    bypassButton.setBounds (header.getRight() - 12 - 80, header.getY() + 16, 80, 28);
    editButton.setBounds   (bypassButton.getX() - 8 - 80, header.getY() + 16, 80, 28);

    auto footer = area.removeFromBottom (24);
    githubLink.setBounds (footer.removeFromRight (260).reduced (8, 4));

    controlRowBounds = area.removeFromBottom (130);
    {
        auto cr = controlRowBounds.reduced (12, 10);
        auto knobArea     = cr.removeFromLeft ((int) (controlRowBounds.getWidth() * 0.48f));
        auto auditionArea = cr.removeFromLeft ((int) (controlRowBounds.getWidth() * 0.30f));
        auto indicatorArea = cr;

        auto placeKnob = [] (juce::Slider& s, juce::Label& l, juce::Rectangle<int> colm)
        {
            const int kd = 78;
            const int kx = colm.getCentreX() - kd / 2;
            s.setBounds (kx, colm.getY(), kd, kd + 18);
            l.setBounds (colm.getX(), colm.getY() + kd + 18, colm.getWidth(), 16);
        };
        const int colW = knobArea.getWidth() / 3;
        placeKnob (inputGainSlider,  inputGainLabel,  knobArea.removeFromLeft (colW));
        placeKnob (mixSlider,        mixLabel,        knobArea.removeFromLeft (colW));
        placeKnob (outputGainSlider, outputGainLabel, knobArea);

        auto ab = auditionArea.withSizeKeepingCentre (auditionArea.getWidth() - 12, 32);
        const int bw = (ab.getWidth() - 16) / 3;
        clickButton.setBounds (ab.removeFromLeft (bw)); ab.removeFromLeft (8);
        sweepButton.setBounds (ab.removeFromLeft (bw)); ab.removeFromLeft (8);
        noiseButton.setBounds (ab.removeFromLeft (bw));

        renderingIndicator.setBounds (indicatorArea);
    }

    // Sidebar + room view fill the rest.
    auto content = area.reduced (12, 8);
    const int sidebarW = presetBrowser.isCollapsed() ? 32 : 200;
    presetBrowser.setBounds (content.removeFromLeft (sidebarW));
    content.removeFromLeft (12);
    roomView.setBounds (content);
}
