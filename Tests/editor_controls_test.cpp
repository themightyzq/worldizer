/**
    EditorControlsTest -- every rotary in the plugin editor has the house control behaviour.

    Opens the real editor headlessly (no window server needed on macOS, the same way
    worldizer_ui_snapshot does) and walks its component tree. Every juce::Slider in it must be
    a zqsfx::ui::Dial (keyboard focus, focus ring, Shift+arrow fine step) and must return to its
    default on a double-click. The slider list is checked as a whole, so a ninth slider added
    later without the house type fails here instead of shipping mouse-only.

      * focus:   the Dial wants keyboard focus and draws a focus outline.
      * default: double-click return is on and holds the real default. The six parameter-bound
                 knobs are compared with the parameter's own default; XY Angle and Rotate are not
                 parameters, so they are compared with the scene model's defaults (MicArray's
                 90 degree capsule angle, MicNode's default facing -X = azimuth 180).
      * keys:    on the parameter-bound knobs, Up moves the value, Shift+Up never moves it further
                 than Up, and a real double-click puts the parameter back at its default.

    Before the house control was adopted the sliders were plain juce::Slider members, which
    refuse keyboard focus, so this test fails on that code. Returns 0 on pass / nonzero on
    fail (CTest); 77 (skipped) off macOS, where a headless editor needs a display server.
*/
#include <JuceHeader.h>
#include <cmath>
#include <iostream>
#include <vector>
#include "../Source/Plugin/PluginProcessor.h"
#include "../Source/Plugin/PluginEditor.h"
#include "../Source/Model/MicArray.h"
#include "../Source/Model/MicNode.h"

namespace
{
int failures = 0;

void check (bool condition, const juce::String& what)
{
    std::cout << (condition ? "  PASS: " : "  FAIL: ") << what << std::endl;
    if (! condition)
        ++failures;
}

void collectSliders (juce::Component& c, std::vector<juce::Slider*>& out)
{
    if (auto* s = dynamic_cast<juce::Slider*> (&c))
        out.push_back (s);

    // Includes hidden children: XY Angle and Rotate are shown only for some mic setups.
    for (auto* child : c.getChildren())
        collectSliders (*child, out);
}

bool near (double a, double b) { return std::abs (a - b) < 1.0e-4; }

struct Expected
{
    const char* title;
    const char* paramId;   // nullptr: not a plugin parameter, default comes from the scene model
};

// The user-facing sliders in the editor. Titles are the accessible names the editor sets.
const Expected kSliders[] = {
    { "Input Gain",  "inputGain" },
    { "Mix",         "mix" },
    { "Output Gain", "outputGain" },
    { "Drive",       "sourceDrive" },
    { "Noise",       "micNoise" },
    { "Bed",         "ambientLevel" },
    { "XY Angle",    nullptr },
    { "Rotate",      nullptr },
};
}

int main()
{
#if ! JUCE_MAC
    std::cout << "SKIP: the headless editor needs a window server off macOS\n";
    return 77;
#else
    juce::ScopedJuceInitialiser_GUI gui;

    // processor declared before editor: the editor is destroyed first.
    WorldizerAudioProcessor processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());

    if (editor == nullptr)
    {
        std::cout << "  FAIL: createEditor returned null\n";
        return 1;
    }

    std::vector<juce::Slider*> sliders;
    collectSliders (*editor, sliders);

    check (sliders.size() == std::size (kSliders),
           "the editor holds exactly " + juce::String ((int) std::size (kSliders)) + " sliders (found "
               + juce::String ((int) sliders.size()) + ")");

    // Defaults for the two sliders that are not parameters, taken from the model.
    const double modelXYAngle = (double) Worldizer::MicArray().getXYAngleDegrees();
    const auto facing = Worldizer::MicNode().getOrientation();
    double modelAzimuth = juce::radiansToDegrees (std::atan2 ((double) facing.y, (double) facing.x));
    if (modelAzimuth < 0.0) modelAzimuth += 360.0;

    for (const auto& e : kSliders)
    {
        std::cout << e.title << "\n";

        juce::Slider* s = nullptr;
        for (auto* candidate : sliders)
            if (candidate->getTitle() == e.title)
                s = candidate;

        check (s != nullptr, juce::String (e.title) + ": slider found by its accessible title");
        if (s == nullptr)
            continue;

        auto* dial = dynamic_cast<zqsfx::ui::Dial*> (s);
        check (dial != nullptr, "is a zqsfx::ui::Dial");
        check (s->getWantsKeyboardFocus(), "wants keyboard focus");
        check (s->hasFocusOutline(), "draws a focus outline");
        check (s->isDoubleClickReturnEnabled(), "double-click return is enabled");

        const double expectedDefault = e.paramId != nullptr
            ? (double) processor.apvts.getParameter (e.paramId)->convertFrom0to1 (
                  processor.apvts.getParameter (e.paramId)->getDefaultValue())
            : (juce::String (e.title) == "XY Angle" ? modelXYAngle : modelAzimuth);

        check (near (s->getDoubleClickReturnValue(), expectedDefault),
               "double-click value " + juce::String (s->getDoubleClickReturnValue(), 4)
                   + " equals the default " + juce::String (expectedDefault, 4));

        if (e.paramId == nullptr || dial == nullptr)
            continue;   // XY Angle / Rotate would re-render the scene; their wiring is checked above

        auto* param = processor.apvts.getParameter (e.paramId);
        const auto key = [&] (int code, juce::ModifierKeys mods) { s->keyPressed (juce::KeyPress (code, mods, 0)); };

        // Start away from both ends so Up and Down have room.
        s->setValue (s->getMinimum() + 0.4 * (s->getMaximum() - s->getMinimum()), juce::sendNotificationSync);

        const double v0 = s->getValue();
        key (juce::KeyPress::upKey, {});
        const double coarse = s->getValue() - v0;
        check (coarse > 0.0, "Up arrow raises the value");

        const double v1 = s->getValue();
        key (juce::KeyPress::upKey, juce::ModifierKeys::shiftModifier);
        const double fine = s->getValue() - v1;
        // These parameters are stepped (interval 0.1), so zqsfx_ui's Dial makes Shift+Up move one
        // interval, the finest step the control can take (a tenth would be snapped away). Shift+Up
        // must move, and never further than Up (equal up to the parameter's float storage, which
        // rounds a 0.1 step to 0.0999985 or 0.100002 near -60 dB).
        check (fine > 0.0, "Shift+Up moves the value");
        check (fine <= coarse + 0.01 * s->getInterval(), "Shift+Up never raises it by more than Up");

        // A real double-click: the argument is unused by Slider, any event will do.
        const auto now = juce::Time::getCurrentTime();
        juce::MouseEvent click (juce::Desktop::getInstance().getMainMouseSource(), {}, {}, 0.0f, 0.0f, 0.0f, 0.0f,
                                0.0f, s, s, now, {}, now, 2, false);
        s->mouseDoubleClick (click);
        check (near (s->getValue(), expectedDefault), "double-click returns the knob to its default");
        check (near (param->convertFrom0to1 (param->getValue()), expectedDefault),
               "the parameter follows the double-click");
    }

    std::cout << (failures == 0 ? "EditorControlsTest: PASS\n" : "EditorControlsTest: FAIL\n");
    return failures == 0 ? 0 : 1;
#endif
}
