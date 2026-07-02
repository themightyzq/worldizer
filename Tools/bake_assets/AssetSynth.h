#pragma once

#include <JuceHeader.h>
#include <cmath>
#include <vector>

/**
    Synthesis recipes for the shipped placeholder assets: speaker character IRs,
    mic character IRs, and ambient room-tone beds. Shared (header-only) by the
    BakeAssets tool and CharacterTest so the test can assert spectral properties
    without depending on baked files.

    These are PLACEHOLDERS: plausible filter-chain characters standing in until
    real recordings replace the WAVs (Slice 9 physical work). Each recipe is a
    cascade of RBJ biquads plus, for cabinet-y speakers, a couple of short early
    reflection taps (panel/box resonance). Deterministic (seeded noise) so bakes
    are bit-identical.
*/
namespace WorldizerAssetSynth
{
using Buffer = juce::AudioBuffer<float>;

//==============================================================================
// Small RBJ biquad chain applied in place to a mono buffer.
struct FilterChain
{
    std::vector<juce::dsp::IIR::Coefficients<float>::Ptr> stages;

    void highPass (double sr, float f, float q = 0.707f, int order2Count = 1)
    {
        for (int i = 0; i < order2Count; ++i)
            stages.push_back (juce::dsp::IIR::Coefficients<float>::makeHighPass (sr, f, q));
    }
    void lowPass (double sr, float f, float q = 0.707f, int order2Count = 1)
    {
        for (int i = 0; i < order2Count; ++i)
            stages.push_back (juce::dsp::IIR::Coefficients<float>::makeLowPass (sr, f, q));
    }
    void peak (double sr, float f, float q, float gainDb)
    {
        stages.push_back (juce::dsp::IIR::Coefficients<float>::makePeakFilter (
            sr, f, q, juce::Decibels::decibelsToGain (gainDb)));
    }
    void highShelf (double sr, float f, float q, float gainDb)
    {
        stages.push_back (juce::dsp::IIR::Coefficients<float>::makeHighShelf (
            sr, f, q, juce::Decibels::decibelsToGain (gainDb)));
    }
    void lowShelf (double sr, float f, float q, float gainDb)
    {
        stages.push_back (juce::dsp::IIR::Coefficients<float>::makeLowShelf (
            sr, f, q, juce::Decibels::decibelsToGain (gainDb)));
    }

