#pragma once

#include <JuceHeader.h>
#include "../Model/Scene.h"
#include "../Shared/Vec3.h"

namespace Worldizer
{
/** Renders a Scene as a top-down image (no component needed) — for thumbnails. */
juce::Image renderSceneThumbnail (const Scene& scene, int widthPx, int heightPx);

/**
    Top-down 2D view of a worldizing scene. Draws geometry as outlines plus source
    and mic icons, and lets the user drag source/mic in the horizontal plane (Z
    fixed). Browse mode only for Slice 4; brush editing is Slice 7.
*/
class RoomView2D : public juce::Component
{
public:
    enum class Target { None, Source, Mic };

    RoomView2D();
    ~RoomView2D() override;

    void setScene (const Scene& newScene);
    const Scene& getScene() const noexcept { return scene; }

    /** Fired continuously during a drag (use for low-quality preview rendering). */
    std::function<void (Vec3 sourcePos, Vec3 micPos)> onPositionsChanged;
    /** Fired on drag release (use for full-quality rendering). */
    std::function<void (Vec3 sourcePos, Vec3 micPos)> onPositionsFinalized;

    /** Renders the current scene to an image (no selection/hover state). */
    juce::Image renderToImage (int widthPx, int heightPx) const;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

private:
    Scene  scene;
    Target activeDrag = Target::None;
    Target hovered    = Target::None;

    bool showDirectPath = false;
    juce::TextButton directPathToggle { "Path" };

    Target pickTarget (juce::Point<float> p) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RoomView2D)
};
} // namespace Worldizer
