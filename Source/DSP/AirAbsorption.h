#pragma once

#include <JuceHeader.h>
#include <array>
#include "../Model/Material.h"

namespace Worldizer
{
/**
    Distance-dependent atmospheric absorption. Simplified model (a lookup table
    of dB/100 m per octave band) standing in for the full ISO 9613-1 / ITU-R P.676
    formula, which is a v1.0 refinement. Temperature and humidity are accepted for
    API stability but ignored in this simplified version.
*/
class AirAbsorption
{
public:
    /** Absorption coefficient (dB/m) at a given band centre frequency. */
    static float getAbsorptionPerMeter (float frequencyHz,
                                         float temperatureCelsius,
                                         float relativeHumidity);

    /** Broadband amplitude factor (0..1) for a distance and band:
        10^(-absorption_dB_per_m * distance / 20). */
    static float getAmplitudeFactor (int band,
                                     float distanceMeters,
                                     float temperatureCelsius = 20.0f,
                                     float relativeHumidity = 50.0f);

    /** All 8 band amplitude factors for a given distance. */
    static std::array<float, Material::kNumBands>
        getAllBandFactors (float distanceMeters,
                           float temperatureCelsius = 20.0f,
                           float relativeHumidity = 50.0f);
};
} // namespace Worldizer
