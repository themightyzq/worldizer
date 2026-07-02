#include "CharacterPicker.h"
#include "../Shared/WorldizerLookAndFeel.h"

namespace Worldizer
{
namespace Col = Worldizer::Colors;

//==============================================================================
class CharacterPicker::ListContent : public juce::Component
{
public:
    ListContent (CharacterPicker& ownerIn) : owner (&ownerIn)
    {
        setSize (kWidth, kRowHeight * (int) ownerIn.defs.size());
    }

    ~ListContent() override
    {
        // Popup closed (selection, click-away, Esc). If a row was committed the
        // owner's onSelected already ran; restoring from the committed value here
        // is then a no-op — otherwise it cancels the hover audition.
        if (owner != nullptr && owner->onAuditionEnd != nullptr)
            owner->onAuditionEnd();
    }

    void paint (juce::Graphics& g) override
    {
        if (owner == nullptr)
            return;
        const auto& list = owner->defs;
        for (int i = 0; i < (int) list.size(); ++i)
        {
            auto row = juce::Rectangle<int> (0, i * kRowHeight, getWidth(), kRowHeight);
            const bool isHover    = i == hoveredRow;
            const bool isSelected = i == owner->selected;

            if (isHover)
                g.setColour (Col::primaryDim);
            else if (isSelected)
                g.setColour (Col::surfaceVariant);
            else
                g.setColour (Col::surface);
            g.fillRect (row);

            auto text = row.reduced (10, 3);
            g.setColour (isHover ? Col::background : Col::onSurface);
            g.setFont (juce::Font (juce::FontOptions (13.0f)));
            g.drawText (list[(size_t) i].name, text.removeFromTop (17), juce::Justification::centredLeft);

            g.setColour (isHover ? Col::background.withAlpha (0.8f) : Col::onSurfaceVariant);
            g.setFont (juce::Font (juce::FontOptions (10.5f)));
            g.drawText (list[(size_t) i].description, text, juce::Justification::centredLeft);
        }
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        const int row = juce::jlimit (0, (int) (owner != nullptr ? owner->defs.size() : 1) - 1,
                                      e.y / kRowHeight);
        if (row != hoveredRow)
        {
            hoveredRow = row;
            repaint();
            if (owner != nullptr && owner->onAudition != nullptr)
                owner->onAudition (row);
        }
    }

    void mouseExit (const juce::MouseEvent&) override
    {
        hoveredRow = -1;
        repaint();
        // Keep the last-hovered audition running while the popup is open — flicker
        // between rows and the frame is fine; the destructor restores on close.
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (owner == nullptr)
            return;
        const int row = e.y / kRowHeight;
        if (e.mouseWasClicked() && row >= 0 && row < (int) owner->defs.size())
        {
            owner->setSelectedIndex (row);
            if (owner->onSelected != nullptr)
                owner->onSelected (row);
            if (auto* box = findParentComponentOfClass<juce::CallOutBox>())
                box->dismiss();
        }
    }

private:
    static constexpr int kWidth = 280;
    static constexpr int kRowHeight = 38;

    juce::Component::SafePointer<CharacterPicker> owner;
    int hoveredRow = -1;
};

//==============================================================================
CharacterPicker::CharacterPicker (const std::vector<CharacterDef>& defsIn)
    : defs (defsIn)
{
    button.onClick = [this] { showMenu(); };
    addAndMakeVisible (button);
    setSelectedIndex (0);
}

void CharacterPicker::setSelectedIndex (int index)
{
    selected = juce::jlimit (0, (int) defs.size() - 1, index);
    button.setButtonText (defs[(size_t) selected].name);
}

void CharacterPicker::resized()
{
    button.setBounds (getLocalBounds());
}

void CharacterPicker::showMenu()
{
    auto content = std::make_unique<ListContent> (*this);
    juce::CallOutBox::launchAsynchronously (std::move (content),
                                            getScreenBounds(), nullptr);
}
} // namespace Worldizer
