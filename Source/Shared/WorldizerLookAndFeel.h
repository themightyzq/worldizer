#pragma once

#include <JuceHeader.h>

/**
    Worldizer's visual style. Analogous to HyperPrismLookAndFeel in ZQ's other
    projects (see JUCE_VST3_UI_UX_BEST_PRACTICES.md). This is a stub for Slice 0;
    Slice 4 fills in the full color system, knob drawing, and control styling.

    The semantic color categories (Dynamics / Timing / Modulation / Frequency /
    Output) carry over from the best-practices guide. Worldizer's primary accent
    is TBD — likely a warm amber / muted gold to distinguish it from the
    HyperPrism cyan palette. The placeholder below is not final.
*/
class WorldizerLookAndFeel : public juce::LookAndFeel_V4
{
public:
    WorldizerLookAndFeel() = default;

    /** Semantic color system. Values are placeholders pending the Slice 4 palette. */
    struct Colors
    {
        static const juce::Colour background;   // window background
        static const juce::Colour surface;      // panels
        static const juce::Colour onSurface;    // primary text
        static const juce::Colour outline;      // dividers, muted text
        static const juce::Colour accent;       // primary accent (warm amber, TBD)
    };

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WorldizerLookAndFeel)
};
