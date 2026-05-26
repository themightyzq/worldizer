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

    juce::String getSelectedPresetId() const { return selectedPresetId; }
    void setSelectedPresetId (const juce::String& presetId);

    bool isCollapsed() const noexcept { return collapsed; }
    void setCollapsed (bool shouldBeCollapsed);

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override; // expand when collapsed

private:
    void timerCallback() override;
    void rebuildList();
    void layoutList();

    PresetManager& presetManager;

    juce::TextButton collapseButton { "<" };
    juce::TextEditor searchBox;
    juce::Viewport   listViewport;
    juce::Component  listContents;
    juce::TextButton saveAsButton { "+ Save As" };

    juce::OwnedArray<PresetEntry> entries;
    juce::String selectedPresetId;
    juce::String searchText;
    bool collapsed = false;

    juce::Time lastUserFolderMTime;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetBrowser)
};
} // namespace Worldizer
