#include "InspectorPanel.h"
#include "../Shared/WorldizerLookAndFeel.h"
#include "../Model/MaterialResolver.h"
#include <cmath>

namespace Worldizer
{
namespace
{
    void labelStyle (juce::Label& l, float pt, juce::Colour c, juce::Justification j = juce::Justification::centredLeft)
    {
        l.setColour (juce::Label::textColourId, c);
        l.setFont (juce::Font (juce::FontOptions (pt)));
        l.setJustificationType (j);
    }

    // Numeric readouts get the LCD treatment (product spec): the field's own paint comes from
    // WorldizerLookAndFeel::fillTextEditorBackground/drawTextEditorOutline (phosphor screen,
    // house has no TextEditor equivalent); this only sets the text/caret/selection colours,
    // which those paint hooks don't touch.
    void textFieldStyle (juce::TextEditor& t)
    {
        t.setColour (juce::TextEditor::textColourId,       zqsfx::ui::colour::lcdText);
        t.setColour (juce::TextEditor::highlightColourId,  zqsfx::ui::colour::accent.withAlpha (0.35f));
        t.setColour (juce::TextEditor::highlightedTextColourId, zqsfx::ui::colour::accentInk);
        t.setSelectAllWhenFocused (true);
        t.setInputRestrictions (16, "-0123456789.");
    }

