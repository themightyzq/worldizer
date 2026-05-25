#pragma once

#include <JuceHeader.h>
#include "../Shared/Constants.h"

namespace wz
{
/**
    Distance-dependent high-frequency rolloff (a simplified ITU-R P.676 air
    absorption model). Applied per reflection path during IR building.

    Parametrised by distance, temperature, and humidity. MVP uses sensible
    defaults; temperature/humidity are exposed in v2.

    Slice 1 implements the per-band attenuation coefficients.
*/
class AirAbsorption
{
public:
    AirAbsorption() = default;

    /** Per-band attenuation (linear gain, 0..1) for a path of the given length. */
    std::array<float, (size_t) Worldizer::kIRBands>
        attenuationForDistance (float metres,
                                float temperatureC = 20.0f,
                                float humidityPct  = 50.0f) const;
};
} // namespace wz
