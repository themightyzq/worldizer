#include "IRBuilder.h"
#include "AirAbsorption.h"
#include <cmath>
#include <vector>
#include <limits>

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
    // roughly flat. Steeper skirts keep each band's air-absorption decay independent.
    constexpr float kOctaveQ = 0.9f;

    // Crossfade from ray-traced to synthesised tail starts at this multiple of RT60.
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
    // [fromBin, toBin), aggregated into ~20 ms blocks in the log domain.
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

//==============================================================================
IRBuilder::ChannelBuild IRBuilder::buildChannel (
    const std::array<std::vector<float>, Material::kNumBands>& histogram,
    const RayTracer::Result::DirectInfo& direct,
    int refOffset, int numBins, int numRaysIn, float micRadiusIn,
    const Settings& settings, int sr,
    int noiseSeed, int tailSeed) const
{
    ChannelBuild cb;

    constexpr float c   = kSpeedOfSound;
    const int   numRays = juce::jmax (1, numRaysIn);

    // Calibrate reverb energy to the direct sound's scale (mic capture cross-section
    // => amplitude micRadius/(2*dist), so energy factor (2/micRadius)^2 / numRays).
    const float micR      = juce::jmax (0.01f, micRadiusIn);
    const float energyCal = (4.0f / (micR * micR)) / (float) numRays;

    // Output sample 0 corresponds to absolute time refOffset/sr (shared across all
    // channels). A reflection at absolute bin b lands at output bin (b - refOffset);
    // this channel's direct lands at (its arrival - refOffset). Air absorption still
    // uses ABSOLUTE path length (= (k + refOffset)/sr * c).
    const int tracedOutputLen = juce::jlimit (1, settings.maxLengthSamples, numBins - refOffset);

    // Envelope smoothing window grows with time (early detail, heavily-smoothed sparse
    // late tail to avoid pumping). See Slice 4.5 notes.
    const float smoothScale = juce::jmax (1.0f, settings.envelopeSmoothingScale);
    const int   halfMin = juce::jmax (1, (int) std::round (settings.envelopeMs * 0.001f * (float) sr) / 2);
    const int   halfMax = juce::jmax (halfMin, (int) (0.2f * smoothScale * (float) sr)); // up to ~400 ms (omni)
    const float halfGrowth = 0.09f * smoothScale;

    std::vector<float> rayIR ((size_t) tracedOutputLen, 0.0f);
    std::array<std::vector<float>, Material::kNumBands> bandEnergy;
    std::vector<float> broadbandEnergy ((size_t) tracedOutputLen, 0.0f);

    std::vector<float>  bandNoise ((size_t) tracedOutputLen);
    std::vector<double> prefix    ((size_t) tracedOutputLen + 1);
    juce::Random rng (noiseSeed);

    for (int b = 0; b < Material::kNumBands; ++b)
    {
        auto& be = bandEnergy[(size_t) b];
        be.assign ((size_t) tracedOutputLen, 0.0f);

        // 1) Air-weighted energy per output sample for this band (+ prefix sum).
        prefix[0] = 0.0;
        for (int k = 0; k < tracedOutputLen; ++k)
        {
            const int histBin = k + refOffset; // absolute arrival bin
            float e = 0.0f;
            if (histBin >= 0 && histBin < numBins)
            {
                e = histogram[(size_t) b][(size_t) histBin] * energyCal;
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

    // Direct sound. Timing is relative to the SHARED reference, so this channel's
    // direct lands at (its arrival - refOffset) — 0 for the nearest mic, a few samples
    // later for a farther one (the spaced-pair inter-channel time difference). Its
    // amplitude keeps 1/distance scaling relative to the reflections, times the mic's
    // reception-pattern gain in the direct direction (1.0 for omni).
    if (settings.includeDirect && direct.visible)
    {
        const int directSample = juce::jlimit (0, tracedOutputLen - 1,
                                               (int) std::lround (direct.arrivalTime * (float) sr) - refOffset);
        const float dist = juce::jmax (0.5f, direct.distance);
        float amp = (1.0f / dist) * settings.directGainCompensation * direct.receptionGain;
        if (settings.applyAirAbsorption)
        {
            const auto air = AirAbsorption::getAllBandFactors (dist, settings.temperatureCelsius, settings.relativeHumidity);
            float mean = 0.0f;
            for (auto a : air) mean += a;
            amp *= mean / (float) Material::kNumBands;
        }
        rayIR[(size_t) directSample] += amp;
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

        const int slopeTo = peakBin + juce::jmax (1, (int) (0.8f * (float) (dataExtentBin - peakBin)));

        float bbSlope = -60.0f;
        const bool bbOk = fitDecaySlope (broadbandEnergy, peakBin, slopeTo, sr, bbSlope);
        if (! bbOk || bbSlope >= 0.0f)
            bbSlope = -60.0f;
        const float rt60 = juce::jlimit (0.1f, (float) kMaxIRLengthSeconds, 60.0f / std::abs (bbSlope));

        const float tracedSeconds = (float) tracedOutputLen / (float) sr;
        const float cliffGuardSec = dataExtentSec * 0.65f;
        const float xfStartSec = juce::jlimit (0.25f,
                                               juce::jmax (0.25f, tracedSeconds - settings.crossfadeSeconds - 0.05f),
                                               juce::jmin (kCrossfadeRT60Factor * rt60, cliffGuardSec));
        crossfadeStartBin = juce::jlimit (1, tracedOutputLen - 1, (int) std::lround (xfStartSec * sr));

        const int tailExtSamples = (int) std::lround (settings.tailExtensionSeconds * sr);
        const int candidateLen   = juce::jmin (settings.maxLengthSamples, crossfadeStartBin + tailExtSamples);
        outLen = juce::jmax (tracedOutputLen, candidateLen);
        outLen = juce::jmin (outLen, settings.maxLengthSamples);

        const int tailLen = outLen - crossfadeStartBin;
        if (tailLen <= 0)
        {
            doSynth = false;
        }
        else
        {
            const int analysisFrom = peakBin;
            const int analysisTo   = juce::jmax (analysisFrom + 1, slopeTo);

            const int half = juce::jlimit (halfMin, halfMax, (int) (halfGrowth * (float) crossfadeStartBin));
            const int lvlLo = juce::jmax (0, crossfadeStartBin - half);
            const int lvlHi = juce::jmin (tracedOutputLen, crossfadeStartBin + half + 1);

            std::vector<float> tail ((size_t) tailLen, 0.0f);
            std::vector<float> tn   ((size_t) tailLen);
            juce::Random tailRng (tailSeed); // decorrelated per channel (stereo tail width)

            for (int b = 0; b < Material::kNumBands; ++b)
            {
                const auto& be = bandEnergy[(size_t) b];

                double sum = 0.0;
                for (int k = lvlLo; k < lvlHi; ++k) sum += (double) be[(size_t) k];
                const float energyAtXf = (float) (sum / (double) (lvlHi - lvlLo));
                if (energyAtXf < 1.0e-10f)
                    continue; // band already silent — no tail needed

                float slope = bbSlope;
                float fit = 0.0f;
                if (fitDecaySlope (be, analysisFrom, analysisTo, sr, fit) && fit < 0.0f)
                    slope = fit;
                slope = juce::jmin (slope, bbSlope);
                slope = juce::jlimit (-120.0f, -10.0f, slope);

                for (int i = 0; i < tailLen; ++i)
                    tn[(size_t) i] = tailRng.nextFloat() * 2.0f - 1.0f;
                bandpassToUnitRms (tn, b, (double) sr);

                const float startDb = 10.0f * std::log10 (energyAtXf);
                for (int i = 0; i < tailLen; ++i)
                {
                    const float t  = (float) i / (float) sr;
                    const float dB = startDb + slope * t;
                    tail[(size_t) i] += tn[(size_t) i] * juce::Decibels::decibelsToGain (dB);
                }
            }

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

    cb.ir = std::move (rayIR);
    cb.synthesized = doSynth;
    return cb;
}

//==============================================================================
juce::AudioBuffer<float> IRBuilder::build (const RayTracer::Result& tr,
                                           const Settings& settings) const
{
    const int sr       = tr.sampleRate;
    const int numMics  = juce::jmax (1, tr.getNumMics());
    const int numBins  = juce::jmax (1, tr.numBins);

    if (tr.histogramsPerMic.empty())
        return juce::AudioBuffer<float> (1, 1); // degenerate — empty trace

    // A directional mic's reception weighting makes its histogram effectively sparse,
    // so widen the envelope smoothing to keep the late tail monotonic (anti-pump).
    // Omni keeps scale 1.0 (bit-identical to Slice 4.5); an explicit setting can only
    // raise it further. Resolved once and shared by all channels.
    Settings s = settings;
    s.envelopeSmoothingScale = juce::jmax (settings.envelopeSmoothingScale,
                                           tr.anyMicDirectional ? 3.0f : 1.0f);

    // Shared time reference = the EARLIEST direct arrival across mics. All channels
    // place output sample 0 at this time, so a spaced pair preserves its inter-channel
    // time difference (the near mic's direct at ~0, the far mic's a few samples later).
    float refArrival = std::numeric_limits<float>::max();
    for (const auto& di : tr.directPerMic)
        refArrival = juce::jmin (refArrival, di.arrivalTime);
    if (! (refArrival < std::numeric_limits<float>::max()))
        refArrival = 0.0f;
    const int refOffset = juce::jmax (0, (int) std::lround (refArrival * (float) sr));

    // Carrier/tail decorrelation depends on the actual mic spacing. COINCIDENT mics
    // (XY pair) share one position, so their diffuse fields arrive identically — they
    // should use the SAME noise seed (intensity stereophony: the stereo image comes
    // purely from the per-capsule reception-gain envelopes, an omni coincident pair is
    // correctly mono). SPACED mics get DECORRELATED seeds so their independent diffuse
    // fields / tails widen the image (time-of-arrival stereophony). (Deviation from the
    // prompt's blanket decorrelation, which over-widens a coincident pair toward fully
    // uncorrelated L/R.)
    bool coincident = true;
    for (int m = 1; m < numMics && m < (int) tr.micPositions.size(); ++m)
        if ((tr.micPositions[(size_t) m] - tr.micPositions[0]).length() > 1.0e-3f)
            coincident = false;

    std::vector<ChannelBuild> channels;
    channels.reserve ((size_t) numMics);
    int commonLen = 1;
    bool anySynth = false;
    int  maxDirectSample = 0;
    for (int m = 0; m < numMics; ++m)
    {
        const int seedOffset = coincident ? 0 : m;
        auto cb = buildChannel (tr.histogramsPerMic[(size_t) m], tr.directPerMic[(size_t) m],
                                refOffset, numBins, tr.numRays, tr.micRadius,
                                s, sr,
                                /*noiseSeed*/ 20240517 + seedOffset, /*tailSeed*/ 12345 + seedOffset);
        commonLen = juce::jmax (commonLen, (int) cb.ir.size());
        anySynth  = anySynth || cb.synthesized;
        maxDirectSample = juce::jmax (maxDirectSample,
                                      (int) std::lround (tr.directPerMic[(size_t) m].arrivalTime * (float) sr) - refOffset);
        channels.push_back (std::move (cb));
    }

    // Pad every channel to the common length (the longer tail wins; shorter channels
    // are zero-padded, then individually faded below).
    for (auto& cb : channels)
        cb.ir.resize ((size_t) commonLen, 0.0f);

    // === Trailing-silence trim (only when a tail was synthesised) ===
    int finalLen = commonLen;
    if (anySynth)
    {
        // Skip past every channel's direct spike (so the reverb-peak reference isn't
        // dominated by the direct), then measure the floor relative to the reverb peak.
        const int directGuard = juce::jmin (commonLen, maxDirectSample + (int) (0.005f * sr));
        float reverbPeak = 0.0f;
        for (const auto& cb : channels)
            for (int i = directGuard; i < commonLen; ++i)
                reverbPeak = juce::jmax (reverbPeak, std::abs (cb.ir[(size_t) i]));
        if (reverbPeak <= 0.0f)
            for (const auto& cb : channels)
                for (float v : cb.ir) reverbPeak = juce::jmax (reverbPeak, std::abs (v));

        const float floorAbs = reverbPeak * juce::Decibels::decibelsToGain (-75.0f);
        int last = directGuard;
        for (const auto& cb : channels)
            for (int i = commonLen - 1; i >= 0; --i)
                if (std::abs (cb.ir[(size_t) i]) > floorAbs) { last = juce::jmax (last, i); break; }

        const int minLen = juce::jmin (commonLen, (int) (0.2f * sr));
        finalLen = juce::jlimit (minLen, commonLen, last + 1);
    }

    // === Emit, per-channel final fade, then SHARED peak-normalise to -1 dBFS ===
    juce::AudioBuffer<float> ir (numMics, finalLen);
    ir.clear();

    const int fadeSamples = juce::jmax (1, (int) std::lround (settings.finalFadeSeconds * sr));
    const int fade = juce::jmin (finalLen, fadeSamples);

    for (int m = 0; m < numMics; ++m)
    {
        auto* o = ir.getWritePointer (m);
        const auto& src = channels[(size_t) m].ir;
        for (int i = 0; i < finalLen; ++i)
            o[i] = src[(size_t) i];

        // Raised-cosine fade-out over the final samples, ending at EXACTLY zero.
        for (int i = 0; i < fade; ++i)
        {
            const float x = (float) i / (float) fade;
            const float w = 0.5f * (1.0f - std::cos (juce::MathConstants<float>::pi * x));
            o[finalLen - 1 - i] *= w;
        }
    }

    // Shared normalisation: one global peak across ALL channels -> one gain factor,
    // so inter-channel level differences (the XY stereo image) survive.
    float peak = 0.0f;
    for (int m = 0; m < numMics; ++m)
    {
        const auto* o = ir.getReadPointer (m);
        for (int i = 0; i < finalLen; ++i)
            peak = juce::jmax (peak, std::abs (o[i]));
    }
    if (peak > 0.0f)
        ir.applyGain (juce::Decibels::decibelsToGain (-1.0f) / peak);

    return ir;
}
} // namespace Worldizer
