#pragma once

#include <JuceHeader.h>

namespace wz
{
/**
    Categorised, searchable list of available presets with click-to-load (and
    audition-on-click). Loading a preset is instant — only rendered.wav and
    metadata.json are read; no ray tracing is triggered.

    Slice 4 implements the list, categories, and the load callback.
*/
class PresetBrowser : public juce::Component
{
public:
    PresetBrowser() = default;

    void paint (juce::Graphics&) override;
    void resized() override;

    /** Called when the user selects a preset bundle to load. */
    std::function<void (const juce::File& presetBundle)> onPresetChosen;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetBrowser)
};
} // namespace wz
