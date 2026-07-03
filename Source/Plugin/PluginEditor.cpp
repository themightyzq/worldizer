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

    // Combo popups in the Worldizer palette (the L&F has no custom combo drawing).
    lookAndFeel.setColour (juce::PopupMenu::backgroundColourId,            Col::surface);
    lookAndFeel.setColour (juce::PopupMenu::textColourId,                  Col::onSurface);
    lookAndFeel.setColour (juce::PopupMenu::highlightedBackgroundColourId, Col::primaryDim);
    lookAndFeel.setColour (juce::PopupMenu::highlightedTextColourId,       Col::background);

    // --- Header buttons ---
    editButton.setClickingTogglesState (true);
    editButton.setTooltip ("Enter / exit the geometry editor (S/D/X tools; Cmd+Z undo; Esc cancel).");
    editButton.onClick = [this] { setEditMode (editButton.getToggleState()); };
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
    roomView.setScene (p.getCurrentScene());
    previousScene = p.getCurrentScene();
    lastSceneRevision = p.getSceneRevision();
    roomView.onSceneEdited = [this] (const Worldizer::Scene& s, bool finalized)
    {
        // Geometry change vs the last committed snapshot? Push the snapshot for undo
        // and mark the session dirty (drives the discard prompt on preset switch).
        if (finalized)
        {
            const auto& oldSG = previousScene.getSectorGeometry().toJson();
            const auto& newSG = s.getSectorGeometry().toJson();
            const bool geometryChanged = juce::JSON::toString (oldSG, true) != juce::JSON::toString (newSG, true);
            if (geometryChanged)
            {
                undoStack.push (previousScene);
                processorRef.markDirty();
            }
            previousScene = s;
        }

        positionsModified = true;
        processorRef.applyEditedScene (s, finalized);
        lastSceneRevision = processorRef.getSceneRevision(); // our own edit — timer must not treat it as external
        // Keep the mic knobs tracking an arrow/icon drag (value-only, no relayout).
        xyAngleSlider.setValue (processorRef.getXYAngleDegrees(), juce::dontSendNotification);
        rotateSlider.setValue (currentRotateAzimuth(), juce::dontSendNotification);
        if (editMode) inspector.setScene (processorRef.getCurrentScene());
        updateSubtitle();
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
    setupAudition (clickButton,  0, "single click (best for hearing one tail decay)");
    setupAudition (clicksButton, 1, "4-click burst");
    setupAudition (sweepButton,  2, "sweep");
    setupAudition (noiseButton,  3, "noise");

    // --- Rendering indicator ---
    renderingIndicator.setText ("rendering...", juce::dontSendNotification);
    renderingIndicator.setJustificationType (juce::Justification::centred);
    renderingIndicator.setColour (juce::Label::textColourId, Col::primary);
    renderingIndicator.setVisible (false);
    addAndMakeVisible (renderingIndicator);

    // --- Mic array controls (control row 2) ---
    auto makeSectionLabel = [this] (juce::Label& l, const juce::String& text, float pt, juce::Colour c)
    {
        l.setText (text, juce::dontSendNotification);
        l.setColour (juce::Label::textColourId, c);
        l.setFont (juce::Font (juce::FontOptions (pt).withStyle ("Bold")));
        l.setJustificationType (juce::Justification::centredLeft);
        addAndMakeVisible (l);
    };
    makeSectionLabel (micSectionLabel, "MIC",     12.0f, Col::primary);
    makeSectionLabel (micConfigLabel,  "Config",  11.0f, Col::onSurfaceVariant);
    makeSectionLabel (micPatternLabel, "Pattern", 11.0f, Col::onSurfaceVariant);
    makeSectionLabel (xyAngleLabel,    "XY Angle", 11.0f, Col::onSurfaceVariant);

    auto styleCombo = [] (juce::ComboBox& c)
    {
        c.setColour (juce::ComboBox::backgroundColourId, Col::surfaceVariant);
        c.setColour (juce::ComboBox::textColourId,       Col::onSurface);
        c.setColour (juce::ComboBox::outlineColourId,    Col::outline);
        c.setColour (juce::ComboBox::arrowColourId,      Col::primary);
    };
    styleCombo (micConfigCombo);
    styleCombo (micPatternCombo);

    // Config selector — ids: 1 Single, 2 Stereo XY, 3 Spaced Pair.
    micConfigCombo.addItem ("Single",      1);
    micConfigCombo.addItem ("Stereo XY",   2);
    micConfigCombo.addItem ("Spaced Pair", 3);
    micConfigCombo.setTooltip ("Microphone configuration. Stereo XY / Spaced Pair produce a true-stereo IR.");
    micConfigCombo.onChange = [this]
    {
        using Cfg = Worldizer::MicArray::Configuration;
        const auto cfg = micConfigCombo.getSelectedId() == 2 ? Cfg::StereoXY
                       : micConfigCombo.getSelectedId() == 3 ? Cfg::SpacedPair
                                                             : Cfg::Single;
        processorRef.setMicConfiguration (cfg); // full re-render
        positionsModified = true;
        refreshRoomViewFromProcessor();
        syncMicControlsFromProcessor();
        updateSubtitle();
    };
    addAndMakeVisible (micConfigCombo);

    // Pattern selector — ids: 1 Omni, 2 Shotgun.
    micPatternCombo.addItem ("Omni",    1);
    micPatternCombo.addItem ("Shotgun", 2);
    micPatternCombo.setTooltip ("Mic polar pattern. Shotgun has a narrow forward lobe - rotate it with the arrow.");
    micPatternCombo.onChange = [this]
    {
        const auto pat = micPatternCombo.getSelectedId() == 2 ? Worldizer::MicPattern::Shotgun
                                                              : Worldizer::MicPattern::Omnidirectional;
        processorRef.setMicPattern (pat); // full re-render
        positionsModified = true;
        refreshRoomViewFromProcessor();
        syncMicControlsFromProcessor(); // show/hide the Rotate knob for shotgun
        updateSubtitle();
    };
    addAndMakeVisible (micPatternCombo);

    // XY splay angle — rotary, 30..180 degrees.
    xyAngleSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    xyAngleSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 56, 16);
    xyAngleSlider.setRange (30.0, 180.0, 1.0);
    xyAngleSlider.setTextValueSuffix (juce::String::fromUTF8 ("\xc2\xb0"));
    xyAngleSlider.setValue (90.0, juce::dontSendNotification);
    xyAngleSlider.setTooltip ("Angle between the two XY capsules. Wider = broader stereo image.");
    xyAngleSlider.onValueChange = [this]
    {
        // Preview while dragging the knob; full quality on text entry / release.
        const bool full = ! xyAngleSlider.isMouseButtonDown();
        processorRef.setXYAngleDegrees ((float) xyAngleSlider.getValue(), full);
        positionsModified = true;
        refreshRoomViewFromProcessor();
    };
    xyAngleSlider.onDragEnd = [this]
    {
        processorRef.setXYAngleDegrees ((float) xyAngleSlider.getValue(), true);
        refreshRoomViewFromProcessor();
    };
    addAndMakeVisible (xyAngleSlider);

    // Rotate — azimuth of the directional element (single mic / XY array facing /
    // the selected spaced-pair mic). Shown only for shotgun. 0deg = +X (east);
    // increases counter-clockwise (matches the top-down view).
    makeSectionLabel (rotateLabel, "Rotate", 11.0f, Col::onSurfaceVariant);
    rotateSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    rotateSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 56, 16);
    rotateSlider.setRange (0.0, 360.0, 1.0);
    rotateSlider.setTextValueSuffix (juce::String::fromUTF8 ("\xc2\xb0"));
    rotateSlider.setTooltip ("Rotate the selected directional mic (or drag its arrow in the room view).");
    rotateSlider.onValueChange = [this]
    {
        const bool full = ! rotateSlider.isMouseButtonDown();
        applyRotate ((float) rotateSlider.getValue(), full);
        positionsModified = true;
        refreshRoomViewFromProcessor();
    };
    rotateSlider.onDragEnd = [this] { applyRotate ((float) rotateSlider.getValue(), true); refreshRoomViewFromProcessor(); };
    addAndMakeVisible (rotateSlider);

    // Clicking a spaced-pair mic re-targets the Rotate knob.
    roomView.onMicSelected = [this] (int) { syncMicControlsFromProcessor(); };

    // Double-clicking a dot in browse mode reverts positions to the preset default.
    roomView.onResetPositions = [this]
    {
        if (processorRef.resetPositionsToPresetDefault())
        {
            positionsModified = false;
            refreshRoomViewFromProcessor();
            syncMicControlsFromProcessor();
            updateSubtitle();
        }
    };

    syncMicControlsFromProcessor();

    // --- Character row (Slice 5.5) + ambient level (Slice 6) ---
    makeSectionLabel (speakerSectionLabel, "SPEAKER", 12.0f, Col::primary);
    makeSectionLabel (micCharSectionLabel, "MIC",     12.0f, Col::primary);
    makeSectionLabel (ambientSectionLabel, "AMBIENT", 12.0f, Col::primary);

    // Pickers: hover = instant audition (the character convolver crossfades
    // internally); click = commit to the parameter; close = restore committed.
    auto wireCharacterPicker = [this] (Worldizer::CharacterPicker& picker, const char* paramId,
                                       std::function<void (int)> audition, std::function<void()> endAudition)
    {
        auto* param = dynamic_cast<juce::AudioParameterChoice*> (processorRef.apvts.getParameter (paramId));
        jassert (param != nullptr);
        picker.setSelectedIndex (param->getIndex());
        picker.onSelected = [param] (int index)
        {
            param->beginChangeGesture();
            param->setValueNotifyingHost (param->convertTo0to1 ((float) index));
            param->endChangeGesture();
        };
        picker.onAudition    = std::move (audition);
        picker.onAuditionEnd = std::move (endAudition);
        addAndMakeVisible (picker);
    };
    // Track whether an audition is actually in flight, so the destructor only
    // ends one that's running (avoids a redundant WAV reload + crossfade on every
    // editor close).
    wireCharacterPicker (speakerPicker, "sourceCharacter",
                         [this] (int i) { sourceAuditionActive = true; processorRef.auditionSourceCharacter (i); },
                         [this] { processorRef.endSourceCharacterAudition(); sourceAuditionActive = false; });
    wireCharacterPicker (micCharPicker, "micCharacter",
                         [this] (int i) { micAuditionActive = true; processorRef.auditionMicCharacter (i); },
                         [this] { processorRef.endMicCharacterAudition(); micAuditionActive = false; });

    // Small knobs matching the mic-row style.
    auto setupSmallKnob = [this] (juce::Slider& s, juce::Label& l, const juce::String& name, const juce::String& tip)
    {
        s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 56, 16);
        s.setTooltip (tip);
        addAndMakeVisible (s);
        l.setText (name, juce::dontSendNotification);
        l.setColour (juce::Label::textColourId, Col::onSurfaceVariant);
        l.setFont (juce::Font (juce::FontOptions (11.0f).withStyle ("Bold")));
        l.setJustificationType (juce::Justification::centredLeft);
        addAndMakeVisible (l);
    };
    setupSmallKnob (driveSlider,   driveLabel,   "Drive",
                    "Light speaker nonlinearity - gently saturates the reproducer before the room.");
    setupSmallKnob (noiseSlider,   noiseLabel,   "Noise",
                    "Microphone self-noise floor added to the wet path.");
    setupSmallKnob (ambientSlider, ambientLabel, "Bed",
                    "Level of the preset's ambient room-tone bed (Off = no bed).");
    driveAttach   = std::make_unique<juce::SliderParameterAttachment> (*p.apvts.getParameter ("sourceDrive"),  driveSlider);
    noiseAttach   = std::make_unique<juce::SliderParameterAttachment> (*p.apvts.getParameter ("micNoise"),     noiseSlider);
    ambientAttach = std::make_unique<juce::SliderParameterAttachment> (*p.apvts.getParameter ("ambientLevel"), ambientSlider);

    // === Edit-mode UI wiring (Slice 6a) — components are hidden until setEditMode(true) ===
    addChildComponent (editorTools);
    editorTools.onToolChanged = [this] (auto t) { roomView.setTool (t); updateStatusBar(); };
    editorTools.onSnapChanged = [this] (bool on) { roomView.setSnapToGrid (on); };
    editorTools.onUndo        = [this] { doUndo(); };

    addChildComponent (inspector);
    inspector.onVertexMoved = [this] (int idx, float x, float y)
    {
        handleInspectorEdit ([idx, x, y] (Worldizer::Scene& s)
        {
            if (! s.getSectorGeometry().sectors.empty())
                s.getSectorGeometry().sectors[0].moveVertex (idx, { x, y });
        });
    };
    inspector.onLineDefMaterialChanged = [this] (int idx, juce::String m)
    {
        handleInspectorEdit ([idx, m] (Worldizer::Scene& s)
        {
            if (! s.getSectorGeometry().sectors.empty()
                && idx >= 0 && (size_t) idx < s.getSectorGeometry().sectors[0].lineDefs.size())
                s.getSectorGeometry().sectors[0].lineDefs[(size_t) idx].frontMaterial = m;
        });
    };
    // setFloor/CeilingHeight keep ceiling >= floor + kMinRoomHeight so the user
    // can't invert or zero the room (which would silently delete every wall).
    inspector.onSectorFloorHeightChanged   = [this] (float h) { handleInspectorEdit ([h] (Worldizer::Scene& s) { if (! s.getSectorGeometry().sectors.empty()) s.getSectorGeometry().sectors[0].setFloorHeight   (h); }); if (editMode) inspector.setScene (processorRef.getCurrentScene()); };
    inspector.onSectorCeilingHeightChanged = [this] (float h) { handleInspectorEdit ([h] (Worldizer::Scene& s) { if (! s.getSectorGeometry().sectors.empty()) s.getSectorGeometry().sectors[0].setCeilingHeight (h); }); if (editMode) inspector.setScene (processorRef.getCurrentScene()); };
    inspector.onSectorFloorMaterialChanged   = [this] (juce::String m) { handleInspectorEdit ([m] (Worldizer::Scene& s) { if (! s.getSectorGeometry().sectors.empty()) s.getSectorGeometry().sectors[0].floorMaterial   = m; }); };
    inspector.onSectorCeilingMaterialChanged = [this] (juce::String m) { handleInspectorEdit ([m] (Worldizer::Scene& s) { if (! s.getSectorGeometry().sectors.empty()) s.getSectorGeometry().sectors[0].ceilingMaterial = m; }); };

    statusBar.setColour (juce::Label::backgroundColourId, Col::surface);
    statusBar.setColour (juce::Label::textColourId,       Col::onSurfaceVariant);
    statusBar.setFont (juce::Font (juce::FontOptions (11.0f)));
    statusBar.setJustificationType (juce::Justification::centredLeft);
    addChildComponent (statusBar);

    roomView.onSelectionChanged = [this] (Worldizer::EditorSelection sel)
    {
        inspector.setSelection (sel);
        updateStatusBar();
    };
    roomView.onSectorRejected = [this]
    {
        statusBar.setText ("That shape can't be used - walls crossed or the area is too small. Draw a simple room.",
                           juce::dontSendNotification);
    };
    roomView.onSectorCreated = [this] (const Worldizer::Sector&)
    {
        // The room view already mutated its own scene + fired onSceneEdited; this hook
        // just marks dirty, swaps the active tool back to Select, and refreshes the UI.
        processorRef.markDirty();
        editorTools.setTool (Worldizer::EditorToolPalette::Tool::Select, juce::dontSendNotification);
        positionsModified = true;
        updateSubtitle();
        updateStatusBar();
    };
    roomView.onDeleteRequested = [this] (Worldizer::EditorSelection sel)
    {
        handleInspectorEdit ([sel] (Worldizer::Scene& s)
        {
            if (s.getSectorGeometry().sectors.empty()) return;
            auto& sector = s.getSectorGeometry().sectors[0];
            if (sel.kind == Worldizer::EditorSelection::Kind::Vertex)
            {
                sector.removeVertex (sel.index);
                if (sector.vertices.size() < 3)
                    s.getSectorGeometry().sectors.clear();
            }
            else if (sel.kind == Worldizer::EditorSelection::Kind::Sector
                  || sel.kind == Worldizer::EditorSelection::Kind::LineDef)
            {
                // Slice 6a: linedef delete collapses the whole sector (see prompt §5.5).
                s.getSectorGeometry().sectors.clear();
            }
        });
        roomView.setSelection ({});
        inspector.setSelection ({});
        updateStatusBar();
    };

    presetBrowser.onSaveAsRequested = [this] { onSaveAsButton(); };
    setWantsKeyboardFocus (true);

    // --- Footer ---
    githubLink.setButtonText ("github.com/themightyzq/worldizer");
    githubLink.setURL (juce::URL ("https://github.com/themightyzq/worldizer"));
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
    // If the window closes mid-hover in a character picker, the CallOutBox's
    // restore callback dies with it (SafePointer) — make sure the processor is
    // not left playing an uncommitted audition IR. Only when one is actually in
    // flight (else this is a needless WAV reload + crossfade on every close).
    if (sourceAuditionActive) processorRef.endSourceCharacterAudition();
    if (micAuditionActive)    processorRef.endMicCharacterAudition();
    setLookAndFeel (nullptr);
}

