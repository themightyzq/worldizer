#pragma once

#include <JuceHeader.h>
#include <array>

namespace Worldizer
{
/**
    Frequency-dependent acoustic material properties.

    Absorption is per octave band (8 bands matching kIRBands). Scattering is
    broadband (0.0 = fully specular, 1.0 = fully diffuse). The static factory
    methods produce plausible values based on published acoustic tables; the
    canonical, measured library arrives in Slice 9 (Resources/Materials/materials.json).
*/
class Material
{
public:
    static constexpr int kNumBands = 8;
    // Band centre frequencies (Hz): 62.5, 125, 250, 500, 1000, 2000, 4000, 8000

    Material() = default;
    Material (const juce::String& name,
              std::array<float, kNumBands> absorption,
              float scattering);

    const juce::String& getName() const noexcept                       { return name; }
    float getAbsorption (int band) const noexcept                      { return absorption[(size_t) band]; }
    const std::array<float, kNumBands>& getAbsorption() const noexcept { return absorption; }
    float getScattering() const noexcept                               { return scattering; }

    /** Returns the reflection coefficient (1 - absorption) per band, clamped to [0, 1]. */
    std::array<float, kNumBands> getReflection() const noexcept;

    // === Standard materials (published octave-band absorption tables; the
    //     62.5 Hz and 8 kHz ends are extrapolated from the 125–4k data) ===
    static Material concrete();    // very low absorption, low scattering
    static Material drywall();     // mid absorption (bass panel loss), low scattering
    static Material woodFloor();   // mid-low absorption, low scattering
    static Material carpet();      // high absorption above 500Hz, medium scattering
    static Material curtain();     // very high absorption, high scattering
    static Material glass();       // very low absorption, very low scattering
    static Material foliage();     // mid absorption, very high scattering
    static Material gravel();      // mid absorption, high scattering
    static Material openAir();     // 1.0 absorption (ray dies on contact) — "no surface here"
    static Material brick();       // unglazed brick: hard, mortar-joint scatter
    static Material marble();      // polished stone: hardest interior surface
    static Material tile();        // glazed ceramic: bathroom/kitchen shine
    static Material plaster();     // plaster on lath: hard with LF panel loss
    static Material acousticTile();// suspended mineral-fibre ceiling: very dead
    static Material metal();       // sheet metal panel: hard top, LF panel absorption
    static Material woodPanel();   // panelling over airspace: warm LF soak
    static Material upholstery();  // padded seating / fabric surfaces
    static Material asphalt();     // road surface: hard, slightly textured
    static Material grass();       // lawn/turf: soft ground, HF eaten
    static Material water();       // water surface: near-perfect reflector

private:
    juce::String name { "default" };
    std::array<float, kNumBands> absorption { { 0.1f, 0.1f, 0.1f, 0.1f, 0.1f, 0.1f, 0.1f, 0.1f } };
    float scattering = 0.1f;
};
} // namespace Worldizer
