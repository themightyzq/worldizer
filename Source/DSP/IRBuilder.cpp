#include "IRBuilder.h"
#include "AirAbsorption.h"
#include <cmath>
#include <vector>

namespace Worldizer
{
namespace
{
    // Octave band centre frequencies (Hz), matching Material / AirAbsorption.
    constexpr std::array<float, Material::kNumBands> kBandCentres
        { 62.5f, 125.0f, 250.0f, 500.0f, 1000.0f, 2000.0f, 4000.0f, 8000.0f };

    // Per-section Q for the octave bandpass. Two identical sections are cascaded
    // (=> 24 dB/oct skirts); Q ~= 0.9 puts the cascade's -3 dB points at the octave
    // edges, so adjacent bands cross near -3 dB and the summed magnitude stays
    // roughly flat. Steeper skirts keep each band's air-absorption decay independent,
    // so the tail darkens naturally (HF dies away before LF).
    constexpr float kOctaveQ = 0.9f;
}

juce::AudioBuffer<float> IRBuilder::build (const RayTracer::Result& tr,
                                           const Settings& settings) const
{
    const int   sr     = tr.sampleRate;
    const int   length = juce::jmax (1, juce::jmin (settings.maxLengthSamples, tr.numBins));
    constexpr float c  = 343.0f; // matches RayTracer::Settings::speedOfSound default
    const int   numRays = juce::jmax (1, tr.numRays);

    // Calibrate reverb energy to the direct sound's scale (mic capture cross-section
    // => amplitude micRadius/(2*dist), so energy factor (2/micRadius)^2 / numRays).
    const float micR      = juce::jmax (0.01f, tr.micRadius);
    const float energyCal = (4.0f / (micR * micR)) / (float) numRays;

    // Independent white-noise carrier per band (filled inside the loop, deterministic).
    // Independent noise => overlapping steep bands sum incoherently, keeping the
    // summed magnitude flat with no comb colouration at the crossover frequencies.
    juce::Random rng (20240517);
    std::vector<float> noise ((size_t) length);

    // Energy-envelope smoothing: a centred moving average whose window GROWS with
    // time. Early reflections keep their detail (short window); the sparse, diffuse
    // late tail is smoothed heavily (long window) so it renders as a continuous
    // decay instead of intermittent "sparks" from individual late ray hits.
    const int   halfMin = juce::jmax (1, (int) std::round (settings.envelopeMs * 0.001f * (float) sr) / 2);
    const int   halfMax = juce::jmax (halfMin, (int) (0.05f * (float) sr)); // up to ~100 ms window
    const float halfGrowth = 0.025f; // half-window ~= 0.025 * t  (=> window ~= 0.05 * t)

    std::vector<float>  out      ((size_t) length, 0.0f);
    std::vector<float>  bandNoise((size_t) length);
    std::vector<double> prefix   ((size_t) length + 1); // prefix sum of band energy

    const juce::dsp::ProcessSpec spec { (double) sr, (juce::uint32) length, 1 };

    for (int b = 0; b < Material::kNumBands; ++b)
    {
        // 1) Air-weighted energy per sample for this band, and its prefix sum.
        prefix[0] = 0.0;
        for (int t = 0; t < length; ++t)
        {
            float e = tr.histogram[(size_t) b][(size_t) t] * energyCal;
            if (settings.applyAirAbsorption)
            {
                const float dist = (float) t / (float) sr * c;
                const float af = AirAbsorption::getAmplitudeFactor (b, dist,
                                                                    settings.temperatureCelsius,
                                                                    settings.relativeHumidity);
                e *= af * af; // air absorption acts in the energy domain
            }
            prefix[(size_t) t + 1] = prefix[(size_t) t] + (double) e;
        }

        if (prefix[(size_t) length] <= 0.0)
            continue; // no energy in this band

        // 2) Fresh independent noise, band-limited by two cascaded biquads
        //    (24 dB/oct octave band), normalised to unit RMS.
        for (int t = 0; t < length; ++t)
            noise[(size_t) t] = rng.nextFloat() * 2.0f - 1.0f;

        auto coeffs = juce::dsp::IIR::Coefficients<float>::makeBandPass ((double) sr, kBandCentres[(size_t) b], kOctaveQ);
        juce::dsp::IIR::Filter<float> section1, section2;
        section1.coefficients = coeffs;
        section2.coefficients = coeffs;
        section1.prepare (spec);
        section2.prepare (spec);
        section1.reset();
        section2.reset();

        double sumSq = 0.0;
        for (int t = 0; t < length; ++t)
        {
            const float f = section2.processSample (section1.processSample (noise[(size_t) t]));
            bandNoise[(size_t) t] = f;
            sumSq += (double) f * (double) f;
        }
        const float rms    = (float) std::sqrt (sumSq / (double) length);
        const float invRms = rms > 1.0e-9f ? 1.0f / rms : 0.0f;

        // 3) Modulate by sqrt(energy density) and accumulate.
        for (int t = 0; t < length; ++t)
        {
            const int half = juce::jlimit (halfMin, halfMax, (int) (halfGrowth * (float) t));
            const int lo = juce::jmax (0, t - half);
            const int hi = juce::jmin (length, t + half + 1);
            const double windowEnergy = prefix[(size_t) hi] - prefix[(size_t) lo];
            const float density = (float) (windowEnergy / (double) (hi - lo)); // energy per sample
            const float gain    = std::sqrt (juce::jmax (0.0f, density));
            out[(size_t) t] += bandNoise[(size_t) t] * invRms * gain;
        }
    }

    juce::AudioBuffer<float> ir (1, length);
    ir.clear();
    auto* o = ir.getWritePointer (0);
    for (int t = 0; t < length; ++t)
        o[t] = out[(size_t) t];

    // Direct sound: clean broadband impulse (the dry signal arriving undeflected).
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
                amp *= mean / (float) Material::kNumBands;
            }

            o[dbin] += amp;
        }
    }

    // Short fade-out so the IR ends cleanly (no abrupt truncation click).
    const int fade = juce::jmin (length, (int) (0.02f * (float) sr));
    for (int i = 0; i < fade; ++i)
        o[length - 1 - i] *= (float) (fade - i) / (float) fade;

    // Peak-normalise to -1 dBFS.
    float peak = 0.0f;
    for (int t = 0; t < length; ++t)
        peak = juce::jmax (peak, std::abs (o[t]));

    if (peak > 0.0f)
        ir.applyGain (juce::Decibels::decibelsToGain (-1.0f) / peak);

    return ir;
}
} // namespace Worldizer