//==============================================================================
void WorldizerAudioProcessorEditor::onPresetSelected (const juce::String& presetId)
{
    if (processorRef.hasUncommittedEdits())
    {
        // Snap the sidebar back to the current preset visually until the dialog resolves.
        presetBrowser.setSelectedPresetId (processorRef.getCurrentPresetId());
        confirmDiscardThenAsync ([this, presetId] { onPresetSelected (presetId); });
        return;
    }
    processorRef.setCurrentPresetId (presetId);
    positionsModified = false;
    refreshRoomViewFromProcessor();
    syncMicControlsFromProcessor();
    previousScene = processorRef.getCurrentScene();
    undoStack.clear();
    if (editMode) inspector.setScene (previousScene);
    updateSubtitle();
    updateStatusBar();
}

void WorldizerAudioProcessorEditor::refreshRoomViewFromProcessor()
{
    roomView.setScene (processorRef.getCurrentScene());
    lastSceneRevision = processorRef.getSceneRevision(); // this change is now reflected
}

void WorldizerAudioProcessorEditor::applyRotate (float azimuthDeg, bool full)
{
    const float a = juce::degreesToRadians (azimuthDeg);
    const Worldizer::Vec3 dir { std::cos (a), std::sin (a), 0.0f };
    using Cfg = Worldizer::MicArray::Configuration;
    switch (processorRef.getMicConfiguration())
    {
        case Cfg::StereoXY:   processorRef.setXYOrientation (dir, full); break;
        case Cfg::Single:     processorRef.setMicOrientation (0, dir, full); break;
        case Cfg::SpacedPair: processorRef.setMicOrientation (roomView.getSelectedMic(), dir, full); break;
    }
}

