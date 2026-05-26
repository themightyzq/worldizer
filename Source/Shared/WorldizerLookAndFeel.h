#pragma once

#include <JuceHeader.h>

/**
    Worldizer's visual style. Minimal for Slice 2 — dark background, light text,
    amber accent knobs. The full color system and layout per
    JUCE_VST3_UI_UX_BEST_PRACTICES.md land with RoomView2D in Slice 4.
*/
class WorldizerLookAndFeel : public juce::LookAndFeel_V4
{
public:
    WorldizerLookAndFeel();

    /** Semantic color palette (Slice 2 minimal subset). */
    struct Colors
    {
        static const juce::Colour background;   // window background
        static const juce::Colour surface;      // panels / track
        static const juce::Colour onSurface;    // primary text
        static const juce::Colour outline;      // dividers, muted text
        static const juce::Colour accent;       // primary accent (warm amber)
    };

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle,
                           float rotaryEndAngle, juce::Slider&) override;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WorldizerLookAndFeel)
};
