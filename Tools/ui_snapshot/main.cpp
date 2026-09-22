// worldizer_ui_snapshot: render the real plugin editor headlessly to a PNG.
//
//   worldizer_ui_snapshot <out.png> [scale] [width height]   (scale defaults to 2.0;
//                                                              width/height default to the
//                                                              editor's own default size,
//                                                              Constants::kDefaultWindowWidth/
//                                                              Height -- pass
//                                                              kMinWindowWidth kMinWindowHeight
//                                                              to check the minimum resize floor)
//
// The look-and-feel regression gate for the ZQ SFX house-UI migration (see
// docs/ZQSFX_UI_STYLE_GUIDE.md and docs/ui_migration_report.md): render before a UI change,
// render after, compare. Mirrors LFlOw's lflow_ui_snapshot (tools/ui_snapshot/main.cpp), linking
// against Worldizer's own shared-code CMake target (the "pamplejuce pattern") instead of
// recompiling the plugin sources a second time -- see CMakeLists.txt for the target wiring.
//
// Determinism: nothing here ever pumps JUCE's message loop (no runDispatchLoop), so the
// editor's 10 Hz juce::Timer (WorldizerAudioProcessorEditor::timerCallback, driving the
// rendering indicator / character-picker sync / out-of-band scene refresh) never actually
// fires -- JUCE dispatches timer callbacks through the message queue, not directly from its
// background timer thread. The snapshot is taken immediately after construction, before any
// timer tick, which is what makes two successive renders of unchanged code byte-identical.
//
// The editor is destroyed before the processor it references (unique_ptr destruction order:
// `editor` is declared after `processor`, so it is destroyed FIRST when main() returns).

#include "../../Source/Plugin/PluginProcessor.h"
#include "../../Source/Plugin/PluginEditor.h"
#include <iostream>

int main (int argc, char** argv)
{
    if (argc < 2)
    {
        std::cerr << "usage: worldizer_ui_snapshot <out.png> [scale] [width height]\n";
        return 2;
    }

    juce::ScopedJuceInitialiser_GUI gui;
    const juce::File out = juce::File::getCurrentWorkingDirectory().getChildFile (juce::String (argv[1]));
    const float scale = argc > 2 ? juce::String (argv[2]).getFloatValue() : 2.0f;

    // processor declared before editor: C++ destroys locals in reverse declaration order, so
    // the editor is always torn down before the processor it references (spec requirement).
    WorldizerAudioProcessor processor;
    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    if (editor == nullptr)
    {
        std::cerr << "createEditor returned null\n";
        return 1;
    }

    if (argc > 4)
    {
        const int w = juce::String (argv[3]).getIntValue();
        const int h = juce::String (argv[4]).getIntValue();
        editor->setSize (w, h); // within setResizeLimits(kMinWindowWidth/Height, kMax...)
    }

    // Rendered immediately after construction (and any explicit resize above) -- before any
    // dispatch loop runs and before the editor's startTimerHz(10) can ever fire a callback.
    const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, scale);

    out.getParentDirectory().createDirectory();
    out.deleteFile();
    juce::FileOutputStream stream (out);
    juce::PNGImageFormat png;
    if (! stream.openedOk() || ! png.writeImageToStream (image, stream))
    {
        std::cerr << "could not write " << out.getFullPathName() << "\n";
        return 1;
    }

    std::cout << out.getFullPathName() << "  " << image.getWidth() << "x" << image.getHeight() << "\n";
    return 0;
}