float WorldizerAudioProcessorEditor::currentRotateAzimuth() const
{
    using Cfg = Worldizer::MicArray::Configuration;
    const auto cfg = processorRef.getMicConfiguration();

    // Array facing for XY, the selected mic for a spaced pair, else mic 0.
    Worldizer::Vec3 dir = cfg == Cfg::StereoXY   ? processorRef.getCurrentScene().getMicArray().getXYOrientation()
                        : cfg == Cfg::SpacedPair ? processorRef.getMicOrientation (roomView.getSelectedMic())
                                                 : processorRef.getMicOrientation (0);

    float deg = juce::radiansToDegrees (std::atan2 (dir.y, dir.x));
    if (deg < 0.0f) deg += 360.0f;
    return deg;
}

//==============================================================================
// Edit mode (Slice 6a)
//==============================================================================
void WorldizerAudioProcessorEditor::setEditMode (bool on)
{
    if (editMode == on)
    {
        editButton.setToggleState (on, juce::dontSendNotification);
        return;
    }

    // Exiting edit mode with uncommitted edits: prompt async, re-enter on confirm.
    if (! on && processorRef.hasUncommittedEdits())
    {
        editButton.setToggleState (true, juce::dontSendNotification);
        confirmDiscardThenAsync ([this] { setEditMode (false); });
        return;
    }

    editMode = on;
    editButton.setToggleState (on, juce::dontSendNotification);

    roomView.setEditMode (on);
    editorTools.setVisible (on);
    inspector.setVisible (on);
    statusBar.setVisible (on);

    if (on)
    {
        // Make the preset's own room editable: convert its brush shell into a
        // sector (selectable walls / vertices / materials / heights). Interior
        // prop brushes (columns, crates, furniture) stay as fixed obstacles.
        // Conversion alone is not an "edit" — no dirty flag, no undo entry, no
        // re-render; those all start with the user's first actual change.
        shellJustConverted = processorRef.convertRoomShellForEditing();
        refreshRoomViewFromProcessor();

        previousScene = processorRef.getCurrentScene();
        undoStack.clear();
        inspector.setScene (previousScene);
        inspector.setSelection ({});
        roomView.setSelection ({});
        editorTools.setTool (Worldizer::EditorToolPalette::Tool::Select, juce::dontSendNotification);
        roomView.setTool (Worldizer::EditorToolPalette::Tool::Select);
        roomView.setSnapToGrid (editorTools.isSnapOn());
        grabKeyboardFocus();
    }

    resized();
    repaint();
    updateStatusBar();
    updateSubtitle();
}

