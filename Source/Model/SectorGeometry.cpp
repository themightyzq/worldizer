#include "SectorGeometry.h"
#include "MaterialResolver.h"
#include "Scene.h"
#include <limits>

namespace Worldizer
{
namespace
{
    constexpr float kSlabThickness = 0.1f;  // 10 cm thin floor/ceiling slabs
    constexpr float kWallThickness = 0.1f;  // 10 cm walls
}

std::vector<Brush> SectorGeometry::compileToBrushes() const
{
    std::vector<Brush> out;

    for (size_t s = 0; s < sectors.size(); ++s)
    {
        const auto& sector = sectors[s];
        if (sector.vertices.empty())
            continue;

        const auto aabb = sector.getAABB();
        const auto idPrefix = "sector" + juce::String ((int) s);

        // --- Floor: a thin slab whose TOP face is at sector.floorHeight ---
        Brush floor (idPrefix + "_floor",
                     Vec3 (aabb.minX, aabb.minY, sector.floorHeight - kSlabThickness),
                     Vec3 (aabb.maxX, aabb.maxY, sector.floorHeight),
                     Brush::Kind::Additive);
        floor.setAllFaceMaterials (MaterialResolver::resolve (sector.floorMaterial));
        out.push_back (std::move (floor));

        // --- Ceiling: a thin slab whose BOTTOM face is at sector.ceilingHeight ---
        Brush ceiling (idPrefix + "_ceiling",
                       Vec3 (aabb.minX, aabb.minY, sector.ceilingHeight),
                       Vec3 (aabb.maxX, aabb.maxY, sector.ceilingHeight + kSlabThickness),
                       Brush::Kind::Additive);
        ceiling.setAllFaceMaterials (MaterialResolver::resolve (sector.ceilingMaterial));
        out.push_back (std::move (ceiling));

        // --- Walls: one OrientedWall per linedef (skip out-of-range refs defensively) ---
        for (size_t i = 0; i < sector.lineDefs.size(); ++i)
        {
            const auto& ld = sector.lineDefs[i];
            if (ld.v1Index < 0 || ld.v2Index < 0) continue;
            if ((size_t) ld.v1Index >= sector.vertices.size()) continue;
            if ((size_t) ld.v2Index >= sector.vertices.size()) continue;

            const auto v1 = sector.vertices[(size_t) ld.v1Index];
            const auto v2 = sector.vertices[(size_t) ld.v2Index];

            Brush::OrientedWallParams p;
            p.v1        = Vec3 (v1.x, v1.y, sector.floorHeight);
            p.v2        = Vec3 (v2.x, v2.y, sector.floorHeight);
            p.floorZ    = sector.floorHeight;
            p.ceilingZ  = sector.ceilingHeight;
            p.thickness = kWallThickness;

            Brush wall (idPrefix + "_wall" + juce::String ((int) i), p, Brush::Kind::Additive);
            wall.setAllFaceMaterials (MaterialResolver::resolve (ld.frontMaterial));
            out.push_back (std::move (wall));
        }
    }

    return out;
}

SectorGeometry::AABB3D SectorGeometry::getBounds() const noexcept
{
    AABB3D b;
    if (sectors.empty())
        return b;

    const float inf = std::numeric_limits<float>::infinity();
    b.min = { inf,  inf,  inf };
    b.max = { -inf, -inf, -inf };

    for (const auto& sector : sectors)
    {
        if (sector.vertices.empty()) continue;
        const auto a = sector.getAABB();
        b.min = { std::min (b.min.x, a.minX), std::min (b.min.y, a.minY), std::min (b.min.z, sector.floorHeight) };
        b.max = { std::max (b.max.x, a.maxX), std::max (b.max.y, a.maxY), std::max (b.max.z, sector.ceilingHeight) };
    }

    if (b.min.x > b.max.x)  // never had any vertices
        b = {};
    return b;
}

//==============================================================================
juce::var SectorGeometry::toJson() const
{
    auto* root = new juce::DynamicObject();

    juce::Array<juce::var> sectorArr;
    for (const auto& sector : sectors)
    {
        auto* so = new juce::DynamicObject();

        juce::Array<juce::var> verts;
        for (const auto& v : sector.vertices)
        {
            auto* vo = new juce::DynamicObject();
            vo->setProperty ("x", (double) v.x);
            vo->setProperty ("y", (double) v.y);
            verts.add (juce::var (vo));
        }
        so->setProperty ("vertices", verts);

        juce::Array<juce::var> lines;
        for (const auto& ld : sector.lineDefs)
        {
            auto* lo = new juce::DynamicObject();
            lo->setProperty ("v1", ld.v1Index);
            lo->setProperty ("v2", ld.v2Index);
            lo->setProperty ("front_material", ld.frontMaterial);
            if (ld.isTwoSided)
            {
                lo->setProperty ("back_material", ld.backMaterial);
                lo->setProperty ("two_sided", true);
            }
            lines.add (juce::var (lo));
        }
        so->setProperty ("linedefs", lines);

        so->setProperty ("floor_height",     (double) sector.floorHeight);
        so->setProperty ("ceiling_height",   (double) sector.ceilingHeight);
        so->setProperty ("floor_material",   sector.floorMaterial);
        so->setProperty ("ceiling_material", sector.ceilingMaterial);

        sectorArr.add (juce::var (so));
    }
    root->setProperty ("sectors", sectorArr);
    return juce::var (root);
}

bool SectorGeometry::fromJson (const juce::var& json, juce::String& errorOut)
{
    sectors.clear();
    if (! json.isObject())
        return true; // an empty/missing block is valid — no sectors

    auto* sectorArr = json["sectors"].getArray();
    if (sectorArr == nullptr)
        return true; // no sectors

    for (const auto& sv : *sectorArr)
    {
        Sector sector;

        if (auto* verts = sv["vertices"].getArray())
            for (const auto& vv : *verts)
                sector.vertices.push_back (Vertex { (float) (double) vv["x"], (float) (double) vv["y"] });

        if (auto* lines = sv["linedefs"].getArray())
        {
            for (const auto& lv : *lines)
            {
                LineDef ld;
                ld.v1Index       = (int) lv["v1"];
                ld.v2Index       = (int) lv["v2"];
                ld.frontMaterial = lv["front_material"].toString();
                if (ld.frontMaterial.isEmpty()) ld.frontMaterial = "drywall";
                if (lv.hasProperty ("back_material")) ld.backMaterial = lv["back_material"].toString();
                if (lv.hasProperty ("two_sided"))     ld.isTwoSided   = (bool) lv["two_sided"];
                sector.lineDefs.push_back (ld);
            }
        }

        if (sv.hasProperty ("floor_height"))     sector.floorHeight     = (float) (double) sv["floor_height"];
        if (sv.hasProperty ("ceiling_height"))   sector.ceilingHeight   = (float) (double) sv["ceiling_height"];
        if (sv.hasProperty ("floor_material"))   sector.floorMaterial   = sv["floor_material"].toString();
        if (sv.hasProperty ("ceiling_material")) sector.ceilingMaterial = sv["ceiling_material"].toString();

        sectors.push_back (std::move (sector));
    }

    errorOut.clear();
    return true;
}

//==============================================================================
namespace
{
    struct ShellBrushes
    {
        const Brush* floor   = nullptr;
        const Brush* ceiling = nullptr;
        const Brush* wxn     = nullptr;
        const Brush* wxp     = nullptr;
        const Brush* wyn     = nullptr;
        const Brush* wyp     = nullptr;

