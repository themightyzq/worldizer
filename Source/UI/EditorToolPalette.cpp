#include "EditorToolPalette.h"
#include "../Shared/WorldizerLookAndFeel.h"

namespace Worldizer
{
EditorToolPalette::EditorToolPalette()
{
    auto setupToolButton = [this] (juce::TextButton& b, Tool t, const juce::String& tip)
    {
        b.setClickingTogglesState (true);
        b.setRadioGroupId (1, juce::dontSendNotification);
        b.setTooltip (tip);
        b.setTitle (b.getButtonText());
        b.setDescription (tip);
        b.onClick = [this, t] { setTool (t); };
        addAndMakeVisible (b);
    };
    setupToolButton (selectButton, Tool::Select, "Select (S): click to select; drag to move.");
    setupToolButton (drawButton,   Tool::Draw,   "Draw (D): click to place vertices; click the first vertex to close the sector.");
    setupToolButton (deleteButton, Tool::Delete, "Delete (X): click to delete the element under the cursor.");

    snapButton.setTooltip ("1 m grid snap. Hold Shift while dragging to disable.");
    snapButton.setTitle ("Snap");
    snapButton.setDescription ("1 m grid snap. Hold Shift while dragging to disable.");
    snapButton.setToggleState (true, juce::dontSendNotification);
    snapButton.onClick = [this] { if (onSnapChanged) onSnapChanged (snapButton.getToggleState()); };
    addAndMakeVisible (snapButton);

    undoButton.setTooltip ("Undo last edit (Cmd/Ctrl+Z).");
    undoButton.setTitle ("Undo");
    undoButton.setDescription ("Undo last edit.");
    undoButton.onClick = [this] { if (onUndo) onUndo(); };
    addAndMakeVisible (undoButton);

    setTool (Tool::Select, juce::dontSendNotification);
}

void EditorToolPalette::setTool (Tool t, juce::NotificationType notify)
{
    current = t;
    selectButton.setToggleState (t == Tool::Select, juce::dontSendNotification);
    drawButton.setToggleState   (t == Tool::Draw,   juce::dontSendNotification);
    deleteButton.setToggleState (t == Tool::Delete, juce::dontSendNotification);
    updateToolButtonHighlights();
    if (notify == juce::sendNotification && onToolChanged)
        onToolChanged (t);
}

void EditorToolPalette::updateToolButtonHighlights()
{
    // The L&F's drawButtonBackground reads toggle state for the active fill; nothing
    // extra to do — repaint to reflect the new selection.
    repaint();
}

void EditorToolPalette::paint (juce::Graphics& g)
{
    g.fillAll (Colors::surface);
    g.setColour (Colors::outline);
    g.drawHorizontalLine (getHeight() - 1, 0.0f, (float) getWidth());
}

void EditorToolPalette::resized()
{
    auto r = getLocalBounds().reduced (4, 3);
    auto place = [&r] (juce::Component& c, int w) { c.setBounds (r.removeFromLeft (w)); r.removeFromLeft (4); };
    place (selectButton, 64);
    place (drawButton,   60);
    place (deleteButton, 64);
    r.removeFromLeft (12);
    place (snapButton,   80);
    r.removeFromLeft (12);
    place (undoButton,   60);
}
} // namespace Worldizer