    // NOTE: a `comboStyle` helper used to set ComboBox::backgroundColourId/textColourId/
    // outlineColourId/arrowColourId here. Removed as dead code: the house LookAndFeel's
    // drawComboBox/positionComboBoxText always paint the LCD-screen tokens directly and never
    // consult these component colour IDs (see PluginEditor.cpp's PopupMenu/styleCombo removal
    // notes for the same finding), so these calls never had any visible effect once
    // WorldizerLookAndFeel became a zqsfx::ui::LookAndFeel subclass.
}

InspectorPanel::InspectorPanel()
{
    labelStyle (titleLabel, 12.0f, Colors::primary);
    titleLabel.setText ("INSPECTOR", juce::dontSendNotification);
    titleLabel.setFont (juce::Font (juce::FontOptions (12.0f).withStyle ("Bold")));
    addAndMakeVisible (titleLabel);

    labelStyle (subtitleLabel, 11.0f, Colors::onSurfaceVariant);
    subtitleLabel.setFont (juce::Font (juce::FontOptions (11.0f).withStyle ("Bold")));
    addAndMakeVisible (subtitleLabel);

    labelStyle (statusLabel, 11.0f, Colors::onSurfaceMuted);
    addAndMakeVisible (statusLabel);

    // Vertex.
    labelStyle (vertexXLabel, 11.0f, Colors::onSurfaceVariant); vertexXLabel.setText ("X (m)", juce::dontSendNotification); addChildComponent (vertexXLabel);
    labelStyle (vertexYLabel, 11.0f, Colors::onSurfaceVariant); vertexYLabel.setText ("Y (m)", juce::dontSendNotification); addChildComponent (vertexYLabel);
    labelStyle (vertexInfoLabel, 10.0f, Colors::onSurfaceMuted); addChildComponent (vertexInfoLabel);
    textFieldStyle (vertexXEditor); vertexXEditor.setTitle ("Vertex X"); vertexXEditor.setDescription ("Vertex X position, metres."); addChildComponent (vertexXEditor);
    textFieldStyle (vertexYEditor); vertexYEditor.setTitle ("Vertex Y"); vertexYEditor.setDescription ("Vertex Y position, metres."); addChildComponent (vertexYEditor);
    auto commitVertex = [this]
    {
        if (updatingFromScene) return;
        if (onVertexMoved)
            onVertexMoved (selection.index, vertexXEditor.getText().getFloatValue(), vertexYEditor.getText().getFloatValue());
    };
    vertexXEditor.onReturnKey = commitVertex; vertexXEditor.onFocusLost = commitVertex;
    vertexYEditor.onReturnKey = commitVertex; vertexYEditor.onFocusLost = commitVertex;

    // LineDef.
    labelStyle (lineDefLengthLabel,   11.0f, Colors::onSurfaceVariant); addChildComponent (lineDefLengthLabel);
    labelStyle (lineDefMaterialLabel, 11.0f, Colors::onSurfaceVariant); lineDefMaterialLabel.setText ("Material", juce::dontSendNotification); addChildComponent (lineDefMaterialLabel);
    rebuildMaterialCombo (lineDefMaterialCombo);
    lineDefMaterialCombo.setTitle ("Wall Material");
    lineDefMaterialCombo.setDescription ("Acoustic material of the selected wall.");
    addChildComponent (lineDefMaterialCombo);
    lineDefMaterialCombo.onChange = [this]
    {
        if (updatingFromScene) return;
        if (onLineDefMaterialChanged)
            onLineDefMaterialChanged (selection.index, lineDefMaterialCombo.getText());
    };

    // Sector.
    labelStyle (sectorFloorHLabel,     11.0f, Colors::onSurfaceVariant); sectorFloorHLabel.setText ("Floor height (m)", juce::dontSendNotification); addChildComponent (sectorFloorHLabel);
    labelStyle (sectorCeilingHLabel,   11.0f, Colors::onSurfaceVariant); sectorCeilingHLabel.setText ("Ceiling height (m)", juce::dontSendNotification); addChildComponent (sectorCeilingHLabel);
    labelStyle (sectorFloorMatLabel,   11.0f, Colors::onSurfaceVariant); sectorFloorMatLabel.setText ("Floor material", juce::dontSendNotification); addChildComponent (sectorFloorMatLabel);
    labelStyle (sectorCeilingMatLabel, 11.0f, Colors::onSurfaceVariant); sectorCeilingMatLabel.setText ("Ceiling material", juce::dontSendNotification); addChildComponent (sectorCeilingMatLabel);
    labelStyle (sectorStatsLabel,      10.0f, Colors::onSurfaceMuted); addChildComponent (sectorStatsLabel);

    textFieldStyle (sectorFloorHEditor);   sectorFloorHEditor.setTitle ("Floor Height");     sectorFloorHEditor.setDescription ("Sector floor height, metres.");   addChildComponent (sectorFloorHEditor);
    textFieldStyle (sectorCeilingHEditor); sectorCeilingHEditor.setTitle ("Ceiling Height"); sectorCeilingHEditor.setDescription ("Sector ceiling height, metres."); addChildComponent (sectorCeilingHEditor);
    sectorFloorHEditor.onReturnKey = [this] { if (! updatingFromScene && onSectorFloorHeightChanged) onSectorFloorHeightChanged (sectorFloorHEditor.getText().getFloatValue()); };
    sectorFloorHEditor.onFocusLost = sectorFloorHEditor.onReturnKey;
    sectorCeilingHEditor.onReturnKey = [this] { if (! updatingFromScene && onSectorCeilingHeightChanged) onSectorCeilingHeightChanged (sectorCeilingHEditor.getText().getFloatValue()); };
    sectorCeilingHEditor.onFocusLost = sectorCeilingHEditor.onReturnKey;

    rebuildMaterialCombo (sectorFloorMatCombo);
    sectorFloorMatCombo.setTitle ("Floor Material");
    sectorFloorMatCombo.setDescription ("Acoustic material of the sector floor.");
    addChildComponent (sectorFloorMatCombo);
    rebuildMaterialCombo (sectorCeilingMatCombo);
    sectorCeilingMatCombo.setTitle ("Ceiling Material");
    sectorCeilingMatCombo.setDescription ("Acoustic material of the sector ceiling.");
    addChildComponent (sectorCeilingMatCombo);
    sectorFloorMatCombo.onChange = [this]   { if (! updatingFromScene && onSectorFloorMaterialChanged)   onSectorFloorMaterialChanged   (sectorFloorMatCombo.getText()); };
    sectorCeilingMatCombo.onChange = [this] { if (! updatingFromScene && onSectorCeilingMaterialChanged) onSectorCeilingMaterialChanged (sectorCeilingMatCombo.getText()); };

    showFieldsForKind (EditorSelection::Kind::None);
}

void InspectorPanel::rebuildMaterialCombo (juce::ComboBox& c)
{
    c.clear (juce::dontSendNotification);
    int id = 1;
    for (const auto& n : MaterialResolver::getKnownNames())
        c.addItem (n, id++);
}

void InspectorPanel::setScene (const Scene& s)
{
    scene = s;
    updateFieldsFromScene();
}

void InspectorPanel::setSelection (EditorSelection sel)
{
    selection = sel;
    // Bound-check against the current scene's sector 0.
    if (! scene.getSectorGeometry().sectors.empty())
    {
        const auto& sector = scene.getSectorGeometry().sectors[0];
        if (sel.kind == EditorSelection::Kind::Vertex
            && (sel.index < 0 || (size_t) sel.index >= sector.vertices.size()))
            selection = { EditorSelection::Kind::None, -1 };
        if (sel.kind == EditorSelection::Kind::LineDef
            && (sel.index < 0 || (size_t) sel.index >= sector.lineDefs.size()))
            selection = { EditorSelection::Kind::None, -1 };
    }
    else if (sel.kind != EditorSelection::Kind::None)
    {
        selection = { EditorSelection::Kind::None, -1 };
    }

    showFieldsForKind (selection.kind);
    updateFieldsFromScene();
    resized();
    repaint();
}

void InspectorPanel::showFieldsForKind (EditorSelection::Kind k)
{
    const bool vtx = k == EditorSelection::Kind::Vertex;
    const bool ldf = k == EditorSelection::Kind::LineDef;
    const bool sec = k == EditorSelection::Kind::Sector;

    vertexXLabel.setVisible (vtx); vertexYLabel.setVisible (vtx);
    vertexXEditor.setVisible (vtx); vertexYEditor.setVisible (vtx);
    vertexInfoLabel.setVisible (vtx);

    lineDefLengthLabel.setVisible (ldf); lineDefMaterialLabel.setVisible (ldf); lineDefMaterialCombo.setVisible (ldf);

    sectorFloorHLabel.setVisible (sec); sectorCeilingHLabel.setVisible (sec);
    sectorFloorMatLabel.setVisible (sec); sectorCeilingMatLabel.setVisible (sec);
    sectorFloorHEditor.setVisible (sec); sectorCeilingHEditor.setVisible (sec);
    sectorFloorMatCombo.setVisible (sec); sectorCeilingMatCombo.setVisible (sec);
    sectorStatsLabel.setVisible (sec);

    statusLabel.setVisible (k == EditorSelection::Kind::None);
    if (k == EditorSelection::Kind::None)
        statusLabel.setText ("Nothing selected.\n\nDraw a new sector or click an element to edit.",
                             juce::dontSendNotification);

    switch (k)
    {
        case EditorSelection::Kind::Vertex:  subtitleLabel.setText ("VERTEX",  juce::dontSendNotification); break;
        case EditorSelection::Kind::LineDef: subtitleLabel.setText ("WALL",    juce::dontSendNotification); break;
        case EditorSelection::Kind::Sector:  subtitleLabel.setText ("SECTOR",  juce::dontSendNotification); break;
        case EditorSelection::Kind::None:    subtitleLabel.setText ("",        juce::dontSendNotification); break;
    }
}

void InspectorPanel::updateFieldsFromScene()
{
    if (scene.getSectorGeometry().sectors.empty())
        return;
    const auto& sector = scene.getSectorGeometry().sectors[0];

    const juce::ScopedValueSetter<bool> guard (updatingFromScene, true);

    switch (selection.kind)
    {
        case EditorSelection::Kind::Vertex:
            if (selection.index >= 0 && (size_t) selection.index < sector.vertices.size())
            {
                const auto& v = sector.vertices[(size_t) selection.index];
                vertexXEditor.setText (juce::String (v.x, 2), juce::dontSendNotification);
                vertexYEditor.setText (juce::String (v.y, 2), juce::dontSendNotification);

                int connected = 0;
                for (const auto& ld : sector.lineDefs)
                    if (ld.v1Index == selection.index || ld.v2Index == selection.index)
                        ++connected;
                vertexInfoLabel.setText ("Connected walls: " + juce::String (connected), juce::dontSendNotification);
            }
            break;

        case EditorSelection::Kind::LineDef:
            if (selection.index >= 0 && (size_t) selection.index < sector.lineDefs.size())
            {
                const auto& ld = sector.lineDefs[(size_t) selection.index];
                if (ld.v1Index >= 0 && ld.v2Index >= 0
                    && (size_t) ld.v1Index < sector.vertices.size()
                    && (size_t) ld.v2Index < sector.vertices.size())
                {
                    const auto p = sector.vertices[(size_t) ld.v1Index];
                    const auto q = sector.vertices[(size_t) ld.v2Index];
                    const float len = std::sqrt ((p.x - q.x) * (p.x - q.x) + (p.y - q.y) * (p.y - q.y));
                    lineDefLengthLabel.setText ("Length: " + juce::String (len, 2) + " m", juce::dontSendNotification);
                }
                lineDefMaterialCombo.setText (ld.frontMaterial, juce::dontSendNotification);
            }
            break;

        case EditorSelection::Kind::Sector:
            sectorFloorHEditor.setText (juce::String (sector.floorHeight, 2), juce::dontSendNotification);
            sectorCeilingHEditor.setText (juce::String (sector.ceilingHeight, 2), juce::dontSendNotification);
            sectorFloorMatCombo.setText (sector.floorMaterial, juce::dontSendNotification);
            sectorCeilingMatCombo.setText (sector.ceilingMaterial, juce::dontSendNotification);
            sectorStatsLabel.setText ("Walls: " + juce::String ((int) sector.getNumWalls())
                                      + "    Area: " + juce::String (std::abs (sector.getSignedArea()), 1) + juce::String::fromUTF8 (" m\xc2\xb2"),
                                      juce::dontSendNotification);
            break;

        case EditorSelection::Kind::None: break;
    }
}

void InspectorPanel::paint (juce::Graphics& g)
{
    // Titled-panel chrome (zqsfx::ui::Panel's drawing, reproduced here rather than wrapping
    // the component in a Panel, so the existing child layout is untouched): gradient face,
    // hard border, faint top inner highlight. The INSPECTOR/subtitle text stays as Labels
    // (titleLabel/subtitleLabel), already positioned by resized().
    auto r = getLocalBounds().toFloat();
    g.setGradientFill (zqsfx::ui::gradients::panel (r));
    g.fillRect (r);
    g.setColour (juce::Colours::white.withAlpha (0.04f));
    g.fillRect (r.withHeight (1.0f).translated (0.0f, 1.0f));
    g.setColour (zqsfx::ui::colour::panelBorder);
    g.drawRect (r, 1.0f);
}

void InspectorPanel::resized()
{
    auto r = getLocalBounds().reduced (10, 8);
    titleLabel.setBounds (r.removeFromTop (18));
    r.removeFromTop (4);
    subtitleLabel.setBounds (r.removeFromTop (16));
    r.removeFromTop (8);

    auto rowLabelEditor = [&r] (juce::Label& l, juce::Component& e, int eW = 80)
    {
        auto row = r.removeFromTop (24);
        l.setBounds (row.removeFromLeft (row.getWidth() - eW - 8).reduced (0, 2));
        e.setBounds (row.removeFromLeft (eW));
        r.removeFromTop (4);
    };

    switch (selection.kind)
    {
        case EditorSelection::Kind::Vertex:
            rowLabelEditor (vertexXLabel, vertexXEditor);
            rowLabelEditor (vertexYLabel, vertexYEditor);
            r.removeFromTop (4);
            vertexInfoLabel.setBounds (r.removeFromTop (16));
            break;

        case EditorSelection::Kind::LineDef:
            lineDefLengthLabel.setBounds (r.removeFromTop (18));
            r.removeFromTop (6);
            rowLabelEditor (lineDefMaterialLabel, lineDefMaterialCombo, 130);
            break;

        case EditorSelection::Kind::Sector:
            rowLabelEditor (sectorFloorHLabel,     sectorFloorHEditor);
            rowLabelEditor (sectorCeilingHLabel,   sectorCeilingHEditor);
            r.removeFromTop (4);
            rowLabelEditor (sectorFloorMatLabel,   sectorFloorMatCombo,   130);
            rowLabelEditor (sectorCeilingMatLabel, sectorCeilingMatCombo, 130);
            r.removeFromTop (8);
            sectorStatsLabel.setBounds (r.removeFromTop (18));
            break;

        case EditorSelection::Kind::None:
            statusLabel.setBounds (r);
            break;
    }
}
} // namespace Worldizer