void WorldizerAudioProcessorEditor::handleInspectorEdit (std::function<void (Worldizer::Scene&)> mutator)
{
    auto edited = processorRef.getCurrentScene();
    mutator (edited);
    // Treat inspector commits as finalized edits (full-quality render).
    if (roomView.onSceneEdited)
        roomView.onSceneEdited (edited, /*finalized*/ true);
    // Keep roomView's local scene in sync so its render reflects the change.
    roomView.setScene (edited);
}

void WorldizerAudioProcessorEditor::doUndo()
{
    if (! undoStack.canUndo()) return;
    auto restored = undoStack.undo();
    previousScene = restored;
    processorRef.applyEditedScene (restored, /*finalized*/ true);
    lastSceneRevision = processorRef.getSceneRevision();
    roomView.setScene (restored);
    inspector.setScene (restored);
    inspector.setSelection ({});
    roomView.setSelection ({});
    updateSubtitle();
    updateStatusBar();
}

void WorldizerAudioProcessorEditor::updateStatusBar()
{
    if (! editMode) return;
    juce::String msg;
    if (roomView.getTool() == Worldizer::EditorToolPalette::Tool::Draw)
    {
        msg = "Draw: click to place vertices; click the first vertex to close the sector (Esc cancels).";
    }
    else if (roomView.getTool() == Worldizer::EditorToolPalette::Tool::Delete)
    {
        msg = "Delete: click an element to remove it.";
    }
    else
    {
        const auto sel = inspector.getSelection();
        switch (sel.kind)
        {
            case Worldizer::EditorSelection::Kind::Vertex:  msg = "Selected: vertex " + juce::String (sel.index); break;
            case Worldizer::EditorSelection::Kind::LineDef: msg = "Selected: wall "   + juce::String (sel.index); break;
            case Worldizer::EditorSelection::Kind::Sector:  msg = "Selected: sector"; break;
            case Worldizer::EditorSelection::Kind::None:
                if (shellJustConverted)
                    msg = "Room converted for editing - click a wall or vertex to select; drag vertices to reshape.";
                else if (processorRef.getCurrentScene().getSectorGeometry().isEmpty()
                         && processorRef.getCurrentScene().getNumBrushes() > 0)
                    msg = "This scene's geometry is fixed (open-air / irregular) - use Draw (D) to add your own sector.";
                else
                    msg = "Select: click a vertex, wall, or sector. Drag a vertex to move it.";
                break;
        }
    }
    if (undoStack.canUndo())
        msg += "    (undo depth " + juce::String ((int) undoStack.getDepth()) + ")";
    statusBar.setText (msg, juce::dontSendNotification);
}

