#include "WorldizerLookAndFeel.h"

const juce::Colour WorldizerLookAndFeel::Colors::background { 0xff1a1a1a };
const juce::Colour WorldizerLookAndFeel::Colors::surface    { 0xff2a2a2a };
const juce::Colour WorldizerLookAndFeel::Colors::onSurface  { 0xffe0e0e0 };
const juce::Colour WorldizerLookAndFeel::Colors::outline    { 0xff6a6a6a };
const juce::Colour WorldizerLookAndFeel::Colors::accent     { 0xffffab00 }; // warm amber

WorldizerLookAndFeel::WorldizerLookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, Colors::background);

    setColour (juce::Slider::textBoxTextColourId,        Colors::onSurface);
    setColour (juce::Slider::textBoxOutlineColourId,     juce::Colours::transparentBlack);
    setColour (juce::Slider::rotarySliderFillColourId,   Colors::accent);
    setColour (juce::Slider::rotarySliderOutlineColourId, Colors::surface);

    setColour (juce::Label::textColourId,                Colors::onSurface);

    setColour (juce::ComboBox::backgroundColourId,       Colors::surface);
    setColour (juce::ComboBox::textColourId,             Colors::onSurface);
    setColour (juce::ComboBox::outlineColourId,          Colors::outline);
    setColour (juce::ComboBox::arrowColourId,            Colors::accent);

    setColour (juce::PopupMenu::backgroundColourId,      Colors::surface);
    setColour (juce::PopupMenu::textColourId,            Colors::onSurface);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, Colors::accent.withAlpha (0.3f));

    setColour (juce::TextButton::buttonColourId,         Colors::surface);
    setColour (juce::TextButton::buttonOnColourId,       Colors::accent);
    setColour (juce::TextButton::textColourOffId,        Colors::onSurface);
    setColour (juce::TextButton::textColourOnId,         Colors::background);

    setColour (juce::TooltipWindow::backgroundColourId,  Colors::surface);
    setColour (juce::TooltipWindow::textColourId,        Colors::onSurface);
}

void WorldizerLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                             float sliderPos, float rotaryStartAngle,
                                             float rotaryEndAngle, juce::Slider&)
{
    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (4.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const float thickness = juce::jmax (3.0f, radius * 0.16f);
    const float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

    const float arcRadius = radius - thickness * 0.5f;

    // Track
    juce::Path track;
    track.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f,
                         rotaryStartAngle, rotaryEndAngle, true);
    g.setColour (Colors::surface);
    g.strokePath (track, juce::PathStrokeType (thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Value arc
    juce::Path value;
    value.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f,
                         rotaryStartAngle, angle, true);
    g.setColour (Colors::accent);
    g.strokePath (value, juce::PathStrokeType (thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Pointer
    juce::Point<float> tip (centre.x + arcRadius * std::cos (angle - juce::MathConstants<float>::halfPi),
                            centre.y + arcRadius * std::sin (angle - juce::MathConstants<float>::halfPi));
    g.setColour (Colors::onSurface);
    g.fillEllipse (juce::Rectangle<float> (thickness, thickness).withCentre (tip));

    // Hub
    g.setColour (Colors::surface.brighter (0.1f));
    g.fillEllipse (juce::Rectangle<float> (arcRadius * 0.9f, arcRadius * 0.9f).withCentre (centre));
}
