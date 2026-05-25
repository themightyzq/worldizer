#pragma once

#include <JuceHeader.h>
#include "../Model/Scene.h"

namespace Worldizer
{
/**
    Top-down 2D view of the current scene. Paints brushes as outlines and the
    source/mic as draggable icons.

    - Browse mode: drag source/mic; each move triggers a preview ray-trace and an
      IR crossfade (full-quality on release).
    - Edit mode: add / move / resize / delete brushes and assign materials.

    Slice 4 implements browse-mode painting and dragging; Slice 7 adds edit mode.
*/
class RoomView2D : public juce::Component
{
public:
    RoomView2D() = default;

    void paint (juce::Graphics&) override;
    void resized() override;

    /** Point the view at a scene to render (non-owning). */
    void setScene (const Scene* scene);

    /** Toggle between browse and edit interaction modes. */
    void setEditMode (bool shouldEdit);

private:
    const Scene* scene = nullptr;
    bool editMode = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RoomView2D)
};
} // namespace Worldizer
