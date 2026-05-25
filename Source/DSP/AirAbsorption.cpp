#include "AirAbsorption.h"
#include <cmath>

namespace Worldizer
{
namespace
{
    // Octave band centres (Hz) and rough absorption (dB per 100 m) at 20C / 50% RH.
    constexpr std::array<float, Material::kNumBands> kBandCentres
        { 62.5f, 125.0f, 250.0f, 500.0f, 1000.0f, 2000.0f, 4000.0f, 8000.0f };

    constexpr std::array<float, Material::kNumBands> kDbPer100m
        { 0.0f, 0.04f, 0.10f, 0.25f, 0.5f, 1.5f, 5.0f, 15.0f };

    float amplitudeFromDb (float dbPerMeter, float distanceMeters) noexcept
    {
        const float db = dbPerMeter * juce::jmax (0.0f, distanceMeters);
        return std::pow (10.0f, -db / 20.0f);
    }
}

float AirAbsorption::getAbsorptionPerMeter (float frequencyHz,
                                            float temperatureCelsius,
                                            float relativeHumidity)
{
    juce::ignoreUnused (temperatureCelsius, relativeHumidity);

    // Linear interpolation of the dB/100m table by frequency, then convert to dB/m.
    if (frequencyHz <= kBandCentres.front())
        return kDbPer100m.front() / 100.0f;
    if (frequencyHz >= kBandCentres.back())
        return kDbPer100m.back() / 100.0f;

    for (int i = 1; i < Material::kNumBands; ++i)
    {
        if (frequencyHz <= kBandCentres[(size_t) i])
        {
            const float f0 = kBandCentres[(size_t) (i - 1)];
            const float f1 = kBandCentres[(size_t) i];
            const float a0 = kDbPer100m[(size_t) (i - 1)];
            const float a1 = kDbPer100m[(size_t) i];
            const float frac = (frequencyHz - f0) / (f1 - f0);
            return (a0 + frac * (a1 - a0)) / 100.0f;
        }
    }

    return kDbPer100m.back() / 100.0f;
}

float AirAbsorption::getAmplitudeFactor (int band,
                                         float distanceMeters,
                                         float temperatureCelsius,
                                         float relativeHumidity)
{
    juce::ignoreUnused (temperatureCelsius, relativeHumidity);
    band = juce::jlimit (0, Material::kNumBands - 1, band);
    return amplitudeFromDb (kDbPer100m[(size_t) band] / 100.0f, distanceMeters);
}

std::array<float, Material::kNumBands>
    AirAbsorption::getAllBandFactors (float distanceMeters,
                                      float temperatureCelsius,
                                      float relativeHumidity)
{
    juce::ignoreUnused (temperatureCelsius, relativeHumidity);

    std::array<float, Material::kNumBands> factors {};
    for (int b = 0; b < Material::kNumBands; ++b)
        factors[(size_t) b] = amplitudeFromDb (kDbPer100m[(size_t) b] / 100.0f, distanceMeters);
    return factors;
}
} // namespace Worldizer