void WorldizerAudioProcessorEditor::confirmDiscardThenAsync (std::function<void()> onProceed)
{
    if (! processorRef.hasUncommittedEdits()) { onProceed(); return; }

    // Plugin builds disallow modal loops, so we show the prompt async and run the
    // continuation on Discard. Cancel just returns without doing anything; the caller
    // has already snapped any UI back to "nothing happened" before calling us.
    auto opts = juce::MessageBoxOptions()
                  .withIconType (juce::MessageBoxIconType::QuestionIcon)
                  .withTitle   ("Unsaved changes")
                  .withMessage ("You have uncommitted geometry edits. Discard them?")
                  .withButton  ("Discard")
                  .withButton  ("Cancel")
                  .withAssociatedComponent (this);
    // SafePointer: the alert is a desktop window that can outlive the editor
    // (host closes the plugin window while the prompt is up) — a raw `this`
    // would be a use-after-free when the user finally clicks.
    juce::Component::SafePointer<WorldizerAudioProcessorEditor> safeThis (this);
    juce::AlertWindow::showAsync (opts, [safeThis, onProceed] (int result)
    {
        if (safeThis == nullptr)
            return;
        if (result == 1) { safeThis->processorRef.clearDirtyFlag(); onProceed(); }
    });
}

void WorldizerAudioProcessorEditor::onSaveAsButton()
{
    auto* w = new juce::AlertWindow ("Save As Preset", "Save the current scene to your user library.",
                                     juce::AlertWindow::NoIcon, this);
    w->addTextEditor ("name", "", "Name");
    w->addTextEditor ("desc", "", "Description (optional)");
    w->addTextEditor ("tags", "", "Tags (comma-separated, optional)");
    // Must match the shipped-preset category strings exactly, or user-saved presets
    // form a separate browser group (e.g. "Vehicles & Devices" vs "Vehicles/Devices").
    const juce::StringArray cats { "Indoor", "Outdoor", "Vehicles & Devices", "Cinematic", "Experimental" };
    w->addComboBox ("cat", cats, "Category");
    w->addButton ("Save",   1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    // SafePointer: the dialog can outlive the editor (host closes the plugin
    // window while it is up) — a raw `this` would dangle.
    juce::Component::SafePointer<WorldizerAudioProcessorEditor> safeThis (this);
    w->enterModalState (true, juce::ModalCallbackFunction::create ([safeThis, w] (int code)
    {
        std::unique_ptr<juce::AlertWindow> owner (w);
        if (code != 1 || safeThis == nullptr)
            return;
        auto* self = safeThis.getComponent();

        const juce::String name = w->getTextEditor ("name")->getText().trim();
        if (name.isEmpty())
        {
            juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon,
                                                    "Save As", "A preset name is required.", "OK", self);
            return;
        }

        const juce::String cat  = w->getComboBoxComponent ("cat")->getText();
        const juce::String desc = w->getTextEditor ("desc")->getText();
        const auto tags         = juce::StringArray::fromTokens (w->getTextEditor ("tags")->getText(), ",", "");

        juce::String err;
        // The user typed the name intentionally — Save As always overwrites a same-named
        // preset. To keep both, pick a different name. (Future polish: ask first.)
        if (! self->processorRef.saveCurrentSceneAsPreset (name, cat, desc, tags, /*overwrite*/ true, err))
        {
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                "Save failed", err, "OK", self);
            return;
        }

        self->presetBrowser.refreshList();
        self->presetBrowser.setSelectedPresetId (self->processorRef.getCurrentPresetId());
        self->refreshRoomViewFromProcessor();
        self->previousScene = self->processorRef.getCurrentScene();
        self->undoStack.clear();
        self->positionsModified = false;
        self->updateSubtitle();
    }), false);
}