        bool complete() const noexcept
        {
            return floor != nullptr && ceiling != nullptr
                && wxn != nullptr && wxp != nullptr && wyn != nullptr && wyp != nullptr;
        }
    };

    ShellBrushes findShell (const Scene& scene)
    {
        ShellBrushes s;
        for (const auto& b : scene.getBrushes())
        {
            if (b.getType() != Brush::Type::Box || b.getKind() != Brush::Kind::Additive)
                continue;
            const auto& id = b.getId();
            if      (id == "floor")     s.floor   = &b;
            else if (id == "ceiling")   s.ceiling = &b;
            else if (id == "wall_xneg") s.wxn     = &b;
            else if (id == "wall_xpos") s.wxp     = &b;
            else if (id == "wall_yneg") s.wyn     = &b;
            else if (id == "wall_ypos") s.wyp     = &b;
        }
        return s;
    }

    void eraseShell (Scene& scene)
    {
        for (const auto* id : { "floor", "ceiling", "wall_xneg", "wall_xpos", "wall_yneg", "wall_ypos" })
            scene.removeBrushById (id);
    }
}

bool SectorGeometry::convertRoomShell (Scene& scene)
{
    if (! scene.getSectorGeometry().isEmpty())
        return false;

    const auto shell = findShell (scene);
    if (! shell.complete())
        return false; // open-air / irregular scene — no closed shell to convert

    // Interior bounds: the room-facing faces of the shell slabs.
    const float x0 = shell.wxn->getMax().x, x1 = shell.wxp->getMin().x;
    const float y0 = shell.wyn->getMax().y, y1 = shell.wyp->getMin().y;
    const float z0 = shell.floor->getMax().z, z1 = shell.ceiling->getMin().z;
    if (x1 <= x0 || y1 <= y0 || z1 <= z0)
        return false;

    Sector sector;
    sector.floorHeight     = z0;
    sector.ceilingHeight   = z1;
    // All shipped scenes assign brush materials via setAllFaceMaterials, so any
    // face carries the material; read the room-facing one for clarity.
    sector.floorMaterial   = shell.floor->getFaceMaterial (Brush::Face::PosZ).getName();
    sector.ceilingMaterial = shell.ceiling->getFaceMaterial (Brush::Face::NegZ).getName();

    // Counter-clockwise perimeter starting at (x0, y0). Edge i connects vertex i
    // to i+1: south (y0), east (x1), north (y1), west (x0).
    sector.vertices = { { x0, y0 }, { x1, y0 }, { x1, y1 }, { x0, y1 } };
    sector.lineDefs = {
        LineDef (0, 1, shell.wyn->getFaceMaterial (Brush::Face::PosY).getName()),
        LineDef (1, 2, shell.wxp->getFaceMaterial (Brush::Face::NegX).getName()),
        LineDef (2, 3, shell.wyp->getFaceMaterial (Brush::Face::NegY).getName()),
        LineDef (3, 0, shell.wxn->getFaceMaterial (Brush::Face::PosX).getName()),
    };
    if (! sector.isCounterClockwise())
        sector.reverseWinding();

    scene.getSectorGeometry().sectors = { std::move (sector) };
    eraseShell (scene);
    return true;
}

bool SectorGeometry::removeRoomShell (Scene& scene)
{
    if (! findShell (scene).complete())
        return false;
    eraseShell (scene);
    return true;
}
} // namespace Worldizer
