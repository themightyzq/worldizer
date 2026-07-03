#pragma once

#include <JuceHeader.h>
#include "../Model/Scene.h"
#include "../Shared/Vec3.h"
#include "DrawingState.h"
#include "InspectorPanel.h"
#include "EditorToolPalette.h"

namespace Worldizer
{
/** Renders a Scene as a top-down image (no component needed) — for thumbnails. */
juce::Image renderSceneThumbnail (const Scene& scene, int widthPx, int heightPx);

/**
    Top-down 2D view of a worldizing scene. Two modes:

    **Browse mode (default)** — source / mic icons are interactive (drag to move,
    drag arrow to rotate); the sector geometry is rendered read-only.

    **Edit mode (Slice 6a)** — the three editor tools (Select / Draw / Delete) act on
    the user-authored `SectorGeometry`. Drawing places vertices one click at a time
    (click the first vertex to close the sector); Select picks vertices/walls/sectors
    and drags vertices; Delete removes the element under the cursor. Source / mic
    interaction continues to work alongside.

    Edits mutate the held `Scene` and fire `onSceneEdited` (finalized=false during a
    drag = preview render; finalized=true on release / a committed click = full).
*/
class RoomView2D : public juce::Component
{
public:
    RoomView2D();
    ~RoomView2D() override;

    void setScene (const Scene& newScene);
    const Scene& getScene() const noexcept { return scene; }

    // === Browse-mode interaction ===
    std::function<void (const Scene& scene, bool finalized)> onSceneEdited;
    std::function<void (int micIndex)> onMicSelected;
    std::function<void()>              onResetPositions;   // double-click a dot in browse mode
    int  getSelectedMic() const noexcept { return selectedMic; }
    void setSelectedMic (int idx)        { selectedMic = juce::jlimit (0, 1, idx); repaint(); }

    // === Edit-mode state (Slice 6a) ===
    void setEditMode (bool on);
    bool isEditMode() const noexcept { return editMode; }

    void setTool (EditorToolPalette::Tool t);
    EditorToolPalette::Tool getTool() const noexcept { return currentTool; }

    void setSnapToGrid (bool on)         { snapToGrid = on; repaint(); }
    bool isSnapToGrid() const noexcept   { return snapToGrid; }

    void setSelection (EditorSelection sel);
    EditorSelection getSelection() const noexcept { return selection; }

    /** Cancel an in-progress drawing (Esc key handler dispatches here). */
    void cancelDrawing();

    /** Editor-side callbacks. */
    std::function<void (EditorSelection)>      onSelectionChanged;
    std::function<void (const Sector& built)>  onSectorCreated;        // user closed the drawing loop
    std::function<void()>                       onSectorRejected;       // closed loop was self-intersecting / too small
    std::function<void (EditorSelection)>      onDeleteRequested;      // Delete tool / key

    /** Renders the current scene to an image (no selection/hover state). */
    juce::Image renderToImage (int widthPx, int heightPx) const;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

    // Browse-mode hit-test targets.
    enum class Target { None, Source, Mic0, Mic1, ArrayBody, Mic0Arrow, Mic1Arrow, ArrayArrowL, ArrayArrowR };

private:
    Scene  scene;
    Target activeDrag = Target::None;
    Target hovered    = Target::None;
    int    selectedMic = 0;

    bool showDirectPath = false;
    juce::TextButton directPathToggle { "Path" };

    // Edit mode.
    bool editMode = false;
    EditorToolPalette::Tool currentTool = EditorToolPalette::Tool::Select;
    bool snapToGrid = true;
    DrawingState    drawing;
    EditorSelection selection;
    int draggingVertex = -1;
    juce::Point<float> cursorScenePos;  // for the rubber-band line during Draw

    Target pickTarget (juce::Point<float> p) const;
    EditorSelection pickEditElement (juce::Point<float> screenPos, bool isDeleteMode) const;
    Vertex snapVertex (Vertex v, bool snapModifierDown) const noexcept;

    void fireEdited (bool finalized);
    void fireSelectionChanged();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RoomView2D)
};
} // namespace Worldizer
