#include "MaterialResolver.h"

namespace Worldizer
{
Material MaterialResolver::resolve (const juce::String& name)
{
    if (name == "concrete")   return Material::concrete();
    if (name == "drywall")    return Material::drywall();
    if (name == "wood_floor") return Material::woodFloor();
    if (name == "carpet")     return Material::carpet();
    if (name == "curtain")    return Material::curtain();
    if (name == "glass")      return Material::glass();
    if (name == "foliage")    return Material::foliage();
    if (name == "gravel")     return Material::gravel();
    if (name == "open_air")   return Material::openAir();

    juce::Logger::writeToLog ("MaterialResolver: unknown material '" + name + "', using drywall");
    return Material::drywall();
}

juce::StringArray MaterialResolver::getKnownNames()
{
    return { "concrete", "drywall", "wood_floor", "carpet", "curtain",
             "glass", "foliage", "gravel", "open_air" };
}
} // namespace Worldizer
