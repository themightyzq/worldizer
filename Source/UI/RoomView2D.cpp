#include "RoomView2D.h"
#include "../Shared/WorldizerLookAndFeel.h"
#include <cmath>

namespace Worldizer
{
namespace
{
    constexpr float kArrowLenPx    = 24.0f;
    constexpr float kArrowHitR     = 9.0f;
    constexpr float kMicHitR       = 12.0f;
    constexpr float kSourceHitR    = 13.0f;
    constexpr float kVertexHitPx   = 8.0f;
    constexpr float kLineDefHitPx  = 5.0f;
    constexpr float kVertexDrawR   = 3.5f;
    constexpr float kGridSnapM     = 1.0f;

    // Scene<->view transform: X horizontal, Y vertical (scene +Y is screen-up).
    struct ViewXf
    {
        float ppm = 50.0f, sceneCx = 0.0f, sceneCy = 0.0f, areaCx = 0.0f, areaCy = 0.0f;

        juce::Point<float> toView (float x, float y) const
        {
            return { areaCx + (x - sceneCx) * ppm, areaCy - (y - sceneCy) * ppm };
        }
        juce::Point<float> toView (Vec3 v) const { return toView (v.x, v.y); }
        juce::Point<float> toView (Vertex v) const { return toView (v.x, v.y); }
        juce::Point<float> toScene (juce::Point<float> p) const
        {
            return { sceneCx + (p.x - areaCx) / ppm, sceneCy - (p.y - areaCy) / ppm };
        }
        juce::Point<float> viewDir (Vec3 d) const
        {
            juce::Point<float> v (d.x, -d.y);
            const float len = v.getDistanceFromOrigin();
            return len > 1.0e-6f ? v / len : juce::Point<float> (1.0f, 0.0f);
        }
    };

    Vec3 rotateZ (Vec3 v, float a) noexcept
    {
        const float c = std::cos (a), s = std::sin (a);
        return { v.x * c - v.y * s, v.x * s + v.y * c, v.z };
    }

    Vec3 sceneDirFromDrag (juce::Point<float> centreView, juce::Point<float> p) noexcept
    {
        Vec3 d { p.x - centreView.x, centreView.y - p.y, 0.0f };
        const auto n = d.normalised();
        return n.lengthSquared() > 0.0f ? n : Vec3 { -1.0f, 0.0f, 0.0f };
    }

    ViewXf computeXf (const Scene& scene, juce::Rectangle<float> area)
    {
        const auto b = scene.getBounds();
        float minX = b.first.x, maxX = b.second.x, minY = b.first.y, maxY = b.second.y;

        // Fold in sector geometry too (in case the manual-brushes bounds aren't enough,
        // e.g. an empty-brush scene with a brand-new user sector).
        if (! scene.getSectorGeometry().isEmpty())
        {
            const auto sb = scene.getSectorGeometry().getBounds();
            minX = juce::jmin (minX, sb.min.x); maxX = juce::jmax (maxX, sb.max.x);
            minY = juce::jmin (minY, sb.min.y); maxY = juce::jmax (maxY, sb.max.y);
        }

        if (maxX - minX < 0.5f || maxY - minY < 0.5f)
        {
            const auto s = scene.getSource().getPosition();
            minX = juce::jmin (minX, s.x); maxX = juce::jmax (maxX, s.x);
            minY = juce::jmin (minY, s.y); maxY = juce::jmax (maxY, s.y);
            const auto& arr = scene.getMicArray();
            for (int m = 0; m < arr.getNumMics(); ++m)
            {
                const auto mp = arr.getMic (m).getPosition();
                minX = juce::jmin (minX, mp.x); maxX = juce::jmax (maxX, mp.x);
                minY = juce::jmin (minY, mp.y); maxY = juce::jmax (maxY, mp.y);
            }
            minX -= 3.0f; maxX += 3.0f; minY -= 3.0f; maxY += 3.0f;
        }

        const float sceneW = juce::jmax (1.0f, maxX - minX);
        const float sceneH = juce::jmax (1.0f, maxY - minY);

        ViewXf xf;
        xf.ppm     = juce::jmin (area.getWidth() * 0.9f / sceneW, area.getHeight() * 0.9f / sceneH);
        xf.sceneCx = (minX + maxX) * 0.5f;
        xf.sceneCy = (minY + maxY) * 0.5f;
        xf.areaCx  = area.getCentreX();
        xf.areaCy  = area.getCentreY();
        return xf;
    }

