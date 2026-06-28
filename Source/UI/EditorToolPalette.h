#pragma once

#include <JuceHeader.h>

namespace Worldizer
{
/**
    The editor's top toolbar. Three exclusive tools — Select / Draw / Delete —
    plus a 1 m grid-snap toggle and an Undo button. Keyboard shortcuts (S / D / X /
    Cmd+Z / Esc) are dispatched by the editor; this component only owns the buttons.
*/
class EditorToolPalette : public juce::Component
{
public:
    enum class Tool { Select, Draw, Delete };

    EditorToolPalette();

    void setTool (Tool t, juce::NotificationType notify = juce::sendNotification);
    Tool getTool() const noexcept     { return current; }

    bool isSnapOn() const noexcept    { return snapButton.getToggleState(); }
    void setSnapOn (bool on)          { snapButton.setToggleState (on, juce::dontSendNotification); }

    std::function<void (Tool)> onToolChanged;
    std::function<void()>      onUndo;
    std::function<void (bool)> onSnapChanged;

    void resized() override;
    void paint (juce::Graphics&) override;

private:
    juce::TextButton selectButton { "Select" };
    juce::TextButton drawButton   { "Draw" };
    juce::TextButton deleteButton { "Delete" };
    juce::ToggleButton snapButton { "Snap 1m" };
    juce::TextButton undoButton   { "Undo" };
    Tool current = Tool::Select;

    void updateToolButtonHighlights();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EditorToolPalette)
};
} // namespace Worldizer
