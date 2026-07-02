#include "MaterialResolver.h"

namespace Worldizer
{
Material MaterialResolver::resolve (const juce::String& name)
{
    if (name == "concrete")      return Material::concrete();
    if (name == "drywall")       return Material::drywall();
    if (name == "wood_floor")    return Material::woodFloor();
    if (name == "carpet")        return Material::carpet();
    if (name == "curtain")       return Material::curtain();
    if (name == "glass")         return Material::glass();
    if (name == "foliage")       return Material::foliage();
    if (name == "gravel")        return Material::gravel();
    if (name == "open_air")      return Material::openAir();
    if (name == "brick")         return Material::brick();
    if (name == "marble")        return Material::marble();
    if (name == "tile")          return Material::tile();
    if (name == "plaster")       return Material::plaster();
    if (name == "acoustic_tile") return Material::acousticTile();
    if (name == "metal")         return Material::metal();
    if (name == "wood_panel")    return Material::woodPanel();
    if (name == "upholstery")    return Material::upholstery();
    if (name == "asphalt")       return Material::asphalt();
    if (name == "grass")         return Material::grass();
    if (name == "water")         return Material::water();

    juce::Logger::writeToLog ("MaterialResolver: unknown material '" + name + "', using drywall");
    return Material::drywall();
}

juce::StringArray MaterialResolver::getKnownNames()
{
    return { "concrete", "drywall", "wood_floor", "carpet", "curtain",
             "glass", "foliage", "gravel", "open_air",
             "brick", "marble", "tile", "plaster", "acoustic_tile",
             "metal", "wood_panel", "upholstery", "asphalt", "grass", "water" };
}
} // namespace Worldizer