    void drawIcon (juce::Graphics& g, juce::Point<float> c, float radius, juce::Colour fill,
                   juce::Colour ring, bool glow)
    {
        if (glow)
        {
            g.setColour (Colors::primaryAlpha40);
            g.drawEllipse (juce::Rectangle<float> (radius * 2 + 8, radius * 2 + 8).withCentre (c), 2.0f);
        }
        g.setColour (fill);
        g.fillEllipse (juce::Rectangle<float> (radius * 2, radius * 2).withCentre (c));
        if (! ring.isTransparent())
        {
            g.setColour (ring);
            g.drawEllipse (juce::Rectangle<float> (radius * 2, radius * 2).withCentre (c), 1.5f);
        }
    }

    juce::Point<float> drawArrow (juce::Graphics& g, const ViewXf& xf, juce::Point<float> centre,
                                  Vec3 dir, juce::Colour colour, bool highlight)
    {
        const auto vd  = xf.viewDir (dir);
        const auto tip = centre + vd * kArrowLenPx;

        g.setColour (highlight ? Colors::primary : colour);
        g.drawLine ({ centre, tip }, highlight ? 2.5f : 2.0f);

        const juce::Point<float> perp (-vd.y, vd.x);
        const auto base = tip - vd * 6.0f;
        juce::Path head;
        head.startNewSubPath (tip);
        head.lineTo (base + perp * 4.0f);
        head.lineTo (base - perp * 4.0f);
        head.closeSubPath();
        g.fillPath (head);
        return tip;
    }

    void drawLabel (juce::Graphics& g, juce::Point<float> at, const juce::String& text, juce::Colour c)
    {
        g.setColour (c);
        g.setFont (juce::Font (juce::FontOptions (11.0f).withStyle ("Bold")));
        g.drawText (text, juce::Rectangle<float> (at.x - 12.0f, at.y, 24.0f, 14.0f), juce::Justification::centred);
    }

    // Sector polygons (fill + outline). Shared by paint() and thumbnail rendering.
    void drawSectorGeometry (juce::Graphics& g, const Scene& scene, const ViewXf& xf,
                              EditorSelection sel, bool editMode)
    {
        const auto& sg = scene.getSectorGeometry();
        if (sg.isEmpty()) return;

        for (size_t s = 0; s < sg.sectors.size(); ++s)
        {
            const auto& sector = sg.sectors[s];
            if (sector.vertices.size() < 2) continue;

            // Filled polygon (translucent amber for any sector; brighter for the
            // selected sector in edit mode).
            juce::Path poly;
            const auto v0 = xf.toView (sector.vertices[0]);
            poly.startNewSubPath (v0);
            for (size_t i = 1; i < sector.vertices.size(); ++i)
                poly.lineTo (xf.toView (sector.vertices[i]));
            poly.closeSubPath();

            const bool sectorSelected = editMode && sel.kind == EditorSelection::Kind::Sector;
            g.setColour (sectorSelected ? Colors::primaryAlpha40 : Colors::brushFill);
            g.fillPath (poly);

            // Linedef outlines.
            for (size_t i = 0; i < sector.lineDefs.size(); ++i)
            {
                const auto& ld = sector.lineDefs[i];
                if (ld.v1Index < 0 || ld.v2Index < 0
                    || (size_t) ld.v1Index >= sector.vertices.size()
                    || (size_t) ld.v2Index >= sector.vertices.size())
                    continue;
                const auto p = xf.toView (sector.vertices[(size_t) ld.v1Index]);
                const auto q = xf.toView (sector.vertices[(size_t) ld.v2Index]);

                const bool ldSelected = editMode
                    && sel.kind == EditorSelection::Kind::LineDef && sel.index == (int) i;
                g.setColour (ldSelected ? Colors::primary : Colors::brushOutline);
                g.drawLine ({ p, q }, ldSelected ? 2.5f : 1.5f);
            }

            // Vertex squares (edit mode only — they're not informative in browse).
            if (editMode)
            {
                for (size_t i = 0; i < sector.vertices.size(); ++i)
                {
                    const auto p = xf.toView (sector.vertices[i]);
                    const bool vSel = sel.kind == EditorSelection::Kind::Vertex && sel.index == (int) i;
                    const float r = vSel ? kVertexDrawR + 2.0f : kVertexDrawR;
                    g.setColour (vSel ? Colors::primary : Colors::brushOutline);
                    g.fillRect (juce::Rectangle<float> (r * 2, r * 2).withCentre (p));
                }
            }
        }
    }

