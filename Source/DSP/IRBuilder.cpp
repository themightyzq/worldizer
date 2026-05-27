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

    // Crossfade from ray-traced to synthesised tail starts at this multiple of the
    // estimated RT60. The dense, reliable early/mid reflections (up to ~1x RT60) are
    // kept; the sparse, statistically-thin late portion beyond it is replaced by a
    // smooth synthesised decay. (Deviation from the slice prompt's 1.5x: our test
    // scenes' RT60s fit inside the 4 s trace, so 1.5x would leave almost the whole
    // sparse tail in place. 1.0x hands the unreliable tail to synthesis — the actual
    // Slice 1 defect being fixed. Tunable; see Slice 4.5 retrospective.)
    constexpr float kCrossfadeRT60Factor = 1.0f;

    constexpr float kSpeedOfSound = 343.0f; // matches RayTracer::Settings::speedOfSound

    // Fills buf with its cascaded-octave-bandpassed self, normalised to unit RMS.
    void bandpassToUnitRms (std::vector<float>& buf, int band, double sr)
    {
        if (buf.empty())
            return;

        auto coeffs = juce::dsp::IIR::Coefficients<float>::makeBandPass (sr, kBandCentres[(size_t) band], kOctaveQ);
        juce::dsp::IIR::Filter<float> s1, s2;
        s1.coefficients = coeffs;
        s2.coefficients = coeffs;
        const juce::dsp::ProcessSpec spec { sr, (juce::uint32) buf.size(), 1 };
        s1.prepare (spec); s2.prepare (spec);
        s1.reset();        s2.reset();

        double sumSq = 0.0;
        for (auto& x : buf) { x = s2.processSample (s1.processSample (x)); sumSq += (double) x * (double) x; }

        const float rms = (float) std::sqrt (sumSq / (double) buf.size());
        const float inv = rms > 1.0e-9f ? 1.0f / rms : 0.0f;
        for (auto& x : buf) x *= inv;
    }

    // Least-squares slope (dB per second) of a per-sample energy array over
    // [fromBin, toBin), aggregated into ~20 ms blocks in the log domain. Returns
    // false if there is too little non-silent data to fit.
    bool fitDecaySlope (const std::vector<float>& energy, int fromBin, int toBin,
                        int sr, float& slopeDbPerSecOut)
    {
        const int block = juce::jmax (1, sr / 50); // 20 ms blocks
        std::vector<double> ts, dbs;
        for (int s = juce::jmax (0, fromBin); s + block <= toBin; s += block)
        {
            double sum = 0.0;
            for (int i = 0; i < block; ++i)
                sum += (double) energy[(size_t) (s + i)];
            const double dens = sum / (double) block;
            if (dens <= 1.0e-18)
                continue;
            ts.push_back ((double) (s + block / 2) / (double) sr);
            dbs.push_back (10.0 * std::log10 (dens));
        }

        if (ts.size() < 4)
            return false;

        const double n = (double) ts.size();
        double sx = 0, sy = 0, sxx = 0, sxy = 0;
        for (size_t i = 0; i < ts.size(); ++i)
        {
            sx += ts[i]; sy += dbs[i]; sxx += ts[i] * ts[i]; sxy += ts[i] * dbs[i];
        }
        const double denom = n * sxx - sx * sx;
        if (std::abs (denom) < 1.0e-12)
            return false;

        slopeDbPerSecOut = (float) ((n * sxy - sx * sy) / denom);
        return true;
    }
}

