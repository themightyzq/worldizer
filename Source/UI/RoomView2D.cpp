#include "RoomView2D.h"
#include "../Shared/WorldizerLookAndFeel.h"
#include <cmath>

namespace Worldizer
{
namespace
{
    // Scene<->view transform: X horizontal, Y vertical (scene +Y is screen-up).
    struct ViewXf
    {
        float ppm = 50.0f, sceneCx = 0.0f, sceneCy = 0.0f, areaCx = 0.0f, areaCy = 0.0f;

        juce::Point<float> toView (float x, float y) const
        {
            return { areaCx + (x - sceneCx) * ppm, areaCy - (y - sceneCy) * ppm };
        }
        juce::Point<float> toScene (juce::Point<float> p) const
        {
            return { sceneCx + (p.x - areaCx) / ppm, sceneCy - (p.y - areaCy) / ppm };
        }
    };

    ViewXf computeXf (const Scene& scene, juce::Rectangle<float> area)
    {
        const auto b = scene.getBounds();
        float minX = b.first.x, maxX = b.second.x, minY = b.first.y, maxY = b.second.y;

        // Empty/degenerate geometry: frame the source + mic instead.
        if (maxX - minX < 0.5f || maxY - minY < 0.5f)
        {
            const auto s = scene.getSource().getPosition();
            const auto m = scene.getMic().getPosition();
            minX = juce::jmin (minX, s.x, m.x) - 3.0f; maxX = juce::jmax (maxX, s.x, m.x) + 3.0f;
            minY = juce::jmin (minY, s.y, m.y) - 3.0f; maxY = juce::jmax (maxY, s.y, m.y) + 3.0f;
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

    void drawScene (juce::Graphics& g, const Scene& scene, juce::Rectangle<float> area,
                    bool drawGrid, bool drawIcons, bool drawDirect,
                    RoomView2D::Target hovered, RoomView2D::Target active)
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

        // Brushes (additive amber, subtractive red).
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

        const auto srcP = xf.toView (scene.getSource().getPosition().x, scene.getSource().getPosition().y);
        const auto micP = xf.toView (scene.getMic().getPosition().x,    scene.getMic().getPosition().y);

        if (drawDirect)
        {
            g.setColour (Colors::rayGuide);
            const float dashes[] = { 4.0f, 4.0f };
            juce::Line<float> line (srcP, micP);
            g.drawDashedLine (line, dashes, 2, 1.0f);
        }

        if (drawIcons)
        {
            // Source: amber filled circle. No facing arrow — the source is
            // omnidirectional for now; directivity (and a real orientation
            // indicator) arrives with v1.0.
            drawIcon (g, srcP, 8.0f, Colors::sourceIcon, juce::Colours::transparentBlack,
                      hovered == RoomView2D::Target::Source || active == RoomView2D::Target::Source);
            // Mic: cyan with amber ring.
            drawIcon (g, micP, 7.0f, Colors::micIcon, Colors::primary,
                      hovered == RoomView2D::Target::Mic || active == RoomView2D::Target::Mic);

            // "S" / "M" labels under the icons (skip in small thumbnails).
            if (area.getWidth() > 200.0f)
            {
                g.setFont (juce::Font (juce::FontOptions (11.0f).withStyle ("Bold")));
                g.setColour (Colors::sourceIcon);
                g.drawText ("S", juce::Rectangle<float> (srcP.x - 12.0f, srcP.y + 9.0f, 24.0f, 14.0f), juce::Justification::centred);
                g.setColour (Colors::micIcon);
                g.drawText ("M", juce::Rectangle<float> (micP.x - 12.0f, micP.y + 8.0f, 24.0f, 14.0f), juce::Justification::centred);
            }
        }

        // Scale indicator (bottom-left): a 1 m bar.
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

        // Elevation indicator (bottom-right): source/mic Z.
        if (drawIcons)
        {
            g.setColour (Colors::onSurfaceVariant);
            g.setFont (juce::Font (juce::FontOptions (10.0f)));
            const auto txt = "src z " + juce::String (scene.getSource().getPosition().z, 1)
                           + "  mic z " + juce::String (scene.getMic().getPosition().z, 1);
            g.drawText (txt, area.removeFromBottom (16).removeFromRight (160).reduced (6, 0),
                        juce::Justification::centredRight);
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
               RoomView2D::Target::None, RoomView2D::Target::None);
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

void RoomView2D::paint (juce::Graphics& g)
{
    drawScene (g, scene, getLocalBounds().toFloat(),
               /*grid*/ true, /*icons*/ true, showDirectPath, hovered, activeDrag);
}

void RoomView2D::resized()
{
    directPathToggle.setBounds (getWidth() - 56, 8, 48, 22);
}

RoomView2D::Target RoomView2D::pickTarget (juce::Point<float> p) const
{
    const auto xf = computeXf (scene, getLocalBounds().toFloat());
    const auto src = xf.toView (scene.getSource().getPosition().x, scene.getSource().getPosition().y);
    const auto mic = xf.toView (scene.getMic().getPosition().x,    scene.getMic().getPosition().y);

    // Mic drawn on top — prefer it when overlapping.
    if (p.getDistanceFrom (mic) <= 12.0f) return Target::Mic;
    if (p.getDistanceFrom (src) <= 13.0f) return Target::Source;
    return Target::None;
}

void RoomView2D::mouseMove (const juce::MouseEvent& e)
{
    const auto h = pickTarget (e.position);
    if (h != hovered)
    {
        hovered = h;
        setMouseCursor (h == Target::None ? juce::MouseCursor::NormalCursor : juce::MouseCursor::DraggingHandCursor);
        repaint();
    }
}

void RoomView2D::mouseExit (const juce::MouseEvent&)
{
    if (hovered != Target::None) { hovered = Target::None; repaint(); }
}

void RoomView2D::mouseDown (const juce::MouseEvent& e)
{
    activeDrag = pickTarget (e.position);
    if (activeDrag != Target::None)
        repaint();
}

void RoomView2D::mouseDrag (const juce::MouseEvent& e)
{
    if (activeDrag == Target::None)
        return;

    const auto xf = computeXf (scene, getLocalBounds().toFloat());
    auto sp = xf.toScene (e.position);

    // Soft-clamp to the scene bounds (+ small margin) so icons stay visible.
    const auto b = scene.getBounds();
    const float margin = 1.0f;
    sp.x = juce::jlimit (b.first.x - margin, b.second.x + margin, sp.x);
    sp.y = juce::jlimit (b.first.y - margin, b.second.y + margin, sp.y);

    if (activeDrag == Target::Source)
    {
        auto pos = scene.getSource().getPosition();
        scene.getSource().setPosition ({ sp.x, sp.y, pos.z });
    }
    else
    {
        auto pos = scene.getMic().getPosition();
        scene.getMic().setPosition ({ sp.x, sp.y, pos.z });
    }

    repaint();
    if (onPositionsChanged)
        onPositionsChanged (scene.getSource().getPosition(), scene.getMic().getPosition());
}

void RoomView2D::mouseUp (const juce::MouseEvent&)
{
    if (activeDrag != Target::None)
    {
        activeDrag = Target::None;
        repaint();
        if (onPositionsFinalized)
            onPositionsFinalized (scene.getSource().getPosition(), scene.getMic().getPosition());
    }
}
} // namespace Worldizer
