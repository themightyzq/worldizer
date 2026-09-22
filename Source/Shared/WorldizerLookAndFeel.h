#pragma once

#include <JuceHeader.h>

// Only the Worldizer plugin target and worldizer_ui_snapshot link zqsfx::ui (CMakeLists.txt) —
// BakePresets also compiles Source/UI/RoomView2D.cpp (for renderSceneThumbnail, baking preset
// thumbnail PNGs headlessly) but must NOT pull in the house UI module's embedded font/knob
// binary data, per the product spec ("only the plugin target and your snapshot tool link
// zqsfx::ui"). __has_include reflects that directly: the header is only reachable on a
// translation unit compiled by a target whose include path actually has zqsfx_ui on it.
#if __has_include(<zqsfx_ui/zqsfx_ui.h>)
 #define WORLDIZER_HAS_ZQSFX_UI 1
 #include <zqsfx_ui/zqsfx_ui.h>
#else
 #define WORLDIZER_HAS_ZQSFX_UI 0
#endif

namespace Worldizer
{
/** The Worldizer palette (ZQ SFX house UI migration, zqsfx_ui v0.2.1). Names and call sites
    are unchanged from Slice 4 — this remains the single colour source for Worldizer's own
    code — but every value is now a house token so the plugin reads as the same family as the
    other ZQ SFX products. See docs/ZQSFX_UI_STYLE_GUIDE.md and docs/ui_migration_report.md. */
namespace Colors
{
    // Background hierarchy
    inline const juce::Colour background       { 0xff0a0b0c };   // == zqsfx::ui::colour::chassisMid
    inline const juce::Colour surface           { 0xff121416 };   // == zqsfx::ui::colour::panelBot
    inline const juce::Colour surfaceVariant    { 0xff1d2022 };   // == zqsfx::ui::colour::panelTop
    inline const juce::Colour outline           { 0xff22272a };   // == zqsfx::ui::colour::ruleTitle

    // Foreground hierarchy
    inline const juce::Colour onSurface        { 0xffc9d4d2 };   // == zqsfx::ui::colour::btnText
    inline const juce::Colour onSurfaceVariant { 0xff8fb3ae };   // == zqsfx::ui::colour::silkLabel
    inline const juce::Colour onSurfaceMuted   { 0xff7a9a94 };   // == zqsfx::ui::colour::silkCaption

    // Brand accent (the one house accent — orange means active/lit/focused, nothing else)
    inline const juce::Colour primary          { 0xffe8622a };   // == zqsfx::ui::colour::accent
    inline const juce::Colour primaryDim       { 0xff8a4a28 };   // == zqsfx::ui::colour::accentDim
    inline const juce::Colour primaryAlpha40   { 0x66e8622a };   // == zqsfx::ui::colour::accent @ 40% alpha

    // Room view (meaning-carrying — colour-blind-safe channels, style guide section 3;
    // Worldizer's house mapping: source comp.yellow, mic comp.sky, subtractive comp.purple)
    inline const juce::Colour roomBackground   { 0xff0a120c };   // == zqsfx::ui::colour::lcdScreenDark
    inline const juce::Colour brushOutline     { 0xffe2e5e8 };   // == zqsfx::ui::comp::white (additive geometry)
    inline const juce::Colour brushOutlineSubtractive { 0xffcc79a7 }; // == zqsfx::ui::comp::purple, drawn DASHED
    inline const juce::Colour brushFill        { 0x14e2e5e8 };   // == zqsfx::ui::comp::white @ 8% alpha
    inline const juce::Colour gridLine         { 0x803f7a4a };   // == zqsfx::ui::colour::lcdFaint2 @ 50% alpha
    inline const juce::Colour gridLineMajor    { 0xff3f7a4a };   // == zqsfx::ui::colour::lcdFaint2
    inline const juce::Colour sourceIcon       { 0xfff0e442 };   // == zqsfx::ui::comp::yellow (speaker glyph)
    inline const juce::Colour micIcon          { 0xff56b4e9 };   // == zqsfx::ui::comp::sky (mic-capsule glyph)
    inline const juce::Colour rayGuide         { 0x458fe89a };   // == zqsfx::ui::colour::lcdText @ 27% alpha

    // State
    inline const juce::Colour success          { 0xff8fe89a };   // == zqsfx::ui::colour::lcdText
    inline const juce::Colour warning          { 0xffd9a441 };   // == zqsfx::ui::colour::meterHot
    inline const juce::Colour error            { 0xffdd4433 };   // == zqsfx::ui::colour::warn
}

#if WORLDIZER_HAS_ZQSFX_UI
/**
    Worldizer look & feel: now a THIN SUBCLASS of zqsfx::ui::LookAndFeel (ZQ SFX house UI
    migration, docs/ZQSFX_UI_STYLE_GUIDE.md). The house LookAndFeel supplies rotary knobs
    (CC0 filmstrips, picked by dial size), combo boxes (LCD dropdowns), TextButtons (gradient
    face, accent hover/on), and slider text-box readouts (LCD glass + glow) automatically once
    this subclass leaves drawRotarySlider / drawComboBox / positionComboBoxText / drawLabel /
    drawButtonBackground / drawButtonText un-overridden. This subclass keeps only the overrides
    the house LookAndFeel has no equivalent for — toggle buttons, text editors, and the
    scrollbar — all restyled with house tokens: hard-edged rectangles, no rounded corners.
*/
class WorldizerLookAndFeel : public zqsfx::ui::LookAndFeel
{
public:
    WorldizerLookAndFeel();

    // House has no drawToggleButton override (its bound LitToggle/TextToggle components draw
    // themselves instead); Worldizer's EditorToolPalette snap toggle needs one. Restyled: hard
    // rectangle (no tick box, no rounded pill), `btn` gradient off-state, `accent` fill +
    // `accentInk` text on-state — matching zqsfx::ui::LookAndFeel::drawButtonBackground's own
    // on/off treatment.
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&,
                           bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

    // House has no TextEditor treatment. Worldizer's inspector numeric fields (vertex X/Y,
    // floor/ceiling height) and the Save-As dialog's text fields get the LCD phosphor-screen
    // look instead of a rounded fill, per the product spec ("the inspector's numeric readouts
    // use the LCD treatment").
    void fillTextEditorBackground (juce::Graphics&, int width, int height, juce::TextEditor&) override;
    void drawTextEditorOutline (juce::Graphics&, int width, int height, juce::TextEditor&) override;

    // House has no scrollbar treatment. Restyled to a hard-edged (no rounded ends) thumb in
    // house tokens instead of the old rounded-pill amber thumb.
    void drawScrollbar (juce::Graphics&, juce::ScrollBar&, int x, int y, int width, int height,
                        bool isScrollbarVertical, int thumbStartPosition, int thumbSize,
                        bool isMouseOver, bool isMouseDown) override;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WorldizerLookAndFeel)
};
#endif
} // namespace Worldizer

#if WORLDIZER_HAS_ZQSFX_UI
// Convenience alias so existing/editor code can use the short name.
using WorldizerLookAndFeel = Worldizer::WorldizerLookAndFeel;
#endif