juce::AudioBuffer<float> IRBuilder::build (const RayTracer::Result& tr,
                                           const Settings& settings) const
{
    const int   sr      = tr.sampleRate;
    constexpr float c   = kSpeedOfSound;
    const int   numRays = juce::jmax (1, tr.numRays);
    const int   numBins = juce::jmax (1, tr.numBins);

    // Calibrate reverb energy to the direct sound's scale (mic capture cross-section
    // => amplitude micRadius/(2*dist), so energy factor (2/micRadius)^2 / numRays).
    const float micR      = juce::jmax (0.01f, tr.micRadius);
    const float energyCal = (4.0f / (micR * micR)) / (float) numRays;

    // The room response is stored RELATIVE to the direct sound: the direct sits at
    // sample 0, and a reflection arriving at absolute time t lands at output bin
    // (t - directOffset). All reflection paths are longer than the direct, so these
    // are >= 0. Air absorption still uses ABSOLUTE path length (longer paths darker).
    const int directOffset = juce::jmax (0, (int) std::lround (tr.directArrivalTime * (float) sr));
    const int tracedOutputLen = juce::jlimit (1, settings.maxLengthSamples, numBins - directOffset);

    // Envelope smoothing: a centred moving average whose window GROWS with time.
    // Early reflections keep their detail (short window); the sparse, diffuse late
    // tail must be smoothed HEAVILY or the gaps between sparse ray hits modulate the
    // noise carrier and the tail "pumps" (Slice 4.5 follow-up — the old 0.025*t growth
    // / 100 ms cap was far too narrow for hit-starved scenes like the gym). Grows to a
    // ~400 ms window late, which averages tens of hits into a continuous decay.
    const int   halfMin = juce::jmax (1, (int) std::round (settings.envelopeMs * 0.001f * (float) sr) / 2);
    const int   halfMax = juce::jmax (halfMin, (int) (0.2f * (float) sr)); // up to ~400 ms window
    const float halfGrowth = 0.09f;

    // PASS 1 — build the ray-traced IR (in output/relative time) and keep per-band
    // energy (output frame) for late-tail decay measurement.
    std::vector<float> rayIR ((size_t) tracedOutputLen, 0.0f);
    std::array<std::vector<float>, Material::kNumBands> bandEnergy;
    std::vector<float> broadbandEnergy ((size_t) tracedOutputLen, 0.0f);

    std::vector<float>  bandNoise ((size_t) tracedOutputLen);
    std::vector<double> prefix    ((size_t) tracedOutputLen + 1);
    juce::Random rng (20240517);

    for (int b = 0; b < Material::kNumBands; ++b)
    {
        auto& be = bandEnergy[(size_t) b];
        be.assign ((size_t) tracedOutputLen, 0.0f);

        // 1) Air-weighted energy per output sample for this band (+ prefix sum).
        prefix[0] = 0.0;
        for (int k = 0; k < tracedOutputLen; ++k)
        {
            const int histBin = k + directOffset; // absolute arrival bin
            float e = 0.0f;
            if (histBin >= 0 && histBin < numBins)
            {
                e = tr.histogram[(size_t) b][(size_t) histBin] * energyCal;
                if (settings.applyAirAbsorption)
                {
                    const float dist = (float) histBin / (float) sr * c; // ABSOLUTE path length
                    const float af = AirAbsorption::getAmplitudeFactor (b, dist,
                                                                        settings.temperatureCelsius,
                                                                        settings.relativeHumidity);
                    e *= af * af; // air absorption acts in the energy domain
                }
            }
            be[(size_t) k] = e;
            broadbandEnergy[(size_t) k] += e;
            prefix[(size_t) k + 1] = prefix[(size_t) k] + (double) e;
        }

        if (prefix[(size_t) tracedOutputLen] <= 0.0)
            continue; // no energy in this band

        // 2) Fresh independent noise, band-limited (24 dB/oct), unit RMS.
        for (int k = 0; k < tracedOutputLen; ++k)
            bandNoise[(size_t) k] = rng.nextFloat() * 2.0f - 1.0f;
        bandpassToUnitRms (bandNoise, b, (double) sr);

        // 3) Modulate by sqrt(energy density) and accumulate.
        for (int k = 0; k < tracedOutputLen; ++k)
        {
            const int half = juce::jlimit (halfMin, halfMax, (int) (halfGrowth * (float) k));
            const int lo = juce::jmax (0, k - half);
            const int hi = juce::jmin (tracedOutputLen, k + half + 1);
            const double windowEnergy = prefix[(size_t) hi] - prefix[(size_t) lo];
            const float density = (float) (windowEnergy / (double) (hi - lo));
            rayIR[(size_t) k] += bandNoise[(size_t) k] * std::sqrt (juce::jmax (0.0f, density));
        }
    }

    // Direct sound at sample 0 (timing is relative to the direct, so it's always
    // sample 0). Amplitude keeps its 1/distance scaling RELATIVE to the reflections
    // — that ratio IS the direct-to-reverberant cue (cue #3) and is distance-
    // dependent (far mic => weaker direct => more reverb). The whole IR is peak-
    // normalised afterwards (removing absolute level, which DistanceModel re-applies),
    // so what survives here is the *ratio*, not the absolute direct level. (A flat
    // unit direct would bury the reverb by ~20 dB in a large room — see Slice 4.5
    // follow-up.) Air absorption darkens the direct over its own path length.
    if (settings.includeDirect && tr.directVisible)
    {
        const float dist = juce::jmax (0.5f, tr.directDistance);
        float amp = (1.0f / dist) * settings.directGainCompensation;
        if (settings.applyAirAbsorption)
        {
            const auto air = AirAbsorption::getAllBandFactors (dist, settings.temperatureCelsius, settings.relativeHumidity);
            float mean = 0.0f;
            for (auto a : air) mean += a;
            amp *= mean / (float) Material::kNumBands;
        }
        rayIR[0] += amp;
    }

    // === Decide whether to synthesise a late tail ===
    double totalEnergy = 0.0;
    for (auto e : broadbandEnergy) totalEnergy += (double) e;
    const bool haveDecay = totalEnergy > 1.0e-12;
    bool doSynth = settings.synthesizeLateTail && haveDecay;

    int crossfadeStartBin = tracedOutputLen;
    int outLen            = tracedOutputLen;

    if (doSynth)
    {
        // Locate the broadband energy peak and the data cliff. Big/open scenes are
        // hit-starved: the histogram CLIFFS to silence (no surviving rays reach the
        // mic) long before the room stops ringing. We fit the decay over the RELIABLE
        // region [peak, cliff] (anything narrower under-reads the slope), and the
        // crossfade must begin before the cliff so the synthesiser samples real energy.
        const int eblk = juce::jmax (1, sr / 50); // 20 ms blocks
        double maxBlk = 0.0; int peakBin = 0;
        for (int s = 0; s + eblk <= tracedOutputLen; s += eblk)
        {
            double sum = 0.0; for (int i = 0; i < eblk; ++i) sum += (double) broadbandEnergy[(size_t) (s + i)];
            const double mean = sum / eblk;
            if (mean > maxBlk) { maxBlk = mean; peakBin = s; }
        }
        const double extThresh = maxBlk * 1.0e-5; // -50 dB of the loudest block
        int dataExtentBin = tracedOutputLen;
        for (int s = peakBin; s + eblk <= tracedOutputLen; s += eblk)
        {
            double sum = 0.0; for (int i = 0; i < eblk; ++i) sum += (double) broadbandEnergy[(size_t) (s + i)];
            if (sum / eblk > extThresh) dataExtentBin = s + eblk;
        }
        const float dataExtentSec = (float) dataExtentBin / (float) sr;

        // Fit the decay over the CLEAN part of the reliable region only — the last
        // ~20% before the cliff steepens artificially as hits run out, which would
        // bias the slope and create a kink at the handoff. slopeTo excludes it.
        const int slopeTo = peakBin + juce::jmax (1, (int) (0.8f * (float) (dataExtentBin - peakBin)));

        float bbSlope = -60.0f;
        const bool bbOk = fitDecaySlope (broadbandEnergy, peakBin, slopeTo, sr, bbSlope);
        if (! bbOk || bbSlope >= 0.0f)
            bbSlope = -60.0f;
        const float rt60 = juce::jlimit (0.1f, (float) kMaxIRLengthSeconds, 60.0f / std::abs (bbSlope));

        // Crossfade at the earlier of (1x RT60) and ~0.65x the data extent — the latter
        // keeps the handoff inside the CLEAN decay region (before the cliff steepening),
        // so the synthesised tail continues the same slope from the same level with no
        // plateau/kink. Well-traced rooms: RT60 wins (keep real reflections); starved
        // scenes: the cliff guard wins (hand the unreliable tail to synthesis).
        const float tracedSeconds = (float) tracedOutputLen / (float) sr;
        const float cliffGuardSec = dataExtentSec * 0.65f;
        const float xfStartSec = juce::jlimit (0.25f,
                                               juce::jmax (0.25f, tracedSeconds - settings.crossfadeSeconds - 0.05f),
                                               juce::jmin (kCrossfadeRT60Factor * rt60, cliffGuardSec));
        crossfadeStartBin = juce::jlimit (1, tracedOutputLen - 1, (int) std::lround (xfStartSec * sr));

        const int tailExtSamples = (int) std::lround (settings.tailExtensionSeconds * sr);
        const int candidateLen   = juce::jmin (settings.maxLengthSamples, crossfadeStartBin + tailExtSamples);
        outLen = juce::jmax (tracedOutputLen, candidateLen);
        // Re-cap (candidateLen already <= maxLengthSamples; tracedOutputLen <= maxLengthSamples).
        outLen = juce::jmin (outLen, settings.maxLengthSamples);

        const int tailLen = outLen - crossfadeStartBin;
        if (tailLen <= 0)
        {
            doSynth = false;
        }
        else
        {
            // Per-band decay slope is fit over the same clean region as the broadband
            // estimate, so each band's rate reflects its true decay (not the cliff).
            const int analysisFrom = peakBin;
            const int analysisTo   = juce::jmax (analysisFrom + 1, slopeTo);

            // Level at the crossfade point: a CENTRED window matching exactly what the
            // ray-traced modulation loop computes at this sample, so the synthesised
            // tail starts at the identical level (seamless handoff — no plateau).
            const int half = juce::jlimit (halfMin, halfMax, (int) (halfGrowth * (float) crossfadeStartBin));
            const int lvlLo = juce::jmax (0, crossfadeStartBin - half);
            const int lvlHi = juce::jmin (tracedOutputLen, crossfadeStartBin + half + 1);

            std::vector<float> tail ((size_t) tailLen, 0.0f);
            std::vector<float> tn   ((size_t) tailLen);
            juce::Random tailRng (12345); // fixed seed -> reproducible tail (BakePresets / IRInspect)

            for (int b = 0; b < Material::kNumBands; ++b)
            {
                const auto& be = bandEnergy[(size_t) b];

                // Energy density at the crossfade point (energy per sample).
                double sum = 0.0;
                for (int k = lvlLo; k < lvlHi; ++k) sum += (double) be[(size_t) k];
                const float energyAtXf = (float) (sum / (double) (lvlHi - lvlLo));
                if (energyAtXf < 1.0e-10f)
                    continue; // band already silent — no tail needed

                // Per-band decay slope; fall back to broadband, clamp to sane range.
                // No band may decay SLOWER than the broadband aggregate (jmin keeps it
                // at least as steep): the aggregate is the physical whole, so a single
                // band lingering past it would be unphysical and bloat the tail. HF
                // bands measuring faster keep their faster rate (the tail still darkens).
                float slope = bbSlope;
                float fit = 0.0f;
                if (fitDecaySlope (be, analysisFrom, analysisTo, sr, fit) && fit < 0.0f)
                    slope = fit;
                slope = juce::jmin (slope, bbSlope);
                slope = juce::jlimit (-120.0f, -10.0f, slope);

                // Unit-RMS band noise for the tail.
                for (int i = 0; i < tailLen; ++i)
                    tn[(size_t) i] = tailRng.nextFloat() * 2.0f - 1.0f;
                bandpassToUnitRms (tn, b, (double) sr);

                // Exponential decay envelope continuing the measured slope. With
                // startDb = 10*log10(energy), decibelsToGain(startDb + slope*t) gives
                // sqrt(energy(t)) — i.e. amplitude that continues the ray-traced
                // sqrt(energy-density) envelope without a level jump.
                const float startDb = 10.0f * std::log10 (energyAtXf);
                for (int i = 0; i < tailLen; ++i)
                {
                    const float t  = (float) i / (float) sr;
                    const float dB = startDb + slope * t;
                    tail[(size_t) i] += tn[(size_t) i] * juce::Decibels::decibelsToGain (dB);
                }
            }

            // Mix: in [crossfadeStartBin, outLen) the ray-traced part fades out while
            // the synthesised tail fades in over crossfadeSeconds, then is all tail.
            rayIR.resize ((size_t) outLen, 0.0f); // zero-pad ray part beyond the trace
            const int xfLen = juce::jmax (1, (int) std::lround (settings.crossfadeSeconds * sr));
            for (int i = 0; i < tailLen; ++i)
            {
                const int pos = crossfadeStartBin + i;
                const float xf = juce::jmin (1.0f, (float) i / (float) xfLen); // 0 -> 1
                rayIR[(size_t) pos] = rayIR[(size_t) pos] * (1.0f - xf) + tail[(size_t) i] * xf;
            }
        }
    }

    // === Trim trailing near-silence (so small rooms don't carry seconds of zeros),
    // then emit, final-fade, and peak-normalise. ===
    const int fadeSamples = juce::jmax (1, (int) std::lround (settings.finalFadeSeconds * sr));

    int finalLen = outLen;
    if (doSynth)
    {
        // Floor is measured relative to the REVERB's own peak (samples after the
        // direct impulse), NOT the global peak. The direct is a lone unit spike that
        // dwarfs the tail by ~40 dB; using it as the reference would trim the reverb
        // while it is still decaying. -75 dB below the reverb peak (≈RT60 + margin)
        // keeps the full audible decay, then the final fade takes it to true silence.
        const int directGuard = juce::jmin (outLen, (int) (0.005f * sr)); // skip the direct
        float reverbPeak = 0.0f;
        for (int i = directGuard; i < outLen; ++i) reverbPeak = juce::jmax (reverbPeak, std::abs (rayIR[(size_t) i]));
        if (reverbPeak <= 0.0f)
            for (float v : rayIR) reverbPeak = juce::jmax (reverbPeak, std::abs (v));

        const float floorAbs = reverbPeak * juce::Decibels::decibelsToGain (-75.0f); // ~RT60 + margin
        int last = directGuard;
        for (int i = outLen - 1; i >= 0; --i)
            if (std::abs (rayIR[(size_t) i]) > floorAbs) { last = i; break; }
        // End the buffer AT the last audible sample so the fade below lands on real
        // signal (the old "+ fadeSamples" put the fade in the zero-pad beyond it, so
        // the tail stopped abruptly at the trim floor with no fade — an audible cut).
        finalLen = juce::jlimit (juce::jmin (outLen, halfMax), outLen, last + 1);
    }

    juce::AudioBuffer<float> ir (1, finalLen);
    ir.clear();
    auto* o = ir.getWritePointer (0);
    for (int i = 0; i < finalLen; ++i)
        o[i] = rayIR[(size_t) i];

    // Raised-cosine fade-out over the final samples, ending at EXACTLY zero (i=0 is
    // the last sample -> gain 0; i=fade-1 -> gain ~1). Smooth so the tail eases into
    // silence rather than stepping off the trim floor.
    const int fade = juce::jmin (finalLen, fadeSamples);
    for (int i = 0; i < fade; ++i)
    {
        const float x = (float) i / (float) fade;                                    // 0 (last) .. ~1
        const float w = 0.5f * (1.0f - std::cos (juce::MathConstants<float>::pi * x)); // 0 .. 1
        o[finalLen - 1 - i] *= w;
    }

    // Peak-normalise to -1 dBFS.
    float peak = 0.0f;
    for (int i = 0; i < finalLen; ++i)
        peak = juce::jmax (peak, std::abs (o[i]));
    if (peak > 0.0f)
        ir.applyGain (juce::Decibels::decibelsToGain (-1.0f) / peak);

    return ir;
}
} // namespace Worldizer
