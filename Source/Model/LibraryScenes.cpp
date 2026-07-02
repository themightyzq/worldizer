#include "LibraryScenes.h"
#include "MaterialResolver.h"

namespace Worldizer::LibraryScenes
{
namespace
{
    // Same slab/room helpers as TestScenes (kept local so the two libraries stay
    // independently editable).
    Brush makeSlab (const juce::String& id, Vec3 mn, Vec3 mx, const Material& m)
    {
        Brush b (id, mn, mx, Brush::Kind::Additive);
        b.setAllFaceMaterials (m);
        return b;
    }

    void addRoom (Scene& s,
                  float x0, float x1, float y0, float y1, float z0, float z1,
                  const Material& floorM, const Material& wallM, const Material& ceilM,
                  float th = 0.1f)
    {
        s.addBrush (makeSlab ("floor",     { x0, y0, z0 - th }, { x1, y1, z0      }, floorM));
        s.addBrush (makeSlab ("ceiling",   { x0, y0, z1      }, { x1, y1, z1 + th }, ceilM));
        s.addBrush (makeSlab ("wall_xneg", { x0 - th, y0, z0 }, { x0, y1, z1      }, wallM));
        s.addBrush (makeSlab ("wall_xpos", { x1, y0, z0      }, { x1 + th, y1, z1 }, wallM));
        s.addBrush (makeSlab ("wall_yneg", { x0, y0 - th, z0 }, { x1, y0, z1      }, wallM));
        s.addBrush (makeSlab ("wall_ypos", { x0, y1, z0      }, { x1, y1 + th, z1 }, wallM));
    }

    /** Room without a ceiling (open sky). */
    void addOpenRoom (Scene& s,
                      float x0, float x1, float y0, float y1, float z0, float z1,
                      const Material& floorM, const Material& wallM, float th = 0.1f)
    {
        s.addBrush (makeSlab ("floor",     { x0, y0, z0 - th }, { x1, y1, z0      }, floorM));
        s.addBrush (makeSlab ("wall_xneg", { x0 - th, y0, z0 }, { x0, y1, z1      }, wallM));
        s.addBrush (makeSlab ("wall_xpos", { x1, y0, z0      }, { x1 + th, y1, z1 }, wallM));
        s.addBrush (makeSlab ("wall_yneg", { x0, y0 - th, z0 }, { x1, y0, z1      }, wallM));
        s.addBrush (makeSlab ("wall_ypos", { x0, y1, z0      }, { x1, y1 + th, z1 }, wallM));
    }

    void addBox (Scene& s, const juce::String& id, Vec3 mn, Vec3 mx, const Material& m)
    {
        s.addBrush (makeSlab (id, mn, mx, m));
    }

    void placeSourceMic (Scene& s, Vec3 src, Vec3 mic)
    {
        s.getSource().setPosition (src);
        s.getMic().setPosition (mic);
    }

