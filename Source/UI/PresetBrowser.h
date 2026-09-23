#pragma once

#include <JuceHeader.h>
#include "../Plugin/PresetManager.h"

namespace Worldizer
{
/** One preset row: thumbnail + name + category, selectable/hoverable. */
class PresetEntry : public juce::Component
{
public:
    PresetEntry (juce::String id, juce::String name, juce::String category, juce::Image thumbnail);

    std::function<void()> onClicked;

    void setSelected (bool shouldBeSelected);
    const juce::String& getPresetId() const { return presetId; }
    const juce::String& getDisplayName() const { return displayName; }

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

private:
    juce::String presetId, displayName, category;
    juce::Image  thumbnail;
    bool selected = false, hovered = false;
};

/** Collapsible preset library sidebar: search, scrollable list, Save-As, polling. */
class PresetBrowser : public juce::Component,
                      private juce::Timer
{
public:
    explicit PresetBrowser (PresetManager& mgr);
    ~PresetBrowser() override;

    std::function<void (const juce::String& presetId)> onPresetSelected;
    std::function<void()> onCollapseChanged;
    /** Fired by the "+ Save As" button (Slice 6a). The editor shows the dialog and
        invokes the processor's save flow. */
    std::function<void()> onSaveAsRequested;

    /** Fired by the "Rename" button when a USER preset is selected (the button is
        disabled otherwise). The editor shows the rename dialog and calls
        PresetManager::renameUserPreset, then refreshes/reselects here. */
    std::function<void()> onRenameRequested;

    /** External rescan trigger (e.g. after the processor saves a new preset). */
    void refreshList() { rebuildList(); }

    juce::String getSelectedPresetId() const { return selectedPresetId; }
    void setSelectedPresetId (const juce::String& presetId);

    /** Current display name of the selected preset (from its metadata's `name`),
        or empty if nothing is selected / it's no longer in the list. Used to
        prefill the rename dialog. */
    juce::String getSelectedPresetDisplayName() const;

    bool isCollapsed() const noexcept { return collapsed; }
    void setCollapsed (bool shouldBeCollapsed);

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override; // expand when collapsed

private:
    void timerCallback() override;
    void rebuildList();
    void layoutList();
    void updateRenameButtonState();

    PresetManager& presetManager;

    juce::TextButton collapseButton { "<" };
    juce::TextEditor searchBox;
    juce::Viewport   listViewport;
    juce::Component  listContents;
    juce::TextButton saveAsButton { "+ Save As" };
    juce::TextButton renameButton { "Rename" };

    juce::OwnedArray<PresetEntry> entries;
    juce::String selectedPresetId;
    juce::String searchText;
    bool collapsed = false;

    juce::Time lastUserFolderMTime;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetBrowser)
};
} // namespace Worldizer