bool WorldizerAudioProcessorEditor::keyPressed (const juce::KeyPress& k)
{
    // Cmd/Ctrl+Z anywhere when in edit mode.
    if (k == juce::KeyPress ('z', juce::ModifierKeys::commandModifier, 0))
        { doUndo(); return true; }

    if (! editMode) return false;

    if (k == juce::KeyPress ('s'))    { editorTools.setTool (Worldizer::EditorToolPalette::Tool::Select); return true; }
    if (k == juce::KeyPress ('d'))    { editorTools.setTool (Worldizer::EditorToolPalette::Tool::Draw);   return true; }
    if (k == juce::KeyPress ('x'))    { editorTools.setTool (Worldizer::EditorToolPalette::Tool::Delete); return true; }
    if (k == juce::KeyPress::escapeKey)
    {
        roomView.cancelDrawing();
        roomView.setSelection ({});
        inspector.setSelection ({});
        updateStatusBar();
        return true;
    }
    if (k == juce::KeyPress::deleteKey || k == juce::KeyPress::backspaceKey)
    {
        const auto sel = roomView.getSelection();
        if (sel.kind != Worldizer::EditorSelection::Kind::None && roomView.onDeleteRequested)
            roomView.onDeleteRequested (sel);
        return true;
    }
    return false;
}

void WorldizerAudioProcessorEditor::syncMicControlsFromProcessor()
{
    using Cfg = Worldizer::MicArray::Configuration;
    const auto cfg = processorRef.getMicConfiguration();
    micConfigCombo.setSelectedId (cfg == Cfg::StereoXY ? 2 : cfg == Cfg::SpacedPair ? 3 : 1,
                                  juce::dontSendNotification);
    const bool shotgun = processorRef.getMicPattern() == Worldizer::MicPattern::Shotgun;
    micPatternCombo.setSelectedId (shotgun ? 2 : 1, juce::dontSendNotification);
    xyAngleSlider.setValue (processorRef.getXYAngleDegrees(), juce::dontSendNotification);
    rotateSlider.setValue (currentRotateAzimuth(), juce::dontSendNotification);

    // XY angle applies to the coincident XY pair; Rotate applies to any directional mic.
    const bool isXY = cfg == Cfg::StereoXY;
    xyAngleLabel.setVisible (isXY);
    xyAngleSlider.setVisible (isXY);
    rotateLabel.setVisible (shotgun);
    rotateSlider.setVisible (shotgun);
    // For a spaced pair, the Rotate label names which mic it targets.
    rotateLabel.setText (cfg == Cfg::SpacedPair ? (roomView.getSelectedMic() == 0 ? "Rotate L" : "Rotate R") : "Rotate",
                         juce::dontSendNotification);

    resized(); // visibility changed -> re-lay out the mic row
}