    Material mat (const char* name) { return MaterialResolver::resolve (name); }
}

//==============================================================================
// --- Indoor ---

Scene stairwell()
{
    // 3x3 m concrete shaft, 12 m tall, with concrete landing slabs every 3 m
    // (half-depth, alternating sides) so the vertical decay picks up stepped
    // reflections. Source at the bottom, mic two flights up.
    Scene s;
    addRoom (s, -1.5f, 1.5f, -1.5f, 1.5f, 0.0f, 12.0f,
             mat ("concrete"), mat ("concrete"), mat ("concrete"));
    for (int level = 1; level <= 3; ++level)
    {
        const float z = 3.0f * (float) level;
        const bool left = (level % 2) == 1;
        addBox (s, "landing" + juce::String (level),
                { left ? -1.5f : 0.0f, -1.5f, z - 0.15f },
                { left ?  0.0f : 1.5f,  1.5f, z          }, mat ("concrete"));
    }
    placeSourceMic (s, { 0.8f, 0.8f, 1.2f }, { -0.8f, -0.8f, 7.0f });
    return s;
}

Scene parkingGarage()
{
    // 40x30 m slab, 2.6 m ceiling, grid of concrete columns.
    Scene s;
    addRoom (s, -20.0f, 20.0f, -15.0f, 15.0f, 0.0f, 2.6f,
             mat ("concrete"), mat ("concrete"), mat ("concrete"));
    for (int cx = -1; cx <= 1; ++cx)
        for (int cy = -1; cy <= 1; ++cy)
            addBox (s, "column_" + juce::String (cx + 1) + juce::String (cy + 1),
                    { cx * 8.0f - 0.3f, cy * 7.0f - 0.3f, 0.0f },
                    { cx * 8.0f + 0.3f, cy * 7.0f + 0.3f, 2.6f }, mat ("concrete"));
    placeSourceMic (s, { -8.0f, -5.0f, 1.4f }, { 6.0f, 4.0f, 1.4f });
    return s;
}

Scene tiledBathroom()
{
    // 2.8x2.2x2.5 m, tile everywhere; a porcelain (marble) tub block.
    Scene s;
    addRoom (s, -1.4f, 1.4f, -1.1f, 1.1f, 0.0f, 2.5f,
             mat ("tile"), mat ("tile"), mat ("plaster"));
    addBox (s, "tub", { -1.4f, 0.4f, 0.0f }, { 1.4f, 1.1f, 0.55f }, mat ("marble"));
    placeSourceMic (s, { -0.7f, -0.5f, 1.5f }, { 0.8f, -0.4f, 1.5f });
    return s;
}

Scene bedroom()
{
    // 4.2x3.6x2.4 m domestic room: carpet, drywall, one curtained wall, a bed.
    Scene s;
    addRoom (s, -2.1f, 2.1f, -1.8f, 1.8f, 0.0f, 2.4f,
             mat ("carpet"), mat ("drywall"), mat ("drywall"));
    addBox (s, "curtain_wall", { -2.1f, 1.65f, 0.0f }, { 2.1f, 1.8f, 2.4f }, mat ("curtain"));
    addBox (s, "bed", { -1.6f, -1.5f, 0.0f }, { 0.4f, 0.1f, 0.55f }, mat ("upholstery"));
    placeSourceMic (s, { 1.2f, 0.8f, 1.4f }, { -1.0f, -0.9f, 1.4f });
    return s;
}

Scene cathedral()
{
    // 44x16 m nave, 20 m vault: marble floor, brick (stone) walls, plaster vault,
    // two rows of columns.
    Scene s;
    addRoom (s, -22.0f, 22.0f, -8.0f, 8.0f, 0.0f, 20.0f,
             mat ("marble"), mat ("brick"), mat ("plaster"));
    for (int i = -2; i <= 2; ++i)
        for (float side : { -4.5f, 4.5f })
            addBox (s, "column_" + juce::String (i + 2) + (side < 0 ? "L" : "R"),
                    { i * 8.0f - 0.5f, side - 0.5f, 0.0f },
                    { i * 8.0f + 0.5f, side + 0.5f, 20.0f }, mat ("marble"));
    placeSourceMic (s, { -14.0f, 0.0f, 1.6f }, { 4.0f, 2.0f, 1.6f });
    return s;
}

Scene warehouse()
{
    // 45x28x9 m: concrete floor, metal walls/roof, aisles of crate stacks.
    Scene s;
    addRoom (s, -22.5f, 22.5f, -14.0f, 14.0f, 0.0f, 9.0f,
             mat ("concrete"), mat ("metal"), mat ("metal"));
    for (int i = -1; i <= 1; ++i)
    {
        addBox (s, "stack_a" + juce::String (i + 1),
                { i * 12.0f - 3.0f, -8.0f, 0.0f }, { i * 12.0f + 3.0f, -4.0f, 4.5f }, mat ("wood_panel"));
        addBox (s, "stack_b" + juce::String (i + 1),
                { i * 12.0f - 3.0f, 4.0f, 0.0f }, { i * 12.0f + 3.0f, 8.0f, 4.5f }, mat ("wood_panel"));
    }
    placeSourceMic (s, { -12.0f, 0.0f, 1.5f }, { 8.0f, 0.0f, 1.5f });
    return s;
}

Scene closet()
{
    // 1.6x1.2x2.2 m, hanging clothes (curtain) on three sides, carpet floor.
    Scene s;
    addRoom (s, -0.8f, 0.8f, -0.6f, 0.6f, 0.0f, 2.2f,
             mat ("carpet"), mat ("drywall"), mat ("drywall"));
    addBox (s, "clothes_xneg", { -0.8f, -0.6f, 0.3f }, { -0.55f, 0.6f, 2.0f }, mat ("curtain"));
    addBox (s, "clothes_xpos", {  0.55f, -0.6f, 0.3f }, {  0.8f, 0.6f, 2.0f }, mat ("curtain"));
    addBox (s, "clothes_ypos", { -0.8f, 0.35f, 0.3f },  {  0.8f, 0.6f, 2.0f }, mat ("curtain"));
    placeSourceMic (s, { 0.0f, -0.35f, 1.5f }, { 0.25f, 0.0f, 1.5f });
    return s;
}

Scene kitchen()
{
    // 4.5x3.2x2.5 m: tile floor, drywall, counter runs (wood) and a fridge (metal).
    Scene s;
    addRoom (s, -2.25f, 2.25f, -1.6f, 1.6f, 0.0f, 2.5f,
             mat ("tile"), mat ("drywall"), mat ("plaster"));
    addBox (s, "counter_a", { -2.25f, -1.6f, 0.0f }, { 2.0f, -1.0f, 0.9f }, mat ("wood_panel"));
    addBox (s, "counter_b", { -2.25f, -1.0f, 0.0f }, { -1.65f, 1.2f, 0.9f }, mat ("wood_panel"));
    addBox (s, "fridge",    { 1.55f, 0.9f, 0.0f },   { 2.25f, 1.6f, 1.9f }, mat ("metal"));
    placeSourceMic (s, { -0.8f, 0.6f, 1.4f }, { 1.2f, -0.4f, 1.4f });
    return s;
}

//==============================================================================
// --- Outdoor ---

Scene backAlley()
{
    // Two 12 m brick facades, 4 m apart, 30 m long, asphalt ground, open sky
    // and open ends. Dumpster halfway down.
    Scene s;
    addBox (s, "ground",      { -15.0f, -6.0f, -0.2f }, { 15.0f, 6.0f, 0.0f }, mat ("asphalt"));
    addBox (s, "facade_yneg", { -15.0f, -2.6f, 0.0f },  { 15.0f, -2.0f, 12.0f }, mat ("brick"));
    addBox (s, "facade_ypos", { -15.0f, 2.0f, 0.0f },   { 15.0f, 2.6f, 12.0f }, mat ("brick"));
    addBox (s, "dumpster",    { 3.0f, 0.8f, 0.0f },     { 5.0f, 2.0f, 1.3f }, mat ("metal"));
    placeSourceMic (s, { -6.0f, 0.0f, 1.4f }, { 4.0f, -0.8f, 1.4f });
    return s;
}

Scene courtyard()
{
    // 14x14 m plaster-walled court, 8 m walls, open sky, grass centre + tile path.
    Scene s;
    addOpenRoom (s, -7.0f, 7.0f, -7.0f, 7.0f, 0.0f, 8.0f,
                 mat ("tile"), mat ("plaster"));
    addBox (s, "lawn", { -4.5f, -4.5f, 0.0f }, { 4.5f, 4.5f, 0.05f }, mat ("grass"));
    addBox (s, "fountain", { -1.0f, -1.0f, 0.0f }, { 1.0f, 1.0f, 0.4f }, mat ("water"));
    placeSourceMic (s, { -4.0f, 3.0f, 1.4f }, { 3.5f, -3.0f, 1.4f });
    return s;
}

Scene canyon()
{
    // Two colossal rock faces (brick + heavy scatter reads as rock) 26 m apart,
    // gravel floor, open sky/ends: discrete far echoes.
    Scene s;
    addBox (s, "ground",    { -40.0f, -20.0f, -0.3f }, { 40.0f, 20.0f, 0.0f }, mat ("gravel"));
    addBox (s, "face_yneg", { -40.0f, -14.5f, 0.0f },  { 40.0f, -13.0f, 25.0f }, mat ("brick"));
    addBox (s, "face_ypos", { -40.0f, 13.0f, 0.0f },   { 40.0f, 14.5f, 25.0f }, mat ("brick"));
    placeSourceMic (s, { -10.0f, -6.0f, 1.6f }, { 8.0f, 5.0f, 1.6f });
    return s;
}

Scene streetTunnel()
{
    // 36 m concrete tube, 8 m wide, 5 m high, open both ends (no end caps).
    Scene s;
    addBox (s, "road",      { -18.0f, -4.0f, -0.2f }, { 18.0f, 4.0f, 0.0f }, mat ("asphalt"));
    addBox (s, "roof",      { -18.0f, -4.0f, 5.0f },  { 18.0f, 4.0f, 5.3f }, mat ("concrete"));
    addBox (s, "wall_yneg", { -18.0f, -4.3f, 0.0f },  { 18.0f, -4.0f, 5.0f }, mat ("concrete"));
    addBox (s, "wall_ypos", { -18.0f, 4.0f, 0.0f },   { 18.0f, 4.3f, 5.0f }, mat ("concrete"));
    placeSourceMic (s, { -10.0f, 1.5f, 1.5f }, { 6.0f, -1.5f, 1.5f });
    return s;
}

//==============================================================================
// --- Vehicles & Devices ---

Scene carInterior()
{
    // 2.4x1.5x1.15 m cabin: upholstery seats/headliner, carpet floor, glass sides.
    Scene s;
    addRoom (s, -1.2f, 1.2f, -0.75f, 0.75f, 0.0f, 1.15f,
             mat ("carpet"), mat ("glass"), mat ("upholstery"));
    addBox (s, "front_seats", { -0.9f, -0.65f, 0.0f }, { -0.3f, 0.65f, 0.7f }, mat ("upholstery"));
    addBox (s, "rear_bench",  { 0.5f, -0.65f, 0.0f },  { 1.1f, 0.65f, 0.65f }, mat ("upholstery"));
    placeSourceMic (s, { -0.6f, -0.4f, 0.9f }, { 0.8f, 0.3f, 0.85f });
    return s;
}

Scene cargoVan()
{
    // 3.2x1.7x1.8 m empty steel cargo box, wood floor.
    Scene s;
    addRoom (s, -1.6f, 1.6f, -0.85f, 0.85f, 0.0f, 1.8f,
             mat ("wood_floor"), mat ("metal"), mat ("metal"));
    placeSourceMic (s, { -1.0f, 0.0f, 1.2f }, { 1.1f, 0.3f, 1.2f });
    return s;
}

//==============================================================================
// --- Cinematic ---

Scene bunker()
{
    // 7x4 m concrete cell, 2.1 m ceiling, a metal bulkhead and crates.
    Scene s;
    addRoom (s, -3.5f, 3.5f, -2.0f, 2.0f, 0.0f, 2.1f,
             mat ("concrete"), mat ("concrete"), mat ("concrete"));
    addBox (s, "bulkhead", { 3.2f, -2.0f, 0.0f }, { 3.5f, 2.0f, 2.1f }, mat ("metal"));
    addBox (s, "crates",   { -3.2f, 0.8f, 0.0f }, { -1.8f, 2.0f, 1.1f }, mat ("wood_panel"));
    placeSourceMic (s, { -1.5f, -1.0f, 1.3f }, { 1.8f, 0.8f, 1.3f });
    return s;
}

Scene cave()
{
    // Irregular rock chamber: offset overlapping boxes rough out a ~12 m cavity
    // with heavy scattering (gravel/brick), low uneven ceiling.
    Scene s;
    addBox (s, "floor_a",  { -6.0f, -5.0f, -0.3f }, { 6.0f, 5.0f, 0.0f }, mat ("gravel"));
    addBox (s, "floor_b",  { -2.0f, -3.0f, 0.0f },  { 3.0f, 2.0f, 0.4f }, mat ("gravel"));
    addBox (s, "roof_a",   { -6.0f, -5.0f, 3.2f },  { 6.0f, 5.0f, 3.8f }, mat ("brick"));
    addBox (s, "roof_b",   { -3.0f, -2.0f, 2.4f },  { 2.0f, 3.0f, 3.2f }, mat ("brick"));
    addBox (s, "wall_w",   { -6.5f, -5.0f, 0.0f },  { -5.5f, 5.0f, 3.6f }, mat ("brick"));
    addBox (s, "wall_e",   { 5.5f, -5.0f, 0.0f },   { 6.5f, 5.0f, 3.6f }, mat ("brick"));
    addBox (s, "wall_s",   { -6.0f, -5.6f, 0.0f },  { 6.0f, -4.6f, 3.6f }, mat ("brick"));
    addBox (s, "wall_n",   { -6.0f, 4.6f, 0.0f },   { 6.0f, 5.6f, 3.6f }, mat ("brick"));
    addBox (s, "boulder",  { 2.0f, -2.5f, 0.0f },   { 4.0f, -0.5f, 1.6f }, mat ("brick"));
    placeSourceMic (s, { -3.5f, 2.0f, 1.4f }, { 3.0f, 2.5f, 1.4f });
    return s;
}

Scene grainSilo()
{
    // 5.5x5.5 m metal shaft, 16 m tall (a box approximating the cylinder):
    // strong vertical ring, metallic walls.
    Scene s;
    addRoom (s, -2.75f, 2.75f, -2.75f, 2.75f, 0.0f, 16.0f,
             mat ("concrete"), mat ("metal"), mat ("metal"));
    placeSourceMic (s, { 0.0f, 0.0f, 1.3f }, { 1.6f, 1.6f, 2.2f });
    return s;
}

//==============================================================================
// --- Experimental ---

Scene infiniteCorridor()
{
    // 64 m glass-walled corridor, 2 m wide, 2.4 m high: worst-case flutter,
    // deliberately unphysical-feeling.
    Scene s;
    addRoom (s, -32.0f, 32.0f, -1.0f, 1.0f, 0.0f, 2.4f,
             mat ("marble"), mat ("glass"), mat ("glass"));
    placeSourceMic (s, { -20.0f, 0.0f, 1.4f }, { 8.0f, 0.0f, 1.4f });
    return s;
}

Scene resonantTank()
{
    // 4.2 m all-metal cube: brutal single-note metallic ring.
    Scene s;
    addRoom (s, -2.1f, 2.1f, -2.1f, 2.1f, 0.0f, 4.2f,
             mat ("metal"), mat ("metal"), mat ("metal"));
    placeSourceMic (s, { -1.2f, -1.2f, 1.2f }, { 1.2f, 1.2f, 2.6f });
    return s;
}

Scene deadRoom()
{
    // 3.4x3 m curtain-lined booth over carpet with an acoustic-tile ceiling:
    // drier than an open field, yet claustrophobically close.
    Scene s;
    addRoom (s, -1.7f, 1.7f, -1.5f, 1.5f, 0.0f, 2.3f,
             mat ("carpet"), mat ("curtain"), mat ("acoustic_tile"));
    placeSourceMic (s, { -0.9f, 0.0f, 1.4f }, { 0.9f, 0.0f, 1.4f });
    return s;
}

//==============================================================================
Scene byName (const juce::String& name, bool& ok)
{
    ok = true;
    if (name == "stairwell")        return stairwell();
    if (name == "parkingGarage")    return parkingGarage();
    if (name == "tiledBathroom")    return tiledBathroom();
    if (name == "bedroom")          return bedroom();
    if (name == "cathedral")        return cathedral();
    if (name == "warehouse")        return warehouse();
    if (name == "closet")           return closet();
    if (name == "kitchen")          return kitchen();
    if (name == "backAlley")        return backAlley();
    if (name == "courtyard")        return courtyard();
    if (name == "canyon")           return canyon();
    if (name == "streetTunnel")     return streetTunnel();
    if (name == "carInterior")      return carInterior();
    if (name == "cargoVan")         return cargoVan();
    if (name == "bunker")           return bunker();
    if (name == "cave")             return cave();
    if (name == "grainSilo")        return grainSilo();
    if (name == "infiniteCorridor") return infiniteCorridor();
    if (name == "resonantTank")     return resonantTank();
    if (name == "deadRoom")         return deadRoom();

    ok = false;
    return bedroom();
}
} // namespace Worldizer::LibraryScenes
