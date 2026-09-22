#include "PresetBrowser.h"
#include "RoomView2D.h"
#include "../Shared/WorldizerLookAndFeel.h"

namespace Worldizer
{
//==============================================================================
PresetEntry::PresetEntry (juce::String id, juce::String name, juce::String cat, juce::Image thumb)
    : presetId (std::move (id)), displayName (std::move (name)), category (std::move (cat)), thumbnail (std::move (thumb))
{
    setInterceptsMouseClicks (true, false);
}

void PresetEntry::setSelected (bool s) { if (selected != s) { selected = s; repaint(); } }

void PresetEntry::paint (juce::Graphics& g)
{
    auto b = getLocalBounds();

    if (selected)      g.fillAll (Colors::surfaceVariant);
    else if (hovered)  g.fillAll (Colors::surface.brighter (0.06f));

    if (selected)
    {
        g.setColour (Colors::primary);
        g.fillRect (0, 0, 3, getHeight());
    }

    auto thumbArea = b.removeFromLeft (56).reduced (8);
    if (thumbnail.isValid())
        g.drawImage (thumbnail, thumbArea.toFloat(), juce::RectanglePlacement::centred | juce::RectanglePlacement::onlyReduceInSize);
    else
    {
        g.setColour (Colors::surfaceVariant);
        g.fillRect (thumbArea);
    }

    auto text = b.reduced (4, 6);
    g.setColour (Colors::onSurface);
    g.setFont (juce::Font (juce::FontOptions (13.0f)));
    g.drawText (displayName, text.removeFromTop (20), juce::Justification::centredLeft, true);

    g.setColour (Colors::onSurfaceMuted);
    g.setFont (juce::Font (juce::FontOptions (9.0f).withStyle ("Bold")));
    g.drawText (category.toUpperCase(), text.removeFromTop (14), juce::Justification::centredLeft, true);
}

void PresetEntry::mouseDown (const juce::MouseEvent&) { if (onClicked) onClicked(); }
void PresetEntry::mouseEnter (const juce::MouseEvent&) { hovered = true; repaint(); }
void PresetEntry::mouseExit  (const juce::MouseEvent&) { hovered = false; repaint(); }

//==============================================================================
PresetBrowser::PresetBrowser (PresetManager& mgr) : presetManager (mgr)
{
    collapseButton.setTooltip ("Collapse the preset browser.");
    collapseButton.setTitle ("Collapse Presets");
    collapseButton.setDescription ("Collapse the preset browser.");
    collapseButton.onClick = [this] { setCollapsed (! collapsed); };
    addAndMakeVisible (collapseButton);

    searchBox.setTextToShowWhenEmpty ("search", Colors::onSurfaceMuted);
    searchBox.setFont (juce::Font (juce::FontOptions (13.0f)));
    searchBox.setTitle ("Search Presets");
    searchBox.setDescription ("Filter the preset list by name, category, or tag.");
    searchBox.onTextChange = [this] { searchText = searchBox.getText(); layoutList(); };
    addAndMakeVisible (searchBox);

    listViewport.setViewedComponent (&listContents, false);
    listViewport.setScrollBarsShown (true, false);
    addAndMakeVisible (listViewport);

    saveAsButton.setTooltip ("Save the current scene as a new preset in the user library.");
    saveAsButton.setTitle ("Save As");
    saveAsButton.setDescription ("Save the current scene as a new preset in the user library.");
    saveAsButton.onClick = [this] { if (onSaveAsRequested) onSaveAsRequested(); };
    addAndMakeVisible (saveAsButton);

    rebuildList();

    lastUserFolderMTime = PresetManager::getUserPresetsFolder().getLastModificationTime();
    startTimer (2000); // poll the user folder for external changes (stat only; no FS write)
}

PresetBrowser::~PresetBrowser() { stopTimer(); }

void PresetBrowser::setSelectedPresetId (const juce::String& id)
{
    selectedPresetId = id;
    for (auto* e : entries)
        e->setSelected (e->getPresetId() == id);
}

void PresetBrowser::setCollapsed (bool shouldBeCollapsed)
{
    if (collapsed == shouldBeCollapsed)
        return;

    collapsed = shouldBeCollapsed;
    collapseButton.setButtonText (collapsed ? ">" : "<");

    searchBox.setVisible (! collapsed);
    listViewport.setVisible (! collapsed);
    saveAsButton.setVisible (! collapsed);

    resized();
    repaint();
    if (onCollapseChanged)
        onCollapseChanged();
}

void PresetBrowser::rebuildList()
{
    entries.clear();
    listContents.removeAllChildren();

    const auto all = presetManager.getAvailablePresetMetadata();
    for (int i = 0; i < all.size(); ++i)
    {
        const auto& m = all.getReference (i);
        juce::Image thumb = m.thumbnail.isValid() ? m.thumbnail : renderSceneThumbnail (m.scene, 40, 40);

        auto* e = new PresetEntry (m.presetId, m.name, m.category, thumb);
        e->setAccessible (true);
        e->setTitle (m.name);
        e->setDescription (m.category + " preset. Click to load.");
        const juce::String id = m.presetId;
        e->onClicked = [this, id]
        {
            setSelectedPresetId (id);
            if (onPresetSelected)
                onPresetSelected (id);
        };
        e->setSelected (m.presetId == selectedPresetId);
        // Stash a search key in the component name (lowercased name+category+tags).
        e->setName ((m.name + " " + m.category + " " + m.tags.joinIntoString (" ")).toLowerCase());
        entries.add (e);
        listContents.addAndMakeVisible (e);
    }

    layoutList();
}

void PresetBrowser::layoutList()
{
    const int w = juce::jmax (10, listViewport.getWidth() - 8);
    const int rowH = 56;
    const auto needle = searchText.trim().toLowerCase();

    int y = 0;
    for (auto* e : entries)
    {
        const bool match = needle.isEmpty() || e->getName().contains (needle);
        e->setVisible (match);
        if (match)
        {
            e->setBounds (0, y, w, rowH);
            y += rowH;
        }
    }
    listContents.setSize (w, juce::jmax (y, listViewport.getHeight()));
}

void PresetBrowser::timerCallback()
{
    const auto folder = PresetManager::getUserPresetsFolder();
    const auto mtime = folder.getLastModificationTime();
    if (mtime != lastUserFolderMTime)
    {
        lastUserFolderMTime = mtime;
        presetManager.rescan();
        rebuildList();
    }
}

void PresetBrowser::mouseDown (const juce::MouseEvent&)
{
    if (collapsed)
        setCollapsed (false);
}

void PresetBrowser::paint (juce::Graphics& g)
{
    // Titled-panel chrome (zqsfx::ui::Panel's drawing, reproduced here rather than wrapping
    // the component in a Panel, so the existing child layout is untouched): gradient face,
    // hard border, faint top inner highlight, silk title over a ruleTitle hairline.
    auto r = getLocalBounds().toFloat();
    g.setGradientFill (zqsfx::ui::gradients::panel (r));
    g.fillRect (r);
    g.setColour (juce::Colours::white.withAlpha (0.04f));
    g.fillRect (r.withHeight (1.0f).translated (0.0f, 1.0f));
    g.setColour (zqsfx::ui::colour::panelBorder);
    g.drawRect (r, 1.0f);

    if (collapsed)
    {
        g.setColour (Colors::onSurfaceVariant);
        g.setFont (juce::Font (juce::FontOptions (11.0f).withStyle ("Bold")));
        juce::Graphics::ScopedSaveState save (g);
        g.addTransform (juce::AffineTransform::rotation (-juce::MathConstants<float>::halfPi,
                                                         getWidth() * 0.5f, getHeight() * 0.5f));
        g.drawText ("BROWSE", juce::Rectangle<int> (-getHeight() / 2, 0, getHeight(), getWidth()).withCentre ({ getWidth() / 2, getHeight() / 2 }),
                    juce::Justification::centred);
        return;
    }

    g.setColour (zqsfx::ui::colour::silkTitle);
    g.setFont (juce::Font (juce::FontOptions (14.0f, juce::Font::bold)).withExtraKerningFactor (0.27f));
    g.drawText ("PRESETS", 12, 0, getWidth() - 50, 28, juce::Justification::centredLeft);
    g.setColour (zqsfx::ui::colour::ruleTitle);
    g.fillRect (juce::Rectangle<int> (8, 26, getWidth() - 16, 1));
}

void PresetBrowser::resized()
{
    auto b = getLocalBounds();

    if (collapsed)
    {
        collapseButton.setBounds (b.removeFromTop (28).reduced (4));
        return;
    }

    auto header = b.removeFromTop (28);
    collapseButton.setBounds (header.removeFromRight (28).reduced (3));

    searchBox.setBounds (b.removeFromTop (32).reduced (8, 4));
    saveAsButton.setBounds (b.removeFromBottom (36).reduced (8, 6));
    listViewport.setBounds (b.reduced (4, 2));

    layoutList();
}
} // namespace Worldizer
