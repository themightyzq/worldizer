#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "../Shared/WorldizerLookAndFeel.h"
#include "../UI/RoomView2D.h"
#include "../UI/PresetBrowser.h"
#include "../UI/EditorToolPalette.h"
#include "../UI/InspectorPanel.h"
#include "../UI/UndoStack.h"
#include "../UI/CharacterPicker.h"

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
    bool keyPressed (const juce::KeyPress&) override;

private:
    void timerCallback() override;
    void onPresetSelected (const juce::String& presetId);
    void updateSubtitle();
    void syncMicControlsFromProcessor();   // combos / angle / rotate <- processor (no notify)
    void refreshRoomViewFromProcessor();    // roomView scene <- processor live scene
    void applyRotate (float azimuthDegrees, bool fullQuality); // Rotate knob -> selected directional element
    float currentRotateAzimuth() const;     // azimuth of the current Rotate target, degrees

    // === Edit mode (Slice 6a) ===
    void setEditMode (bool on);
    void onSaveAsButton();                     // shows the modal Save-As dialog
    void handleInspectorEdit (std::function<void (Worldizer::Scene&)> mutator); // push undo + apply
    void confirmDiscardThenAsync (std::function<void()> onProceed);              // discard prompt
    void doUndo();
    void updateStatusBar();

    WorldizerAudioProcessor& processorRef;
    WorldizerLookAndFeel lookAndFeel;

    // Header
    juce::TextButton editButton { "Edit" };
    juce::TextButton bypassButton { "Bypass" };
    juce::String subtitleText;

    // Sidebar + main view
    Worldizer::PresetBrowser presetBrowser;
    Worldizer::RoomView2D    roomView;

    // Control row 1: gain knobs + audition + indicator
    juce::Slider inputGainSlider, mixSlider, outputGainSlider;
    juce::Label  inputGainLabel, mixLabel, outputGainLabel;
    juce::TextButton clickButton { "Click" }, clicksButton { "Clicks" }, sweepButton { "Sweep" }, noiseButton { "Noise" };
    juce::Label  renderingIndicator;

    // Control row 2: mic array configuration / pattern / XY angle / rotate
    juce::Label    micSectionLabel, micConfigLabel, micPatternLabel, xyAngleLabel, rotateLabel;
    juce::ComboBox micConfigCombo, micPatternCombo;
    juce::Slider   xyAngleSlider, rotateSlider;

    // Control row 3 (Slices 5.5/6): character pickers + drive/noise/ambient knobs
    juce::Label  speakerSectionLabel, micCharSectionLabel, ambientSectionLabel;
    Worldizer::CharacterPicker speakerPicker { Worldizer::CharacterLibrary::speakers() };
    Worldizer::CharacterPicker micCharPicker { Worldizer::CharacterLibrary::mics() };
    juce::Slider driveSlider, noiseSlider, ambientSlider;
    juce::Label  driveLabel, noiseLabel, ambientLabel;

    // Footer
    juce::HyperlinkButton githubLink;

    juce::Rectangle<int> controlRowBounds;  // whole control area (rows 1+2+3)
    juce::Rectangle<int> knobRowBounds;     // row 1 only (cluster dividers)
    juce::Rectangle<int> micRowBounds;      // row 2 only (MIC section)
    juce::Rectangle<int> characterRowBounds; // row 3 (SPEAKER / MIC CHARACTER / AMBIENT)
    bool positionsModified = false;
    int lastSceneRevision = -1; // editor-known scene revision; timer detects out-of-band changes

    // === Edit-mode UI (Slice 6a) ===
    bool editMode = false;
    Worldizer::EditorToolPalette editorTools;
    Worldizer::InspectorPanel    inspector;
    Worldizer::UndoStack         undoStack;
    juce::Label                  statusBar;
    Worldizer::Scene             previousScene; // snapshot for the next undo push

    std::unique_ptr<juce::SliderParameterAttachment> inputGainAttach, mixAttach, outputGainAttach,
                                                     driveAttach, noiseAttach, ambientAttach;
    std::unique_ptr<juce::ButtonParameterAttachment> bypassAttach;

    juce::TooltipWindow tooltipWindow { this, 500 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WorldizerAudioProcessorEditor)
};