    void applyTo (Buffer& mono) const
    {
        for (const auto& coeffs : stages)
        {
            juce::dsp::IIR::Filter<float> filter (coeffs);
            juce::dsp::ProcessSpec spec { 48000.0, (juce::uint32) mono.getNumSamples(), 1 };
            filter.prepare (spec);
            juce::dsp::AudioBlock<float> block (mono);
            filter.process (juce::dsp::ProcessContextReplacing<float> (block));
        }
    }
};

// Adds a delayed copy of the buffer onto itself (a cabinet/panel reflection tap).
inline void addReflectionTap (Buffer& mono, double sr, float delayMs, float gainDb)
{
    const int delay = juce::jmax (1, (int) std::lround (sr * delayMs * 0.001));
    const float g   = juce::Decibels::decibelsToGain (gainDb);
    auto* d = mono.getWritePointer (0);
    const int n = mono.getNumSamples();
    for (int i = n - 1; i >= delay; --i)   // backwards: taps read pre-tap samples only
        d[i] += g * d[i - delay];
}

// Raised-cosine fade-out over the last `ms`, and peak-normalise to `peakDb`.
inline void finishIR (Buffer& mono, double sr, float fadeMs, float peakDb)
{
    auto* d = mono.getWritePointer (0);
    const int n    = mono.getNumSamples();
    const int fade = juce::jmin (n, (int) (sr * fadeMs * 0.001));
    for (int i = 0; i < fade; ++i)  // i counts back from the end: gain 0 at the last sample
        d[n - 1 - i] *= 0.5f * (1.0f - std::cos (juce::MathConstants<float>::pi * (float) i / (float) fade));

    float peak = 0.0f;
    for (int i = 0; i < n; ++i)
        peak = juce::jmax (peak, std::abs (d[i]));
    if (peak > 0.0f)
        mono.applyGain (juce::Decibels::decibelsToGain (peakDb) / peak);
}

// Fade-out, then normalise to UNIT ENERGY (sum of h^2 == 1). Character IRs use
// this so the runtime loads them with Normalise::no and they pass at unity
// loudness — exactly matching the 1-sample "none" delta, whose energy is 1.
// (JUCE's own Normalise::yes scales to 0.125/sqrt(E): even a unit delta becomes
// a fixed -18 dB pad, which is why the character path avoids it.)
inline void finishCharacterIR (Buffer& mono, double sr, float fadeMs)
{
    auto* d = mono.getWritePointer (0);
    const int n    = mono.getNumSamples();
    const int fade = juce::jmin (n, (int) (sr * fadeMs * 0.001));
    for (int i = 0; i < fade; ++i)
        d[n - 1 - i] *= 0.5f * (1.0f - std::cos (juce::MathConstants<float>::pi * (float) i / (float) fade));

    double energy = 0.0;
    for (int i = 0; i < n; ++i)
        energy += (double) d[i] * (double) d[i];
    if (energy > 0.0)
        mono.applyGain ((float) (1.0 / std::sqrt (energy)));
}

//==============================================================================
/** Speaker character IR for a CharacterLibrary speaker id ("none" -> empty). */
inline Buffer synthesizeSpeakerIR (const juce::String& id, double sr)
{
    const int len = (int) (sr * 4096.0 / 48000.0); // ~85 ms
    Buffer ir (1, len);
    ir.clear();
    ir.setSample (0, 0, 1.0f);

    FilterChain fc;

    if (id == "spk_fullrange_pa")
    {
        fc.highPass (sr, 55.0f, 0.8f);
        fc.peak     (sr, 2000.0f, 1.2f, 2.0f);
        fc.lowPass  (sr, 16000.0f);
        fc.applyTo (ir);
        addReflectionTap (ir, sr, 1.1f, -16.0f);
        addReflectionTap (ir, sr, 2.7f, -22.0f);
    }
    else if (id == "spk_guitar_cab")
    {
        fc.highPass (sr, 75.0f, 1.0f);
        fc.peak     (sr, 95.0f, 1.4f, 4.0f);
        fc.peak     (sr, 800.0f, 1.0f, -4.0f);
        fc.peak     (sr, 2600.0f, 1.8f, 5.0f);
        fc.lowPass  (sr, 5000.0f, 0.9f, 2);
        fc.applyTo (ir);
        addReflectionTap (ir, sr, 0.9f, -12.0f);
    }
    else if (id == "spk_transistor_radio")
    {
        fc.highPass (sr, 280.0f, 0.707f, 2);
        fc.peak     (sr, 1600.0f, 2.0f, 6.0f);
        fc.peak     (sr, 3100.0f, 6.0f, 4.0f);
        fc.lowPass  (sr, 4200.0f, 0.707f, 2);
        fc.applyTo (ir);
    }
    else if (id == "spk_telephone")
    {
        fc.highPass (sr, 300.0f, 0.707f, 2);
        fc.peak     (sr, 2000.0f, 1.3f, 5.0f);
        fc.peak     (sr, 1000.0f, 1.5f, 2.0f);
        fc.lowPass  (sr, 3400.0f, 0.707f, 2);
        fc.applyTo (ir);
    }
    else if (id == "spk_megaphone")
    {
        fc.highPass (sr, 400.0f, 0.707f, 2);
        fc.peak     (sr, 1100.0f, 2.5f, 9.0f);
        fc.peak     (sr, 2200.0f, 4.0f, 5.0f);
        fc.lowPass  (sr, 3800.0f);
        fc.applyTo (ir);
        addReflectionTap (ir, sr, 0.4f, -8.0f); // horn throat
    }
    else if (id == "spk_vintage_tv")
    {
        fc.highPass (sr, 120.0f);
        fc.peak     (sr, 700.0f, 1.2f, 3.0f);
        fc.peak     (sr, 3500.0f, 3.0f, -5.0f);
        fc.lowPass  (sr, 6000.0f);
        fc.applyTo (ir);
        addReflectionTap (ir, sr, 1.6f, -14.0f); // wooden console body
    }
    else if (id == "spk_intercom")
    {
        // Deliberately DARKER and boxier than the telephone (which peaks at 2 kHz):
        // a low 1 kHz honk and a steep 2.2 kHz ceiling keep the two audibly distinct.
        fc.highPass (sr, 450.0f, 0.707f, 2);
        fc.peak     (sr, 1000.0f, 3.0f, 9.0f);
        fc.lowPass  (sr, 2200.0f, 0.9f, 2);
        fc.applyTo (ir);
        addReflectionTap (ir, sr, 0.6f, -10.0f); // wall panel
    }
    else if (id == "spk_car_speaker")
    {
        fc.highPass (sr, 90.0f, 1.0f);
        fc.peak     (sr, 150.0f, 1.2f, 4.0f);
        fc.peak     (sr, 1200.0f, 1.0f, -6.0f);
        fc.lowPass  (sr, 7500.0f);
        fc.applyTo (ir);
        addReflectionTap (ir, sr, 2.2f, -13.0f); // door panel
    }
    else
        return Buffer {}; // "none" / unknown

    finishCharacterIR (ir, sr, 10.0f);
    return ir;
}

/** Mic character IR for a CharacterLibrary mic id ("none" -> empty). */
inline Buffer synthesizeMicIR (const juce::String& id, double sr)
{
    const int len = (int) (sr * 2048.0 / 48000.0); // ~43 ms
    Buffer ir (1, len);
    ir.clear();
    ir.setSample (0, 0, 1.0f);

    FilterChain fc;

    if (id == "mic_studio_condenser")
    {
        fc.highPass  (sr, 30.0f);
        fc.highShelf (sr, 12000.0f, 0.707f, 1.5f);
        fc.applyTo (ir);
    }
    else if (id == "mic_dynamic_stage")
    {
        // Warm proximity low-mids + earlier top rolloff: keeps it clearly apart
        // from the lavalier (which is lean below 700 Hz with an airier top).
        fc.highPass (sr, 90.0f);
        fc.peak     (sr, 160.0f, 1.0f, 4.0f);   // proximity warmth
        fc.peak     (sr, 4000.0f, 1.5f, 4.0f);  // presence
        fc.lowPass  (sr, 12000.0f);
        fc.applyTo (ir);
    }
    else if (id == "mic_ribbon_vintage")
    {
        fc.highPass  (sr, 40.0f);
        fc.lowShelf  (sr, 120.0f, 0.707f, 2.0f);
        fc.highShelf (sr, 8000.0f, 0.707f, -6.0f);
        fc.lowPass   (sr, 9000.0f, 0.6f);
        fc.applyTo (ir);
    }
    else if (id == "mic_lavalier")
    {
        fc.highPass (sr, 150.0f);
        fc.peak     (sr, 600.0f, 1.2f, -5.0f);  // chest shadow
        fc.peak     (sr, 7000.0f, 1.4f, 4.5f);  // consonant crisping
        fc.lowPass  (sr, 17000.0f);
        fc.applyTo (ir);
    }
    else if (id == "mic_carbon_telephone")
    {
        fc.highPass (sr, 400.0f, 0.707f, 2);
        fc.peak     (sr, 900.0f, 3.0f, 5.0f);
        fc.peak     (sr, 1800.0f, 4.0f, 8.0f);
        fc.lowPass  (sr, 3000.0f, 0.707f, 2);
        fc.applyTo (ir);
    }
    else if (id == "mic_contact")
    {
        fc.highPass (sr, 60.0f);
        fc.peak     (sr, 450.0f, 8.0f, 10.0f);  // body resonances, no air sound
        fc.peak     (sr, 1300.0f, 6.0f, 7.0f);
        fc.peak     (sr, 2900.0f, 7.0f, 5.0f);
        fc.lowPass  (sr, 6000.0f);
        fc.applyTo (ir);
    }
    else
        return Buffer {};

    finishCharacterIR (ir, sr, 8.0f);
    return ir;
}

//==============================================================================
/** Ambient room-tone bed for a CharacterLibrary room-tone id. Mono, `seconds`
    long, seamlessly loopable BY CONSTRUCTION: all slow modulators complete an
    integer number of cycles over the loop and the seam is crossfade-blended. */
inline Buffer synthesizeRoomTone (const juce::String& id, double sr, double seconds = 8.0)
{
    const int n = (int) (sr * seconds);
    Buffer bed (1, n);
    bed.clear();
    auto* d = bed.getWritePointer (0);

    juce::Random rng (0x5EED + id.hashCode());
    const double twoPi = 2.0 * juce::MathConstants<double>::pi;

    // Integer-cycle LFO over the loop => value at sample n == value at sample 0.
    auto lfo = [&] (int i, double cyclesOverLoop, double phase = 0.0)
    {
        return std::sin (twoPi * (cyclesOverLoop * (double) i / (double) n) + phase);
    };
    // Sum of integer-cycle sine partials (hums). Cycles are rounded to a
    // multiple of 32 (= loopLength / seamLength): the seam blend mixes d[i]
    // with d[i + n - xf], whose phase offset is 2*pi*cycles*(xf/n) = cycles/32
    // turns — a multiple of 32 makes every partial phase-ALIGNED across the
    // seam (a 50 Hz hum at 400 cycles would land 180 degrees out and cancel
    // mid-seam every loop).
    auto addHum = [&] (double freq, float amp, int partials, float partialDecay)
    {
        const double cycles = 32.0 * std::round (freq * seconds / 32.0);
        for (int p = 1; p <= partials; ++p)
        {
            const float a = amp * std::pow (partialDecay, (float) (p - 1));
            for (int i = 0; i < n; ++i)
                d[i] += a * (float) std::sin (twoPi * cycles * (double) p * (double) i / (double) n);
        }
    };
    auto fillNoise = [&] (float amp)
    {
        for (int i = 0; i < n; ++i)
            d[i] += amp * (rng.nextFloat() * 2.0f - 1.0f);
    };

    FilterChain fc;

    if (id == "amb_room_hvac")
    {
        fillNoise (0.5f);
        fc.lowPass (sr, 500.0f, 0.707f, 2);
        fc.peak    (sr, 90.0f, 1.0f, 6.0f);
        fc.applyTo (bed);
        addHum (60.0, 0.06f, 3, 0.4f);
        for (int i = 0; i < n; ++i)  // slow breathing, 3 cycles over the loop
            d[i] *= 1.0f + 0.12f * (float) lfo (i, 3.0);
    }
    else if (id == "amb_small_room")
    {
        fillNoise (0.25f);
        fc.lowPass (sr, 2000.0f, 0.707f, 2);
        fc.highPass (sr, 40.0f);
        fc.applyTo (bed);
        addHum (50.0, 0.02f, 2, 0.3f);
    }
    else if (id == "amb_outdoor_air")
    {
        fillNoise (0.5f);
        fc.highPass (sr, 200.0f);
        fc.lowPass  (sr, 2000.0f, 0.707f, 2);
        fc.applyTo (bed);
        for (int i = 0; i < n; ++i)  // wind swells: two incommensurate-feeling integer LFOs
            d[i] *= 1.0f + 0.35f * (float) lfo (i, 2.0) * (float) lfo (i, 5.0, 1.3);
    }
    else if (id == "amb_city_rumble")
    {
        fillNoise (0.6f);
        fc.lowPass (sr, 150.0f, 0.707f, 2);
        fc.peak    (sr, 55.0f, 1.2f, 5.0f);
        fc.applyTo (bed);

        Buffer murmur (1, n);
        murmur.clear();
        auto* m = murmur.getWritePointer (0);
        for (int i = 0; i < n; ++i)
            m[i] = 0.15f * (rng.nextFloat() * 2.0f - 1.0f);
        FilterChain mf;
        mf.highPass (sr, 300.0f);
        mf.lowPass  (sr, 800.0f, 0.707f, 2);
        mf.applyTo (murmur);
        for (int i = 0; i < n; ++i)
            d[i] += m[i] * (1.0f + 0.5f * (float) lfo (i, 4.0));
    }
    else if (id == "amb_fluorescent")
    {
        addHum (120.0, 0.12f, 8, 0.62f);
        Buffer hiss (1, n);
        hiss.clear();
        auto* h = hiss.getWritePointer (0);
        for (int i = 0; i < n; ++i)
            h[i] = 0.05f * (rng.nextFloat() * 2.0f - 1.0f);
        FilterChain hf;
        hf.highPass (sr, 3000.0f);
        hf.applyTo (hiss);
        for (int i = 0; i < n; ++i)
            d[i] += h[i];
    }
    else if (id == "amb_tape_hiss")
    {
        fillNoise (0.3f);
        fc.highPass  (sr, 100.0f);
        fc.highShelf (sr, 6000.0f, 0.707f, 4.0f);
        fc.applyTo (bed);
    }
    else
        return Buffer {};

    // Blend the seam: crossfade the last 250 ms into the start, then trim it off.
    // EQUAL-POWER law: head and tail are uncorrelated noise, so a linear blend
    // would dip -3 dB at the seam midpoint — a periodic "breath" every loop.
    // (The filters' transient tails at the start are also hidden by this blend
    // region being replaced by steady-state signal.)
    const int xf = juce::jmin (n / 4, (int) (sr * 0.25));
    for (int i = 0; i < xf; ++i)
    {
        const float t = (float) i / (float) xf;
        d[i] = d[i] * std::sqrt (t) + d[n - xf + i] * std::sqrt (1.0f - t);
    }
    bed.setSize (1, n - xf, true);

    // Beds sit far under the program material: normalise to -20 dBFS peak.
    finishIR (bed, sr, 0.0f, -20.0f);
    return bed;
}
} // namespace WorldizerAssetSynth
