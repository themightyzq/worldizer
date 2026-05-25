#include "IRBuilder.h"
#include "AirAbsorption.h"
#include <cmath>

namespace Worldizer
{
juce::AudioBuffer<float> IRBuilder::build (const RayTracer::Result& tr,
                                           const Settings& settings) const
{
    const int sr     = tr.sampleRate;
    const int length = juce::jmax (1, juce::jmin (settings.maxLengthSamples, tr.numBins));

    juce::AudioBuffer<float> ir (1, length);
    ir.clear();
    auto* out = ir.getWritePointer (0);

    constexpr float c = 343.0f; // matches RayTracer::Settings::speedOfSound default
    const int   numRays   = juce::jmax (1, tr.numRays);

    // Calibrate reflection amplitude so a reflection with the same geometry as the
    // direct path would land at 1/distance — the mic capture cross-section gives a
    // histogram amplitude of micRadius/(2*dist), so multiply by 2/micRadius.
    const float reflScale = 2.0f / juce::jmax (0.01f, tr.micRadius);

    // Deterministic random signs => the IR is reproducible for a given histogram.
    juce::Random rng (20240517);

    // --- Reflections from the per-band energy histogram ---
    for (int bin = 0; bin < length; ++bin)
    {
        const float dist = (float) bin / (float) sr * c;

        std::array<float, Material::kNumBands> air;
        if (settings.applyAirAbsorption)
            air = AirAbsorption::getAllBandFactors (dist, settings.temperatureCelsius, settings.relativeHumidity);
        else
            air.fill (1.0f);

        float energySum = 0.0f;
        for (int b = 0; b < Material::kNumBands; ++b)
        {
            const float e = tr.histogram[(size_t) b][(size_t) bin];
            energySum += e * air[(size_t) b] * air[(size_t) b]; // air absorption in the energy domain
        }

        if (energySum > 0.0f)
        {
            const float amp  = reflScale * std::sqrt (energySum / (float) numRays);
            const float sign = (rng.nextFloat() < 0.5f) ? -1.0f : 1.0f;
            out[bin] += amp * sign;
        }
    }

    // --- Direct sound (deterministic impulse) ---
    if (settings.includeDirect && tr.directVisible)
    {
        const int dbin = (int) std::lround (tr.directArrivalTime * (float) sr);
        if (dbin >= 0 && dbin < length)
        {
            const float dist = juce::jmax (0.5f, tr.directDistance);
            float amp = (1.0f / dist) * settings.directGainCompensation;

            if (settings.applyAirAbsorption)
            {
                const auto air = AirAbsorption::getAllBandFactors (dist, settings.temperatureCelsius, settings.relativeHumidity);
                float mean = 0.0f;
                for (auto a : air)
                    mean += a;
                amp *= mean / (float) Material::kNumBands; // broadband approximation for a single sample
            }

            out[dbin] += amp;
        }
    }

    // --- Peak-normalise to -1 dBFS ---
    float peak = 0.0f;
    for (int i = 0; i < length; ++i)
        peak = juce::jmax (peak, std::abs (out[i]));

    if (peak > 0.0f)
        ir.applyGain (juce::Decibels::decibelsToGain (-1.0f) / peak);

    return ir;
}
} // namespace Worldizer
