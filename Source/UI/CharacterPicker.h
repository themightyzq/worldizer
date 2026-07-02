#pragma once

#include <JuceHeader.h>
#include "../DSP/CharacterLibrary.h"

namespace Worldizer
{
/**
    A character selector: a button showing the current selection that opens a
    CallOutBox list of the library. Hovering a row AUDITIONS it immediately
    (onAudition — the processor swaps the character IR, which crossfades
    internally, so this is click-free and instant); clicking commits
    (onSelected); closing the popup without committing restores the previous
    selection (onAuditionEnd).

    Used for both the speaker (source character) and mic character lists.
    Plugin-modal-safe: the popup is a non-modal CallOutBox.
*/
class CharacterPicker : public juce::Component
{
public:
    explicit CharacterPicker (const std::vector<CharacterDef>& defsIn);

    std::function<void (int)> onSelected;    // user committed a row
    std::function<void (int)> onAudition;    // user is hovering a row
    std::function<void()>     onAuditionEnd; // popup closed (after any onSelected)

    void setSelectedIndex (int index);       // UI only — does not notify
    int  getSelectedIndex() const noexcept   { return selected; }

    void resized() override;

private:
    class ListContent;
    void showMenu();

    const std::vector<CharacterDef>& defs;
    int selected = 0;
    juce::TextButton button;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CharacterPicker)
};
} // namespace Worldizer
