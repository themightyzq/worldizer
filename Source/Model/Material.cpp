#include "Material.h"

namespace Worldizer
{
Material::Material (const juce::String& n,
                    std::array<float, kNumBands> a,
                    float s)
    : name (n), absorption (a), scattering (juce::jlimit (0.0f, 1.0f, s))
{
    for (auto& v : absorption)
        v = juce::jlimit (0.0f, 1.0f, v);
}

std::array<float, Material::kNumBands> Material::getReflection() const noexcept
{
    std::array<float, kNumBands> r {};
    for (int b = 0; b < kNumBands; ++b)
        r[(size_t) b] = juce::jlimit (0.0f, 1.0f, 1.0f - absorption[(size_t) b]);
    return r;
}

// Per-octave absorption values (62.5, 125, 250, 500, 1k, 2k, 4k, 8k Hz).
// Plausible figures from published architectural-acoustics tables — refined in Slice 9.

Material Material::concrete()
{
    return { "concrete", { 0.01f, 0.01f, 0.02f, 0.02f, 0.02f, 0.03f, 0.04f, 0.05f }, 0.05f };
}

Material Material::drywall()
{
    return { "drywall", { 0.20f, 0.29f, 0.10f, 0.05f, 0.04f, 0.07f, 0.09f, 0.10f }, 0.10f };
}

Material Material::woodFloor()
{
    return { "wood_floor", { 0.15f, 0.15f, 0.11f, 0.10f, 0.07f, 0.06f, 0.07f, 0.07f }, 0.10f };
}

Material Material::carpet()
{
    return { "carpet", { 0.02f, 0.02f, 0.06f, 0.14f, 0.37f, 0.60f, 0.65f, 0.65f }, 0.30f };
}

Material Material::curtain()
{
    return { "curtain", { 0.10f, 0.14f, 0.35f, 0.55f, 0.72f, 0.70f, 0.65f, 0.65f }, 0.50f };
}

Material Material::glass()
{
    return { "glass", { 0.05f, 0.04f, 0.03f, 0.03f, 0.02f, 0.02f, 0.02f, 0.02f }, 0.02f };
}

Material Material::foliage()
{
    return { "foliage", { 0.03f, 0.06f, 0.11f, 0.17f, 0.27f, 0.31f, 0.35f, 0.35f }, 0.90f };
}

Material Material::gravel()
{
    return { "gravel", { 0.05f, 0.10f, 0.20f, 0.30f, 0.45f, 0.55f, 0.60f, 0.60f }, 0.70f };
}

Material Material::openAir()
{
    return { "open_air", { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f }, 0.0f };
}

Material Material::brick()
{
    return { "brick", { 0.03f, 0.03f, 0.03f, 0.03f, 0.04f, 0.05f, 0.07f, 0.07f }, 0.25f };
}

Material Material::marble()
{
    return { "marble", { 0.01f, 0.01f, 0.01f, 0.01f, 0.01f, 0.02f, 0.02f, 0.02f }, 0.02f };
}

Material Material::tile()
{
    return { "tile", { 0.02f, 0.02f, 0.01f, 0.01f, 0.02f, 0.02f, 0.02f, 0.03f }, 0.05f };
}

Material Material::plaster()
{
    return { "plaster", { 0.20f, 0.14f, 0.10f, 0.06f, 0.05f, 0.04f, 0.03f, 0.03f }, 0.08f };
}

Material Material::acousticTile()
{
    return { "acoustic_tile", { 0.30f, 0.29f, 0.55f, 0.75f, 0.85f, 0.90f, 0.85f, 0.80f }, 0.15f };
}

Material Material::metal()
{
    return { "metal", { 0.20f, 0.14f, 0.10f, 0.06f, 0.05f, 0.05f, 0.05f, 0.05f }, 0.05f };
}

Material Material::woodPanel()
{
    return { "wood_panel", { 0.35f, 0.30f, 0.25f, 0.17f, 0.10f, 0.08f, 0.07f, 0.07f }, 0.10f };
}

Material Material::upholstery()
{
    return { "upholstery", { 0.15f, 0.19f, 0.37f, 0.56f, 0.67f, 0.61f, 0.59f, 0.58f }, 0.40f };
}

Material Material::asphalt()
{
    return { "asphalt", { 0.02f, 0.02f, 0.03f, 0.03f, 0.03f, 0.03f, 0.02f, 0.02f }, 0.15f };
}

Material Material::grass()
{
    return { "grass", { 0.05f, 0.11f, 0.26f, 0.60f, 0.69f, 0.92f, 0.99f, 0.99f }, 0.60f };
}

Material Material::water()
{
    return { "water", { 0.01f, 0.01f, 0.01f, 0.01f, 0.02f, 0.02f, 0.03f, 0.03f }, 0.05f };
}
} // namespace Worldizer
