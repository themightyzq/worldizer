#pragma once

#include <JuceHeader.h>
#include "../Model/Scene.h"

namespace Worldizer
{
/** What the user has selected in the editor — drives the InspectorPanel's content. */
struct EditorSelection
{
    enum class Kind { None, Vertex, LineDef, Sector };
    Kind kind = Kind::None;
    int  index = -1;  // vertex index OR linedef index; sector index = 0 (Slice 6a is single-sector)
};

/**
    Right-side inspector for the geometry editor. Shows properties of the currently-
    selected element (vertex / wall / sector) and fires typed callbacks when the user
    commits an edit (Enter or focus-loss on a text field, change on a combo).

    Slice 6a is single-sector; the panel only ever inspects sector 0.
*/
class InspectorPanel : public juce::Component
{
public:
    InspectorPanel();

    /** Pull data from this scene (sector 0) to display in the fields. */
    void setScene (const Scene& scene);

    /** Switch which element's properties are shown. */
    void setSelection (EditorSelection sel);
    EditorSelection getSelection() const noexcept { return selection; }

    // === Edit callbacks (only fire from user actions, not setScene/setSelection) ===
    std::function<void (int /*vertexIdx*/, float x, float y)>        onVertexMoved;
    std::function<void (int /*lineDefIdx*/, juce::String material)>  onLineDefMaterialChanged;
    std::function<void (float floorH)>                               onSectorFloorHeightChanged;
    std::function<void (float ceilingH)>                             onSectorCeilingHeightChanged;
    std::function<void (juce::String material)>                      onSectorFloorMaterialChanged;
    std::function<void (juce::String material)>                      onSectorCeilingMaterialChanged;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    EditorSelection selection;
    Scene           scene;
    bool            updatingFromScene = false;  // re-entrancy guard

    juce::Label titleLabel, subtitleLabel, statusLabel;

    // Vertex fields
    juce::Label vertexXLabel, vertexYLabel, vertexInfoLabel;
    juce::TextEditor vertexXEditor, vertexYEditor;

    // LineDef fields
    juce::Label lineDefLengthLabel, lineDefMaterialLabel;
    juce::ComboBox lineDefMaterialCombo;

    // Sector fields
    juce::Label sectorFloorHLabel, sectorCeilingHLabel, sectorFloorMatLabel, sectorCeilingMatLabel, sectorStatsLabel;
    juce::TextEditor sectorFloorHEditor, sectorCeilingHEditor;
    juce::ComboBox sectorFloorMatCombo, sectorCeilingMatCombo;

    void rebuildMaterialCombo (juce::ComboBox& c);
    void updateFieldsFromScene();
    void showFieldsForKind (EditorSelection::Kind k);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (InspectorPanel)
};
} // namespace Worldizer
