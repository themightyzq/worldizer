#include "WorldizerLookAndFeel.h"
#include <cmath>

namespace Worldizer
{
WorldizerLookAndFeel::WorldizerLookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId,   Colors::background);

    setColour (juce::Slider::textBoxTextColourId,           Colors::onSurface);
    setColour (juce::Slider::textBoxOutlineColourId,        juce::Colours::transparentBlack);
    setColour (juce::Slider::rotarySliderFillColourId,      Colors::primary);
    setColour (juce::Slider::rotarySliderOutlineColourId,   Colors::surfaceVariant);

    setColour (juce::Label::textColourId,                   Colors::onSurface);

    setColour (juce::TextButton::buttonColourId,            Colors::surfaceVariant);
    setColour (juce::TextButton::buttonOnColourId,          Colors::primary);
    setColour (juce::TextButton::textColourOffId,           Colors::onSurface);
    setColour (juce::TextButton::textColourOnId,            Colors::background);

    setColour (juce::TextEditor::backgroundColourId,        Colors::surface);
    setColour (juce::TextEditor::textColourId,              Colors::onSurface);
    setColour (juce::TextEditor::highlightColourId,         Colors::primaryAlpha40);
    setColour (juce::TextEditor::outlineColourId,           juce::Colours::transparentBlack);
    setColour (juce::CaretComponent::caretColourId,         Colors::primary);

    setColour (juce::PopupMenu::backgroundColourId,         Colors::surface);
    setColour (juce::PopupMenu::textColourId,               Colors::onSurface);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, Colors::primaryAlpha40);

    setColour (juce::TooltipWindow::backgroundColourId,     Colors::surface);
    setColour (juce::TooltipWindow::textColourId,           Colors::onSurface);
    setColour (juce::TooltipWindow::outlineColourId,        Colors::outline);

    setColour (juce::ScrollBar::thumbColourId,              Colors::outline);

    setColour (juce::HyperlinkButton::textColourId,         Colors::onSurfaceVariant);
}

void WorldizerLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                             float sliderPos, float rotaryStartAngle,
                                             float rotaryEndAngle, juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (4.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto c = bounds.getCentre();
    const float thickness = juce::jmax (3.0f, radius * 0.14f);
    const float arcR = radius - thickness * 0.5f;
    const float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

    juce::Path track;
    track.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
    g.setColour (Colors::surfaceVariant);
    g.strokePath (track, juce::PathStrokeType (thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const bool active = slider.isMouseOverOrDragging() && slider.isEnabled();
    juce::Path value;
    value.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, rotaryStartAngle, angle, true);
    g.setColour (active ? Colors::primary.brighter (0.15f) : Colors::primary);
    g.strokePath (value, juce::PathStrokeType (thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Hub
    g.setColour (Colors::surface.brighter (0.04f));
    g.fillEllipse (juce::Rectangle<float> (arcR * 1.2f, arcR * 1.2f).withCentre (c));

    // Indicator pip (angle measured clockwise from 12 o'clock, JUCE convention).
    const float pipR = juce::jmax (2.5f, radius * 0.10f);
    const juce::Point<float> pip (c.x + arcR * std::sin (angle), c.y - arcR * std::cos (angle));
    g.setColour (Colors::onSurface);
    g.fillEllipse (juce::Rectangle<float> (pipR * 2.0f, pipR * 2.0f).withCentre (pip));
}

void WorldizerLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button,
                                                 const juce::Colour&, bool highlighted, bool down)
{
    const auto b = button.getLocalBounds().toFloat().reduced (0.5f);
    const float radius = 4.0f;
    const bool on = button.getToggleState();

    juce::Colour fill;
    if (! button.isEnabled())   fill = Colors::surface;
    else if (on)                fill = down ? Colors::primaryDim : Colors::primary;
    else if (down)              fill = Colors::primaryDim.withAlpha (0.4f);
    else if (highlighted)       fill = Colors::surfaceVariant.brighter (0.25f);
    else                        fill = Colors::surfaceVariant;

    g.setColour (fill);
    g.fillRoundedRectangle (b, radius);
    g.setColour (on ? Colors::primary : Colors::outline);
    g.drawRoundedRectangle (b, radius, 1.0f);
}

void WorldizerLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& button, bool, bool)
{
    g.setFont (getTextButtonFont (button, button.getHeight()));

    juce::Colour c;
    if (! button.isEnabled())            c = Colors::onSurfaceMuted;
    else if (button.getToggleState())    c = Colors::background;
    else                                 c = Colors::onSurface;

    g.setColour (c);
    g.drawFittedText (button.getButtonText(), button.getLocalBounds().reduced (4, 0),
                      juce::Justification::centred, 1);
}

juce::Font WorldizerLookAndFeel::getTextButtonFont (juce::TextButton&, int)
{
    return juce::Font (juce::FontOptions (13.0f));
}

void WorldizerLookAndFeel::fillTextEditorBackground (juce::Graphics& g, int width, int height, juce::TextEditor&)
{
    g.setColour (Colors::surface);
    g.fillRoundedRectangle (0.0f, 0.0f, (float) width, (float) height, 4.0f);
}

void WorldizerLookAndFeel::drawTextEditorOutline (juce::Graphics& g, int width, int height, juce::TextEditor& ed)
{
    g.setColour (ed.hasKeyboardFocus (true) ? Colors::primary : Colors::outline);
    g.drawRoundedRectangle (0.5f, 0.5f, (float) width - 1.0f, (float) height - 1.0f, 4.0f, 1.0f);
}

void WorldizerLookAndFeel::drawScrollbar (juce::Graphics& g, juce::ScrollBar&, int x, int y, int width, int height,
                                          bool isVertical, int thumbStart, int thumbSize,
                                          bool isMouseOver, bool isMouseDown)
{
    juce::Rectangle<int> thumb;
    if (isVertical)
        thumb = { x + width / 3, thumbStart, juce::jmax (3, width / 3), thumbSize };
    else
        thumb = { thumbStart, y + height / 3, thumbSize, juce::jmax (3, height / 3) };

    g.setColour ((isMouseOver || isMouseDown) ? Colors::onSurfaceMuted : Colors::outline);
    g.fillRoundedRectangle (thumb.toFloat(), (float) juce::jmin (thumb.getWidth(), thumb.getHeight()) * 0.5f);
}
} // namespace Worldizer
