#pragma once

#include <JuceHeader.h>

namespace Worldizer
{
/** The committed Worldizer palette (Slice 4). Dark neutral + amber accent. */
namespace Colors
{
    // Background hierarchy
    inline const juce::Colour background       { 0xff1a1a1a };
    inline const juce::Colour surface          { 0xff242424 };
    inline const juce::Colour surfaceVariant   { 0xff2e2e2e };
    inline const juce::Colour outline          { 0xff3a3a3a };

    // Foreground hierarchy
    inline const juce::Colour onSurface        { 0xffe8e8e8 };
    inline const juce::Colour onSurfaceVariant { 0xff9a9a9a };
    inline const juce::Colour onSurfaceMuted   { 0xff6a6a6a };

    // Brand accent (amber)
    inline const juce::Colour primary          { 0xffffab00 };
    inline const juce::Colour primaryDim       { 0xffb37800 };
    inline const juce::Colour primaryAlpha40   { 0x66ffab00 };

    // Room view
    inline const juce::Colour roomBackground   { 0xff141414 };
    inline const juce::Colour brushOutline     { 0xffffab00 };
    inline const juce::Colour brushOutlineSubtractive { 0xffff5555 };
    inline const juce::Colour brushFill        { 0x14ffab00 };
    inline const juce::Colour gridLine         { 0xff2a2a2a };
    inline const juce::Colour gridLineMajor    { 0xff343434 };
    inline const juce::Colour sourceIcon       { 0xffffab00 };
    inline const juce::Colour micIcon          { 0xff00d9ff };
    inline const juce::Colour rayGuide         { 0x44ffab00 };

    // State
    inline const juce::Colour success          { 0xff00ff41 };
    inline const juce::Colour warning          { 0xffffb700 };
    inline const juce::Colour error            { 0xffff5555 };
}

/** Worldizer look & feel: amber knobs, outline/filled buttons, dark UI. */
class WorldizerLookAndFeel : public juce::LookAndFeel_V4
{
public:
    WorldizerLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle,
                           float rotaryEndAngle, juce::Slider&) override;

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

    void drawButtonText (juce::Graphics&, juce::TextButton&,
                         bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;

    void fillTextEditorBackground (juce::Graphics&, int width, int height, juce::TextEditor&) override;
    void drawTextEditorOutline (juce::Graphics&, int width, int height, juce::TextEditor&) override;

    void drawScrollbar (juce::Graphics&, juce::ScrollBar&, int x, int y, int width, int height,
                        bool isScrollbarVertical, int thumbStartPosition, int thumbSize,
                        bool isMouseOver, bool isMouseDown) override;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WorldizerLookAndFeel)
};
} // namespace Worldizer

// Convenience alias so existing/editor code can use the short name.
using WorldizerLookAndFeel = Worldizer::WorldizerLookAndFeel;
