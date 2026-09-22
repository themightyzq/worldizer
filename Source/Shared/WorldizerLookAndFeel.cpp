#include "WorldizerLookAndFeel.h"
#include <cmath>

namespace Worldizer
{
WorldizerLookAndFeel::WorldizerLookAndFeel()
{
    // The base zqsfx::ui::LookAndFeel constructor already set the house colours this class
    // used to set itself: ResizableWindow background, ComboBox/PopupMenu -> LCD glass,
    // TextButton -> btn gradient / accent-on, Slider textbox -> LCD glass + glow,
    // TooltipWindow/AlertWindow -> house tokens. rotarySliderFillColourId /
    // rotarySliderOutlineColourId are gone too: the house's filmstrip knobs carry their own
    // pointer and consult no per-slider colour at all (zqsfx::ui::LookAndFeel::drawRotarySlider
    // / drawVectorKnob). Only the ToggleButton colours this class's own drawToggleButton below
    // reads need setting here.
    setColour (juce::ToggleButton::textColourId, Colors::onSurfaceVariant);
    setColour (juce::ToggleButton::tickColourId, Colors::primary);

    setColour (juce::CaretComponent::caretColourId, zqsfx::ui::colour::lcdText);
    setColour (juce::ScrollBar::thumbColourId, zqsfx::ui::colour::ledOffRim);
}

// ---------------------------------------------------------------------- Toggle buttons
void WorldizerLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                                             bool /*shouldDrawButtonAsHighlighted*/, bool /*shouldDrawButtonAsDown*/)
{
    namespace colour = zqsfx::ui::colour;

    auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = button.getToggleState();

    // Hard-edged rectangle -- no tick box, no rounded pill (style guide section 6).
    if (on)
    {
        g.setColour (colour::accent);
        g.fillRect (bounds);
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.fillRect (bounds.withTop (bounds.getBottom() - 2.0f));
    }
    else
    {
        g.setGradientFill (zqsfx::ui::gradients::button (bounds, button.isEnabled()));
        g.fillRect (bounds);
        g.setColour (juce::Colours::white.withAlpha (button.isEnabled() ? 0.07f : 0.0f));
        g.fillRect (bounds.removeFromTop (1.0f));
    }
    g.setColour (colour::btnBorder);
    g.drawRect (button.getLocalBounds().toFloat(), 1.0f);

    const auto textColour = ! button.isEnabled() ? colour::silkCaption
                           : on                   ? colour::accentInk
                                                   : button.findColour (juce::ToggleButton::textColourId);
    g.setColour (textColour);
    g.setFont (silkFont (12.0f, true));
    g.drawText (button.getButtonText(), button.getLocalBounds().reduced (4, 0), juce::Justification::centredLeft, false);
}

// ---------------------------------------------------------------------- Text editors (LCD look)
void WorldizerLookAndFeel::fillTextEditorBackground (juce::Graphics& g, int width, int height, juce::TextEditor&)
{
    // Phosphor screen glass, same treatment as every other LCD field in the house look. Too
    // small at these field heights to carry scanlines legibly.
    drawScreen (g, juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height), false);
}

void WorldizerLookAndFeel::drawTextEditorOutline (juce::Graphics& g, int width, int height, juce::TextEditor& ed)
{
    // drawScreen already drew the bezel + lcdBorder edge; add the house accent focus ring on
    // top when focused. Hard rectangle, no rounded corners (style guide section 6).
    if (ed.hasKeyboardFocus (true))
    {
        g.setColour (zqsfx::ui::colour::accent);
        g.drawRect (0.0f, 0.0f, (float) width, (float) height, 1.0f);
    }
}

// ---------------------------------------------------------------------- Scrollbar
void WorldizerLookAndFeel::drawScrollbar (juce::Graphics& g, juce::ScrollBar&, int x, int y, int width, int height,
                                          bool isVertical, int thumbStart, int thumbSize,
                                          bool isMouseOver, bool isMouseDown)
{
    namespace colour = zqsfx::ui::colour;
    juce::Rectangle<int> thumb;
    if (isVertical)
        thumb = { x + width / 3, thumbStart, juce::jmax (3, width / 3), thumbSize };
    else
        thumb = { thumbStart, y + height / 3, thumbSize, juce::jmax (3, height / 3) };

    // Hard-edged rectangle, no rounded ends (style guide section 6).
    g.setColour ((isMouseOver || isMouseDown) ? colour::accent : colour::ledOffRim);
    g.fillRect (thumb);
}
} // namespace Worldizer