void WorldizerAudioProcessorEditor::updateSubtitle()
{
    // Non-ASCII literals must go through CharPointer_UTF8 — juce::String's
    // plain const char* path decodes them as Latin-1 in Release ("â€¢").
    const juce::String bullet (juce::CharPointer_UTF8 ("  \xe2\x80\xa2  "));
    juce::String preset (juce::CharPointer_UTF8 ("\xe2\x80\x94"));
    if (auto meta = processorRef.getCurrentPresetMetadata())
        preset = meta->name;
    const bool starred = positionsModified || processorRef.hasUncommittedEdits();
    juce::String mode  = editMode ? bullet + "EDITING" : juce::String();
    subtitleText = "v" + juce::String (Worldizer::kVersionString) + bullet + preset
                 + (starred ? "*" : "") + mode;
    repaint();
}

void WorldizerAudioProcessorEditor::timerCallback()
{
    const bool r = processorRef.isRendering();
    if (renderingIndicator.isVisible() != r)
        renderingIndicator.setVisible (r);

    // Output-ceiling clip indicator: hold the dot ~0.8 s after the last catch.
    if (processorRef.getAndClearCeilingActive())
    {
        clipHoldTicks = 8;
        repaint (outputGainSlider.getBounds().expanded (10));
    }
    else if (clipHoldTicks > 0)
    {
        if (--clipHoldTicks == 0)
            repaint (outputGainSlider.getBounds().expanded (10));
    }

    // Keep the character pickers tracking the parameters (automation, preset
    // defaults, state restore) — cheap: setSelectedIndex only repaints on change.
    auto syncPicker = [this] (Worldizer::CharacterPicker& picker, const char* paramId)
    {
        if (auto* param = dynamic_cast<juce::AudioParameterChoice*> (processorRef.apvts.getParameter (paramId)))
            if (param->getIndex() != picker.getSelectedIndex())
                picker.setSelectedIndex (param->getIndex());
    };
    syncPicker (speakerPicker, "sourceCharacter");
    syncPicker (micCharPicker, "micCharacter");

    // Sync the UI if the preset changed outside the browser (e.g. state restore),
    // OR the scene changed under the SAME preset id (state restore with a
    // different mic config / positions / sectors — the id check alone misses it).
    // Editor-originated edits record lastSceneRevision at their call sites, so
    // only out-of-band changes land here.
    const bool presetChanged = processorRef.getCurrentPresetId() != presetBrowser.getSelectedPresetId();
    if (presetChanged || processorRef.getSceneRevision() != lastSceneRevision)
    {
        if (presetChanged)
            presetBrowser.setSelectedPresetId (processorRef.getCurrentPresetId());
        refreshRoomViewFromProcessor();
        syncMicControlsFromProcessor();
        previousScene = processorRef.getCurrentScene();
        undoStack.clear();
        if (editMode) inspector.setScene (previousScene);
        positionsModified = false;
        updateSubtitle();
    }

    // Sidebar collapse is session state too (restored by setStateInformation).
    if (presetBrowser.isCollapsed() != processorRef.getSidebarCollapsed())
    {
        presetBrowser.setCollapsed (processorRef.getSidebarCollapsed());
        resized();
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

    // Control-area dividers: a line above the whole area, a line between the gain row
    // and the mic row, and two vertical cluster dividers within the gain row.
    if (! controlRowBounds.isEmpty())
    {
        g.setColour (Col::outline);
        g.drawHorizontalLine (controlRowBounds.getY(), 0.0f, (float) getWidth());

        const int x1 = knobRowBounds.getX() + (int) (knobRowBounds.getWidth() * 0.48f);
        const int x2 = knobRowBounds.getX() + (int) (knobRowBounds.getWidth() * 0.78f);
        g.drawVerticalLine (x1, (float) knobRowBounds.getY() + 8, (float) knobRowBounds.getBottom() - 8);
        g.drawVerticalLine (x2, (float) knobRowBounds.getY() + 8, (float) knobRowBounds.getBottom() - 8);

        g.drawHorizontalLine (micRowBounds.getY(), 8.0f, (float) getWidth() - 8.0f);
        g.drawHorizontalLine (characterRowBounds.getY(), 8.0f, (float) getWidth() - 8.0f);
    }

    // Output-ceiling clip dot: lights when the safety soft-clip catches the output,
    // so the user can tell "loud but clean" from "loud and being caught".
    {
        const auto ob = outputGainSlider.getBounds();
        const auto dot = juce::Rectangle<float> (8.0f, 8.0f).withCentre (
            { (float) ob.getRight() - 6.0f, (float) ob.getY() + 4.0f });
        g.setColour (clipHoldTicks > 0 ? Col::error : Col::outline.withAlpha (0.5f));
        g.fillEllipse (dot);
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

    // Control area: gain row + mic row + character row.
    controlRowBounds   = area.removeFromBottom (206);
    knobRowBounds      = controlRowBounds.withHeight (86);
    micRowBounds       = controlRowBounds.withTrimmedTop (86).withHeight (64);
    characterRowBounds = controlRowBounds.withTrimmedTop (150);

    {
        auto cr = knobRowBounds.reduced (12, 10);
        auto knobArea     = cr.removeFromLeft ((int) (knobRowBounds.getWidth() * 0.48f));
        auto auditionArea = cr.removeFromLeft ((int) (knobRowBounds.getWidth() * 0.30f));
        auto indicatorArea = cr;

        auto placeKnob = [] (juce::Slider& s, juce::Label& l, juce::Rectangle<int> colm)
        {
            l.setBounds (colm.removeFromBottom (14));
            s.setBounds (colm); // rotary + text box fill the remaining column height
        };
        const int colW = knobArea.getWidth() / 3;
        placeKnob (inputGainSlider,  inputGainLabel,  knobArea.removeFromLeft (colW));
        placeKnob (mixSlider,        mixLabel,        knobArea.removeFromLeft (colW));
        placeKnob (outputGainSlider, outputGainLabel, knobArea);

        auto ab = auditionArea.withSizeKeepingCentre (auditionArea.getWidth() - 12, 30);
        const int bw = (ab.getWidth() - 18) / 4; // four buttons, 6 px gaps
        clickButton.setBounds  (ab.removeFromLeft (bw)); ab.removeFromLeft (6);
        clicksButton.setBounds (ab.removeFromLeft (bw)); ab.removeFromLeft (6);
        sweepButton.setBounds  (ab.removeFromLeft (bw)); ab.removeFromLeft (6);
        noiseButton.setBounds  (ab.removeFromLeft (bw));

        renderingIndicator.setBounds (indicatorArea);
    }

    // Mic row: [MIC]  Config [v]   Pattern [v]   XY Angle (knob)
    {
        auto mr = micRowBounds.reduced (12, 6);
        micSectionLabel.setBounds (mr.removeFromLeft (44).withTrimmedTop (8));

        auto comboCol = [&mr] (juce::Label& l, juce::Component& c, int labelW, int comboW)
        {
            l.setBounds (mr.removeFromLeft (labelW));
            c.setBounds (mr.removeFromLeft (comboW).withSizeKeepingCentre (comboW, 26));
            mr.removeFromLeft (16);
        };
        comboCol (micConfigLabel,  micConfigCombo,  50, 110);
        comboCol (micPatternLabel, micPatternCombo, 54, 96);

        // XY angle + Rotate knobs — placed only when visible (no gaps). Both can show
        // at once for an XY shotgun pair (splay + array facing).
        auto knobCol = [&mr] (juce::Label& l, juce::Slider& s, int labelW)
        {
            if (! s.isVisible()) return;
            l.setBounds (mr.removeFromLeft (labelW).withTrimmedTop (8));
            s.setBounds (mr.removeFromLeft (52));
            mr.removeFromLeft (12);
        };
        knobCol (xyAngleLabel, xyAngleSlider, 60);
        knobCol (rotateLabel,  rotateSlider,  64);
    }

    // Character row: [SPEAKER] picker  Drive | [MIC] picker  Noise | [AMBIENT] Bed
    {
        auto chr = characterRowBounds.reduced (12, 6);

        auto pickerCol = [&chr] (juce::Label& section, Worldizer::CharacterPicker& picker,
                                 juce::Label& knobLabel, juce::Slider& knob, int sectionW, int pickerW)
        {
            section.setBounds (chr.removeFromLeft (sectionW).withTrimmedTop (8));
            picker.setBounds (chr.removeFromLeft (pickerW).withSizeKeepingCentre (pickerW, 26));
            chr.removeFromLeft (10);
            knobLabel.setBounds (chr.removeFromLeft (44).withTrimmedTop (8));
            knob.setBounds (chr.removeFromLeft (52));
            chr.removeFromLeft (18);
        };
        // Responsive: everything except the two pickers is fixed-width; the pickers
        // absorb the remaining slack so the whole row (through AMBIENT/Bed) always
        // fits — never truncating off the right edge at the minimum window width.
        const int fixedSpeaker = 70 + 10 + 44 + 52 + 18; // section + gaps + Drive
        const int fixedMic     = 40 + 10 + 44 + 52 + 18; // section + gaps + Noise
        const int ambientBlock = 72 + 34 + 52;           // AMBIENT + Bed
        const int pickerBudget = chr.getWidth() - fixedSpeaker - fixedMic - ambientBlock;
        const int pickerW = juce::jlimit (90, 170, pickerBudget / 2);

        pickerCol (speakerSectionLabel, speakerPicker, driveLabel, driveSlider, 70, pickerW);
        pickerCol (micCharSectionLabel, micCharPicker, noiseLabel, noiseSlider, 40, pickerW);

        ambientSectionLabel.setBounds (chr.removeFromLeft (72).withTrimmedTop (8));
        ambientLabel.setBounds (chr.removeFromLeft (34).withTrimmedTop (8));
        ambientSlider.setBounds (chr.removeFromLeft (52));
    }

    // Sidebar + (edit-mode panels) + room view fill the rest.
    auto content = area.reduced (12, 8);
    const int sidebarW = presetBrowser.isCollapsed() ? 32 : 200;
    presetBrowser.setBounds (content.removeFromLeft (sidebarW));
    content.removeFromLeft (12);

    if (editMode)
    {
        // Right-anchored inspector, top tool palette, bottom status bar; the room
        // view fills the remaining centre rectangle. At the 900 px default this
        // leaves the room view ~370 px wide — tight but usable; the window resizes.
        const int inspectorW = 260;
        inspector.setBounds (content.removeFromRight (inspectorW));
        content.removeFromRight (8);
        editorTools.setBounds (content.removeFromTop (32));
        statusBar.setBounds   (content.removeFromBottom (24));
        roomView.setBounds (content);
    }
    else
    {
        roomView.setBounds (content);
    }
}