    void drawScene (juce::Graphics& g, const Scene& scene, juce::Rectangle<float> area,
                    bool drawGrid, bool drawIcons, bool drawDirect,
                    RoomView2D::Target hovered, RoomView2D::Target active, int selectedMic,
                    EditorSelection editSel, bool editMode, bool showReadout = false)
    {
        g.setColour (Colors::roomBackground);
        g.fillRect (area);

        const auto xf = computeXf (scene, area);

        if (drawGrid)
        {
            const auto tl = xf.toScene (area.getTopLeft());
            const auto br = xf.toScene (area.getBottomRight());
            const int minXm = (int) std::floor (juce::jmin (tl.x, br.x));
            const int maxXm = (int) std::ceil  (juce::jmax (tl.x, br.x));
            const int minYm = (int) std::floor (juce::jmin (tl.y, br.y));
            const int maxYm = (int) std::ceil  (juce::jmax (tl.y, br.y));

            if (maxXm - minXm <= 400 && maxYm - minYm <= 400)
            {
                for (int xm = minXm; xm <= maxXm; ++xm)
                {
                    const float px = xf.toView ((float) xm, 0.0f).x;
                    g.setColour (xm % 5 == 0 ? Colors::gridLineMajor : Colors::gridLine);
                    g.drawVerticalLine ((int) px, area.getY(), area.getBottom());
                }
                for (int ym = minYm; ym <= maxYm; ++ym)
                {
                    const float py = xf.toView (0.0f, (float) ym).y;
                    g.setColour (ym % 5 == 0 ? Colors::gridLineMajor : Colors::gridLine);
                    g.drawHorizontalLine ((int) py, area.getX(), area.getRight());
                }
            }
        }

        // Manual brushes (Test scenes / legacy presets).
        for (const auto& br : scene.getBrushes())
        {
            const auto a = xf.toView (br.getMin().x, br.getMin().y);
            const auto b = xf.toView (br.getMax().x, br.getMax().y);
            const juce::Rectangle<float> r (juce::jmin (a.x, b.x), juce::jmin (a.y, b.y),
                                            std::abs (b.x - a.x), std::abs (b.y - a.y));
            const bool subtractive = br.getKind() == Brush::Kind::Subtractive;
            g.setColour (Colors::brushFill);
            g.fillRect (r);
            g.setColour (subtractive ? Colors::brushOutlineSubtractive : Colors::brushOutline);
            g.drawRect (r, 1.5f);
        }

        // Sector geometry (user-authored, Slice 6a).
        drawSectorGeometry (g, scene, xf, editSel, editMode);

        const auto& arr   = scene.getMicArray();
        const auto  srcP  = xf.toView (scene.getSource().getPosition());
        const bool  big   = area.getWidth() > 200.0f;
        const auto  micRing = Colors::primary;

        if (drawDirect)
        {
            g.setColour (Colors::rayGuide);
            const float dashes[] = { 4.0f, 4.0f };
            juce::Line<float> line (srcP, xf.toView (arr.getCenterPosition()));
            g.drawDashedLine (line, dashes, 2, 1.0f);
        }

        if (drawIcons)
        {
            using Target = RoomView2D::Target;
            const bool shotgun = arr.getPattern() == MicPattern::Shotgun;

            switch (arr.getConfiguration())
            {
                case MicArray::Configuration::Single:
                {
                    const auto m = xf.toView (arr.getMic (0).getPosition());
                    if (shotgun)
                        drawArrow (g, xf, m, arr.getMic (0).getOrientation(), Colors::primaryDim,
                                   hovered == Target::Mic0Arrow || active == Target::Mic0Arrow);
                    drawIcon (g, m, 7.0f, Colors::micIcon, micRing,
                              hovered == Target::Mic0 || active == Target::Mic0);
                    if (big) drawLabel (g, { m.x, m.y + 8.0f }, "M", Colors::micIcon);
                    break;
                }

                case MicArray::Configuration::StereoXY:
                {
                    const auto c    = xf.toView (arr.getXYPosition());
                    const float half = juce::degreesToRadians (arr.getXYAngleDegrees() * 0.5f);
                    const auto dirL = rotateZ (arr.getXYOrientation(),  half);
                    const auto dirR = rotateZ (arr.getXYOrientation(), -half);
                    const auto tipL = drawArrow (g, xf, c, dirL, Colors::primaryDim,
                                                 hovered == Target::ArrayArrowL || active == Target::ArrayArrowL);
                    const auto tipR = drawArrow (g, xf, c, dirR, Colors::primaryDim,
                                                 hovered == Target::ArrayArrowR || active == Target::ArrayArrowR);
                    drawIcon (g, c, 9.0f, Colors::micIcon, micRing,
                              hovered == Target::ArrayBody || active == Target::ArrayBody);
                    if (big)
                    {
                        drawLabel (g, { tipL.x, tipL.y - 14.0f }, "L", Colors::micIcon);
                        drawLabel (g, { tipR.x, tipR.y - 14.0f }, "R", Colors::micIcon);
                    }
                    break;
                }

                case MicArray::Configuration::SpacedPair:
                {
                    const auto m0 = xf.toView (arr.getMic (0).getPosition());
                    const auto m1 = xf.toView (arr.getMic (1).getPosition());

                    g.setColour (Colors::onSurfaceMuted.withAlpha (0.5f));
                    g.drawLine ({ m0, m1 }, 1.0f);

                    if (shotgun)
                    {
                        drawArrow (g, xf, m0, arr.getMic (0).getOrientation(), Colors::primaryDim,
                                   hovered == Target::Mic0Arrow || active == Target::Mic0Arrow);
                        drawArrow (g, xf, m1, arr.getMic (1).getOrientation(), Colors::primaryDim,
                                   hovered == Target::Mic1Arrow || active == Target::Mic1Arrow);
                    }
                    drawIcon (g, m0, 7.0f, Colors::micIcon, micRing,
                              hovered == Target::Mic0 || active == Target::Mic0);
                    drawIcon (g, m1, 7.0f, Colors::micIcon, micRing,
                              hovered == Target::Mic1 || active == Target::Mic1);
                    if (selectedMic == 0 || selectedMic == 1)
                    {
                        g.setColour (Colors::onSurface);
                        g.drawEllipse (juce::Rectangle<float> (20.0f, 20.0f).withCentre (selectedMic == 0 ? m0 : m1), 1.5f);
                    }
                    if (big)
                    {
                        drawLabel (g, { m0.x, m0.y + 8.0f }, "L", Colors::micIcon);
                        drawLabel (g, { m1.x, m1.y + 8.0f }, "R", Colors::micIcon);
                    }
                    break;
                }
            }

            drawIcon (g, srcP, 8.0f, Colors::sourceIcon, juce::Colours::transparentBlack,
                      hovered == Target::Source || active == Target::Source);
            if (big) drawLabel (g, { srcP.x, srcP.y + 9.0f }, "S", Colors::sourceIcon);
        }

        // Scale + elevation chrome (unchanged).
        {
            const float y = area.getBottom() - 16.0f;
            const float x0 = area.getX() + 12.0f;
            g.setColour (Colors::onSurfaceMuted);
            g.drawLine (x0, y, x0 + xf.ppm, y, 1.5f);
            g.drawLine (x0, y - 3.0f, x0, y + 3.0f, 1.5f);
            g.drawLine (x0 + xf.ppm, y - 3.0f, x0 + xf.ppm, y + 3.0f, 1.5f);
            g.setFont (juce::Font (juce::FontOptions (10.0f)));
            g.drawText ("1 m", juce::Rectangle<float> (x0, y - 18.0f, xf.ppm, 14.0f), juce::Justification::centred);
        }
        if (drawIcons && showReadout)
        {
            // Distance is the headline cue — show it in metres, prominently, with
            // the (fixed) source/mic height as quiet secondary info.
            const float dist = (arr.getCenterPosition() - scene.getSource().getPosition()).length();
            auto row = area.removeFromBottom (18).reduced (8, 0);

            g.setColour (Colors::onSurfaceVariant);
            g.setFont (juce::Font (juce::FontOptions (9.5f)));
            g.drawText ("height " + juce::String (scene.getSource().getPosition().z, 1) + " m",
                        row, juce::Justification::centredLeft);

            g.setColour (Colors::primary);
            g.setFont (juce::Font (juce::FontOptions (12.0f).withStyle ("Bold")));
            g.drawText (juce::String (dist, dist < 10.0f ? 2 : 1) + " m  src \xe2\x86\x94 mic",
                        row, juce::Justification::centredRight);
        }
    }
}

//==============================================================================
juce::Image renderSceneThumbnail (const Scene& scene, int widthPx, int heightPx)
{
    juce::Image img (juce::Image::ARGB, juce::jmax (1, widthPx), juce::jmax (1, heightPx), true);
    juce::Graphics g (img);
    drawScene (g, scene, img.getBounds().toFloat(),
               /*grid*/ false, /*icons*/ true, /*direct*/ false,
               RoomView2D::Target::None, RoomView2D::Target::None, /*selectedMic*/ -1,
               EditorSelection {}, /*editMode*/ false);
    return img;
}

//==============================================================================
RoomView2D::RoomView2D()
{
    directPathToggle.setClickingTogglesState (true);
    directPathToggle.setTooltip ("Show the direct source-to-mic path.");
    directPathToggle.onClick = [this] { showDirectPath = directPathToggle.getToggleState(); repaint(); };
    addAndMakeVisible (directPathToggle);
    setWantsKeyboardFocus (false);
}

RoomView2D::~RoomView2D() = default;

void RoomView2D::setScene (const Scene& newScene)
{
    scene = newScene;
    repaint();
}

juce::Image RoomView2D::renderToImage (int widthPx, int heightPx) const
{
    return renderSceneThumbnail (scene, widthPx, heightPx);
}

//==============================================================================
void RoomView2D::setEditMode (bool on)
{
    if (editMode == on) return;
    editMode = on;
    if (! on) { drawing.cancel(); setSelection ({}); }
    repaint();
}

void RoomView2D::setTool (EditorToolPalette::Tool t)
{
    currentTool = t;
    if (t != EditorToolPalette::Tool::Draw) drawing.cancel();
    repaint();
}

void RoomView2D::setSelection (EditorSelection sel)
{
    selection = sel;
    repaint();
}

void RoomView2D::cancelDrawing()
{
    if (drawing.isActive())
    {
        drawing.cancel();
        repaint();
    }
}

void RoomView2D::fireSelectionChanged()
{
    if (onSelectionChanged) onSelectionChanged (selection);
}

Vertex RoomView2D::snapVertex (Vertex v, bool snapModifierDown) const noexcept
{
    if (! snapToGrid || snapModifierDown) return v;
    return Vertex { std::round (v.x / kGridSnapM) * kGridSnapM,
                    std::round (v.y / kGridSnapM) * kGridSnapM };
}

//==============================================================================
void RoomView2D::paint (juce::Graphics& g)
{
    drawScene (g, scene, getLocalBounds().toFloat(),
               /*grid*/ true, /*icons*/ true, showDirectPath, hovered, activeDrag,
               scene.getMicArray().getConfiguration() == MicArray::Configuration::SpacedPair ? selectedMic : -1,
               selection, editMode, /*showReadout*/ true);

    if (editMode)
    {
        const auto xf = computeXf (scene, getLocalBounds().toFloat());

        // Drawing-in-progress overlay: draft vertices, connecting lines, rubber band.
        if (drawing.isActive())
        {
            const auto& verts = drawing.getVertices();
            for (size_t i = 0; i + 1 < verts.size(); ++i)
            {
                g.setColour (Colors::primary);
                g.drawLine ({ xf.toView (verts[i]), xf.toView (verts[i + 1]) }, 1.5f);
            }
            for (const auto& v : verts)
            {
                g.setColour (Colors::primary);
                g.fillRect (juce::Rectangle<float> (kVertexDrawR * 2, kVertexDrawR * 2).withCentre (xf.toView (v)));
            }

            if (! verts.empty())
            {
                // Rubber band from the last placed vertex to the cursor.
                const auto last  = xf.toView (verts.back());
                const auto curr  = cursorScenePos;  // already in view space (set in mouseMove)
                g.setColour (Colors::primaryAlpha40);
                const float dashes[] = { 5.0f, 5.0f };
                juce::Line<float> rubber (last, curr);
                g.drawDashedLine (rubber, dashes, 2, 1.5f);

                // Snap-to-start ring when the cursor is close enough to close.
                const auto startScene = drawing.getFirstVertex();
                if (verts.size() >= 3)
                {
                    const auto cursorScene = xf.toScene (curr);
                    if (startScene.isCloseTo (Vertex { cursorScene.x, cursorScene.y }, 0.5f))
                    {
                        g.setColour (Colors::primary);
                        g.drawEllipse (juce::Rectangle<float> (14.0f, 14.0f).withCentre (xf.toView (startScene)), 2.0f);
                    }
                }
            }
        }

        // Edit-mode border (thin amber rim around the room view).
        g.setColour (Colors::primary.withAlpha (0.7f));
        g.drawRect (getLocalBounds().toFloat(), 1.5f);
    }
}

void RoomView2D::resized()
{
    directPathToggle.setBounds (getWidth() - 56, 8, 48, 22);
}

//==============================================================================
RoomView2D::Target RoomView2D::pickTarget (juce::Point<float> p) const
{
    const auto xf  = computeXf (scene, getLocalBounds().toFloat());
    const auto& arr = scene.getMicArray();
    const bool shotgun = arr.getPattern() == MicPattern::Shotgun;

    auto isNear = [&p] (juce::Point<float> q, float r) { return p.getDistanceFrom (q) <= r; };

    switch (arr.getConfiguration())
    {
        case MicArray::Configuration::Single:
        {
            const auto m = xf.toView (arr.getMic (0).getPosition());
            if (shotgun && isNear (m + xf.viewDir (arr.getMic (0).getOrientation()) * kArrowLenPx, kArrowHitR))
                return Target::Mic0Arrow;
            if (isNear (m, kMicHitR)) return Target::Mic0;
            break;
        }
        case MicArray::Configuration::StereoXY:
        {
            const auto c = xf.toView (arr.getXYPosition());
            const float half = juce::degreesToRadians (arr.getXYAngleDegrees() * 0.5f);
            if (isNear (c + xf.viewDir (rotateZ (arr.getXYOrientation(),  half)) * kArrowLenPx, kArrowHitR))
                return Target::ArrayArrowL;
            if (isNear (c + xf.viewDir (rotateZ (arr.getXYOrientation(), -half)) * kArrowLenPx, kArrowHitR))
                return Target::ArrayArrowR;
            if (isNear (c, kMicHitR + 2.0f)) return Target::ArrayBody;
            break;
        }
        case MicArray::Configuration::SpacedPair:
        {
            const auto m0 = xf.toView (arr.getMic (0).getPosition());
            const auto m1 = xf.toView (arr.getMic (1).getPosition());
            if (shotgun && isNear (m0 + xf.viewDir (arr.getMic (0).getOrientation()) * kArrowLenPx, kArrowHitR))
                return Target::Mic0Arrow;
            if (shotgun && isNear (m1 + xf.viewDir (arr.getMic (1).getOrientation()) * kArrowLenPx, kArrowHitR))
                return Target::Mic1Arrow;
            if (isNear (m0, kMicHitR)) return Target::Mic0;
            if (isNear (m1, kMicHitR)) return Target::Mic1;
            break;
        }
    }

    if (isNear (xf.toView (scene.getSource().getPosition()), kSourceHitR)) return Target::Source;
    return Target::None;
}

EditorSelection RoomView2D::pickEditElement (juce::Point<float> p, bool /*isDeleteMode*/) const
{
    EditorSelection out;
    if (scene.getSectorGeometry().sectors.empty())
        return out;

    const auto xf = computeXf (scene, getLocalBounds().toFloat());
    const auto& sector = scene.getSectorGeometry().sectors[0];

    // Vertices first (highest priority).
    for (size_t i = 0; i < sector.vertices.size(); ++i)
    {
        const auto vp = xf.toView (sector.vertices[i]);
        if (p.getDistanceFrom (vp) <= kVertexHitPx)
            return { EditorSelection::Kind::Vertex, (int) i };
    }
    // Then linedefs (perpendicular distance to the segment, clamped to its endpoints).
    auto distToSegment = [] (juce::Point<float> P, juce::Point<float> A, juce::Point<float> B)
    {
        const juce::Point<float> v = B - A;
        const juce::Point<float> w = P - A;
        const float c1 = v.getX() * w.getX() + v.getY() * w.getY();
        if (c1 <= 0.0f)  return P.getDistanceFrom (A);
        const float c2 = v.getX() * v.getX() + v.getY() * v.getY();
        if (c2 <= c1)    return P.getDistanceFrom (B);
        const juce::Point<float> proj = A + v * (c1 / c2);
        return P.getDistanceFrom (proj);
    };
    for (size_t i = 0; i < sector.lineDefs.size(); ++i)
    {
        const auto& ld = sector.lineDefs[i];
        if (ld.v1Index < 0 || ld.v2Index < 0) continue;
        if ((size_t) ld.v1Index >= sector.vertices.size()) continue;
        if ((size_t) ld.v2Index >= sector.vertices.size()) continue;
        const auto a = xf.toView (sector.vertices[(size_t) ld.v1Index]);
        const auto b = xf.toView (sector.vertices[(size_t) ld.v2Index]);
        if (distToSegment (p, a, b) <= kLineDefHitPx)
            return { EditorSelection::Kind::LineDef, (int) i };
    }
    // Sector body last — any click inside the polygon.
    const auto scenePt = xf.toScene (p);
    if (sector.containsPoint (Vertex { scenePt.x, scenePt.y }))
        return { EditorSelection::Kind::Sector, 0 };

    return out;
}

//==============================================================================
void RoomView2D::mouseMove (const juce::MouseEvent& e)
{
    cursorScenePos = e.position;

    if (! editMode)
    {
        const auto h = pickTarget (e.position);
        if (h != hovered)
        {
            hovered = h;
            setMouseCursor (h == Target::None ? juce::MouseCursor::NormalCursor : juce::MouseCursor::DraggingHandCursor);
            repaint();
        }
        return;
    }

    // Edit mode: crosshair cursor; rubber-band repaint while drawing.
    setMouseCursor (juce::MouseCursor::CrosshairCursor);
    if (drawing.isActive())
        repaint();
}

void RoomView2D::mouseExit (const juce::MouseEvent&)
{
    if (hovered != Target::None) { hovered = Target::None; repaint(); }
}

void RoomView2D::mouseDoubleClick (const juce::MouseEvent& e)
{
    // Browse mode: double-click the source or mic to revert positions to the
    // preset's designed placement (a safety net for the primary drag interaction).
    if (! editMode)
    {
        const auto t = pickTarget (e.position);
        if (t != Target::None && onResetPositions)
        {
            onResetPositions();
            return;
        }
    }
}

void RoomView2D::mouseDown (const juce::MouseEvent& e)
{
    cursorScenePos = e.position;

    // Browse mode: existing source/mic interaction wins.
    if (! editMode)
    {
        activeDrag = pickTarget (e.position);
        int sel = -1;
        if (activeDrag == Target::Mic0) sel = 0;
        else if (activeDrag == Target::Mic1) sel = 1;
        if (sel >= 0 && sel != selectedMic)
        {
            selectedMic = sel;
            if (onMicSelected) onMicSelected (selectedMic);
        }
        if (activeDrag != Target::None) repaint();
        return;
    }

    // Edit mode: priority order — first try source/mic targets (so you can still
    // move source/mic in edit mode); then fall through to the tool.
    activeDrag = pickTarget (e.position);
    if (activeDrag != Target::None && activeDrag != Target::Source
        && activeDrag != Target::Mic0 && activeDrag != Target::Mic1 && activeDrag != Target::ArrayBody
        && activeDrag != Target::Mic0Arrow && activeDrag != Target::Mic1Arrow
        && activeDrag != Target::ArrayArrowL && activeDrag != Target::ArrayArrowR)
        activeDrag = Target::None;
    if (activeDrag != Target::None) { repaint(); return; }

    const auto xf = computeXf (scene, getLocalBounds().toFloat());
    const auto scenePt = xf.toScene (e.position);

    switch (currentTool)
    {
        case EditorToolPalette::Tool::Draw:
        {
            if (! drawing.isActive())
            {
                drawing.begin();
                // Replace any existing sector geometry — Slice 6a is single-sector.
                scene.getSectorGeometry().sectors.clear();
            }

            const Vertex snapped = snapVertex (Vertex { scenePt.x, scenePt.y }, e.mods.isShiftDown());

            if (drawing.wouldCloseAt (snapped, 0.5f))
            {
                Sector s = drawing.closeSector();
                // Reject self-intersecting (bowtie) or degenerate/zero-area
                // polygons — they compile to incoherent geometry and break the
                // inside/outside test. The user restarts the draw.
                if (s.vertices.size() >= 3 && s.isSimpleWithArea())
                {
                    scene.getSectorGeometry().sectors.push_back (s);
                    repaint();
                    if (onSectorCreated) onSectorCreated (s);
                    // Auto-revert to Select after the first sector (per spec §5.3).
                    setTool (EditorToolPalette::Tool::Select);
                    fireEdited (true);
                }
                else
                {
                    repaint();
                    if (onSectorRejected) onSectorRejected();
                }
                return;
            }

            drawing.addVertex (snapped);
            repaint();
            return;
        }

        case EditorToolPalette::Tool::Select:
        {
            const auto picked = pickEditElement (e.position, false);
            setSelection (picked);
            fireSelectionChanged();
            if (picked.kind == EditorSelection::Kind::Vertex)
                draggingVertex = picked.index;
            return;
        }

        case EditorToolPalette::Tool::Delete:
        {
            const auto picked = pickEditElement (e.position, true);
            if (picked.kind != EditorSelection::Kind::None && onDeleteRequested)
                onDeleteRequested (picked);
            return;
        }
    }
}

void RoomView2D::mouseDrag (const juce::MouseEvent& e)
{
    cursorScenePos = e.position;

    // Edit-mode vertex drag.
    if (editMode && draggingVertex >= 0)
    {
        auto& sectors = scene.getSectorGeometry().sectors;
        if (! sectors.empty() && (size_t) draggingVertex < sectors[0].vertices.size())
        {
            const auto xf = computeXf (scene, getLocalBounds().toFloat());
            const auto scenePt = xf.toScene (e.position);
            const Vertex snapped = snapVertex (Vertex { scenePt.x, scenePt.y }, e.mods.isShiftDown());
            sectors[0].moveVertex (draggingVertex, snapped);
            repaint();
            fireEdited (false); // preview
            return;
        }
    }

    if (activeDrag == Target::None) return;

    const auto xf  = computeXf (scene, getLocalBounds().toFloat());
    auto& arr = scene.getMicArray();

    auto clampToBounds = [&] (juce::Point<float> sc)
    {
        const auto b = scene.getBounds();
        const float margin = 1.0f;
        sc.x = juce::jlimit (b.first.x - margin, b.second.x + margin, sc.x);
        sc.y = juce::jlimit (b.first.y - margin, b.second.y + margin, sc.y);
        return sc;
    };

    switch (activeDrag)
    {
        case Target::Source:
        {
            const auto sp = clampToBounds (xf.toScene (e.position));
            const auto z = scene.getSource().getPosition().z;
            scene.getSource().setPosition ({ sp.x, sp.y, z });
            break;
        }
        case Target::Mic0:
        {
            const auto sp = clampToBounds (xf.toScene (e.position));
            const auto z = arr.getMic (0).getPosition().z;
            arr.getMic (0).setPosition ({ sp.x, sp.y, z });
            break;
        }
        case Target::Mic1:
        {
            const auto sp = clampToBounds (xf.toScene (e.position));
            const auto z = arr.getMic (1).getPosition().z;
            arr.getMic (1).setPosition ({ sp.x, sp.y, z });
            break;
        }
        case Target::ArrayBody:
        {
            const auto sp = clampToBounds (xf.toScene (e.position));
            const auto z = arr.getXYPosition().z;
            arr.setXYPosition ({ sp.x, sp.y, z });
            break;
        }
        case Target::Mic0Arrow:
            arr.getMic (0).setOrientation (sceneDirFromDrag (xf.toView (arr.getMic (0).getPosition()), e.position));
            break;
        case Target::Mic1Arrow:
            arr.getMic (1).setOrientation (sceneDirFromDrag (xf.toView (arr.getMic (1).getPosition()), e.position));
            break;
        case Target::ArrayArrowL:
        case Target::ArrayArrowR:
        {
            const float half = juce::degreesToRadians (arr.getXYAngleDegrees() * 0.5f);
            const auto newDir = sceneDirFromDrag (xf.toView (arr.getXYPosition()), e.position);
            arr.setXYOrientation (rotateZ (newDir, activeDrag == Target::ArrayArrowL ? -half : half));
            break;
        }
        case Target::None:
        default: break;
    }

    repaint();
    fireEdited (false);
}

void RoomView2D::mouseUp (const juce::MouseEvent&)
{
    bool wasEdit = false;
    if (editMode && draggingVertex >= 0) { draggingVertex = -1; wasEdit = true; }
    if (activeDrag != Target::None) { activeDrag = Target::None; wasEdit = true; }

    if (wasEdit)
    {
        repaint();
        fireEdited (true);
    }
}

void RoomView2D::fireEdited (bool finalized)
{
    if (onSceneEdited)
        onSceneEdited (scene, finalized);
}
} // namespace Worldizer
