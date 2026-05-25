#include "TestScenes.h"

namespace Worldizer::TestScenes
{
namespace
{
    Brush makeSlab (const juce::String& id, Vec3 mn, Vec3 mx, const Material& m)
    {
        Brush b (id, mn, mx, Brush::Kind::Additive);
        b.setAllFaceMaterials (m);
        return b;
    }

    /** Adds the 6 slab brushes forming a closed box around the interior volume
        [x0,x1] x [y0,y1] x [z0,z1]. Each slab sits just outside the interior, so
        the room-facing faces bound the interior exactly. */
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

    /** Adds a ground plane only (no walls/ceiling): a thin slab spanning the area. */
    void addGround (Scene& s, float halfExtent, const Material& m, float th = 0.2f)
    {
        s.addBrush (makeSlab ("ground",
                              { -halfExtent, -halfExtent, -th },
                              {  halfExtent,  halfExtent, 0.0f }, m));
    }

    /** A solid box (e.g. a tree trunk/canopy), all faces the same material. */
    void addBox (Scene& s, const juce::String& id, Vec3 mn, Vec3 mx, const Material& m)
    {
        s.addBrush (makeSlab (id, mn, mx, m));
    }
}

Scene smallConcreteRoom()
{
    Scene s;
    addRoom (s, -3.0f, 3.0f, -2.0f, 2.0f, 0.0f, 3.0f,
             Material::concrete(), Material::drywall(), Material::drywall());

    s.getSource().setPosition ({ 0.0f, 0.0f, 1.5f });
    s.getMic().setPosition    ({ 2.0f, 0.0f, 1.5f });
    return s;
}

Scene hallway()
{
    Scene s;
    addRoom (s, -10.0f, 10.0f, -1.0f, 1.0f, 0.0f, 2.5f,
             Material::woodFloor(), Material::drywall(), Material::drywall());

    s.getSource().setPosition ({ -9.0f, 0.0f, 1.25f });
    s.getMic().setPosition    ({  3.0f, 0.0f, 1.25f }); // 12 m down the hall
    return s;
}

Scene forestClearing()
{
    Scene s;
    addGround (s, 20.0f, Material::gravel());

    // A handful of trees: tall thin foliage boxes scattered around the clearing.
    const auto tree = Material::foliage();
    auto addTree = [&] (const juce::String& id, float cx, float cy)
    {
        addBox (s, id, { cx - 0.25f, cy - 0.25f, 0.0f }, { cx + 0.25f, cy + 0.25f, 8.0f }, tree);
    };
    addTree ("tree1",  0.0f,  3.5f);
    addTree ("tree2", -4.0f, -2.0f);
    addTree ("tree3",  5.0f,  1.5f);
    addTree ("tree4",  3.0f, -4.0f);
    addTree ("tree5", -2.5f,  5.0f);
    addTree ("tree6",  6.0f, -1.0f);

    s.getSource().setPosition ({ -2.5f, 0.0f, 1.5f });
    s.getMic().setPosition    ({  2.5f, 0.0f, 1.5f }); // 5 m apart
    return s;
}

Scene gymnasium()
{
    Scene s;
    addRoom (s, -15.0f, 15.0f, -10.0f, 10.0f, 0.0f, 8.0f,
             Material::concrete(), Material::drywall(), Material::drywall());

    s.getSource().setPosition ({ -5.0f, 0.0f, 1.5f });
    s.getMic().setPosition    ({  5.0f, 0.0f, 1.5f }); // 10 m apart
    return s;
}

Scene anechoic()
{
    Scene s;
    addGround (s, 20.0f, Material::carpet()); // absorbent ground, no other surfaces

    s.getSource().setPosition ({ 0.0f, 0.0f, 1.5f });
    s.getMic().setPosition    ({ 2.0f, 0.0f, 1.5f });
    return s;
}

Scene byName (const juce::String& name, bool& ok)
{
    ok = true;
    if (name == "smallConcreteRoom") return smallConcreteRoom();
    if (name == "hallway")           return hallway();
    if (name == "forestClearing")    return forestClearing();
    if (name == "gymnasium")         return gymnasium();
    if (name == "anechoic")          return anechoic();

    ok = false;
    return smallConcreteRoom();
}
} // namespace Worldizer::TestScenes
