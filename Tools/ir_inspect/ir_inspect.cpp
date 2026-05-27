/*
    IRInspect — Worldizer Slice 4.5 IR analysis / regression tool.

    Dumps the structural properties of an impulse response: length, peak, RMS, the
    first non-zero sample, the energy distribution across direct/early/late/tail
    windows, per-octave-band decay rates, and an estimated RT60. Use it to verify
    the IR pipeline (direct at sample 0, late tail present, peak at -1 dBFS, clean
    fade to silence) and to spot regressions across code changes.

    Usage:
      IRInspect analyze --input <file.wav>
      IRInspect render  --scene <name> [--rays N] [--bounces N] [--seed N]
                        [--no-tail] [--distance <m>] [--accuracy <0..1>]
                        --output <file.wav>

    render builds the IR from a test scene and analyses it. --no-tail disables the
    statistical late-tail synthesis (raw trace, for A/B). --distance bakes the
    DistanceModel pre-delay + attenuation into the output (for offline ear-testing:
    convolve the result with a click to hear the distance).

    Scenes: smallConcreteRoom, hallway, forestClearing, gymnasium, anechoic
*/

#include <JuceHeader.h>
#include <iostream>
#include <iomanip>
#include <cmath>
#include <vector>
#include <array>

#include "../../Source/Model/TestScenes.h"
#include "../../Source/DSP/RayTracer.h"
#include "../../Source/DSP/IRBuilder.h"
#include "../../Source/DSP/DistanceModel.h"
#include "../../Source/Shared/Constants.h"

using namespace Worldizer;

namespace
{
    constexpr std::array<float, Material::kNumBands> kBandCentres
        { 62.5f, 125.0f, 250.0f, 500.0f, 1000.0f, 2000.0f, 4000.0f, 8000.0f };
    constexpr float kOctaveQ = 0.9f;

    juce::String optVal (int argc, char** argv, const juce::String& name, juce::String def)
    {
        for (int i = 1; i < argc; ++i)
        {
            const juce::String a (argv[i]);
            if (a == name) return (i + 1 < argc) ? juce::String (argv[i + 1]) : def;
            if (a.startsWith (name + "=")) return a.fromFirstOccurrenceOf ("=", false, false);
        }
        return def;
    }

    bool hasFlag (int argc, char** argv, const juce::String& name)
    {
        for (int i = 1; i < argc; ++i)
            if (juce::String (argv[i]) == name) return true;
        return false;
    }

    // Cascaded octave bandpass (matches IRBuilder), in place.
    std::vector<float> octaveBandpass (const float* x, int n, int band, double sr)
    {
        std::vector<float> out ((size_t) n);
        auto coeffs = juce::dsp::IIR::Coefficients<float>::makeBandPass (sr, kBandCentres[(size_t) band], kOctaveQ);
        juce::dsp::IIR::Filter<float> s1, s2;
        s1.coefficients = coeffs; s2.coefficients = coeffs;
        const juce::dsp::ProcessSpec spec { sr, (juce::uint32) juce::jmax (1, n), 1 };
        s1.prepare (spec); s2.prepare (spec); s1.reset(); s2.reset();
        for (int i = 0; i < n; ++i)
            out[(size_t) i] = s2.processSample (s1.processSample (x[i]));
        return out;
    }

    // Fit the Schroeder energy-decay curve (dB) of a signal over [hiDb, loDb] and
    // return the slope in dB/s. valid=false if the signal never spans the region.
    float schroederSlopeDbPerSec (const std::vector<float>& sig, int sr, float hiDb, float loDb, bool& valid)
    {
        const int n = (int) sig.size();
        std::vector<double> edc ((size_t) n + 1, 0.0);
        for (int i = n - 1; i >= 0; --i)
            edc[(size_t) i] = edc[(size_t) i + 1] + (double) sig[(size_t) i] * (double) sig[(size_t) i];

        const double total = edc[0];
        if (total <= 1.0e-20) { valid = false; return 0.0f; }

        std::vector<double> ts, dbs;
        for (int i = 0; i < n; ++i)
        {
            const double db = 10.0 * std::log10 (juce::jmax (1.0e-30, edc[(size_t) i] / total));
            if (db <= hiDb && db >= loDb)
            {
                ts.push_back ((double) i / sr);
                dbs.push_back (db);
            }
            if (db < loDb) break;
        }
        if (ts.size() < 4) { valid = false; return 0.0f; }

        const double nn = (double) ts.size();
        double sx = 0, sy = 0, sxx = 0, sxy = 0;
        for (size_t i = 0; i < ts.size(); ++i) { sx += ts[i]; sy += dbs[i]; sxx += ts[i]*ts[i]; sxy += ts[i]*dbs[i]; }
        const double denom = nn * sxx - sx * sx;
        if (std::abs (denom) < 1.0e-12) { valid = false; return 0.0f; }
        valid = true;
        return (float) ((nn * sxy - sx * sy) / denom);
    }

    void analyze (const juce::AudioBuffer<float>& ir, double sr, const juce::String& label)
    {
        const int n = ir.getNumSamples();
        const auto* d = ir.getReadPointer (0);

        float peak = 0.0f; int peakSample = 0; double sumSq = 0.0; int firstNonZero = -1;
        for (int i = 0; i < n; ++i)
        {
            const float a = std::abs (d[i]);
            if (a > peak) { peak = a; peakSample = i; }
            if (firstNonZero < 0 && a > 1.0e-7f) firstNonZero = i;
            sumSq += (double) d[i] * (double) d[i];
        }
        const float rms = n > 0 ? (float) std::sqrt (sumSq / n) : 0.0f;

        // Energy distribution windows (sum of squares fraction).
        auto windowEnergy = [&] (double t0, double t1)
        {
            const int a = juce::jlimit (0, n, (int) std::lround (t0 * sr));
            const int b = juce::jlimit (0, n, (int) std::lround (t1 * sr));
            double e = 0.0;
            for (int i = a; i < b; ++i) e += (double) d[i] * d[i];
            return e;
        };
        const double total = juce::jmax (1.0e-20, sumSq);
        const double eDirect = windowEnergy (0.0,   0.05);
        const double eEarly  = windowEnergy (0.05,  0.5);
        const double eLate   = windowEnergy (0.5,   2.0);
        const double eTail   = windowEnergy (2.0,   (double) n / sr);

        // Last sample above -80 dB of the REVERB peak (samples after the direct), so
        // the "fade to silence" check is not fooled by the dominant direct impulse.
        const int directGuard = juce::jmin (n, (int) (0.005 * sr));
        float reverbPeak = 0.0f;
        for (int i = directGuard; i < n; ++i) reverbPeak = juce::jmax (reverbPeak, std::abs (d[i]));
        if (reverbPeak <= 0.0f) reverbPeak = peak;
        int lastAudible = 0;
        const float floorAbs = reverbPeak * 1.0e-4f;
        for (int i = n - 1; i >= 0; --i) if (std::abs (d[i]) > floorAbs) { lastAudible = i; break; }

        std::cout << "File: " << label.toRawUTF8() << "\n";
        std::cout << "  Length: " << n << " samples (" << juce::String ((double) n / sr, 2).toRawUTF8()
                  << "s @ " << (int) sr << "Hz)\n";
        std::cout << "  Peak:  " << juce::String (juce::Decibels::gainToDecibels (peak), 2).toRawUTF8()
                  << " dBFS at sample " << peakSample << " (" << juce::String (peakSample * 1000.0 / sr, 2).toRawUTF8() << " ms)\n";
        std::cout << "  RMS:   " << juce::String (juce::Decibels::gainToDecibels (rms), 1).toRawUTF8() << " dBFS\n";
        std::cout << "  First non-zero sample: " << firstNonZero << "\n";
        // Real truncation = significant energy in the actual last sample (a clean fade
        // leaves it ~0). -50 dB below the reverb peak is the "abrupt cut" threshold.
        const bool truncated = std::abs (d[n - 1]) > reverbPeak * juce::Decibels::decibelsToGain (-50.0f);
        std::cout << "  Tail audible until (-80dB of reverb peak): " << juce::String (lastAudible * 1000.0 / sr, 1).toRawUTF8()
                  << " ms" << (truncated ? "  <-- WARNING: hard truncation (last sample loud)" : "  (fades clean)") << "\n";
        std::cout << "  Energy distribution:\n";
        std::cout << "    0-50ms   (direct+early): " << juce::String (100.0 * eDirect / total, 1).toRawUTF8() << "%\n";
        std::cout << "    50-500ms (early late):   " << juce::String (100.0 * eEarly  / total, 1).toRawUTF8() << "%\n";
        std::cout << "    500ms-2s (late):         " << juce::String (100.0 * eLate   / total, 1).toRawUTF8() << "%\n";
        std::cout << "    2s-end   (synthesized):  " << juce::String (100.0 * eTail   / total, 1).toRawUTF8() << "%\n";

        // Coarse decay envelope: RMS per 200 ms block (dB relative to whole-IR peak).
        std::cout << "  Envelope (200ms block RMS, dB rel. peak):\n   ";
        {
            const int blk = juce::jmax (1, (int) (0.2 * sr));
            for (int s = 0; s < n; s += blk)
            {
                const int e = juce::jmin (n, s + blk);
                double ss = 0.0; for (int i = s; i < e; ++i) ss += (double) d[i] * d[i];
                const float r = (float) std::sqrt (ss / juce::jmax (1, e - s));
                const float db = juce::Decibels::gainToDecibels (r / juce::jmax (1.0e-12f, peak));
                std::cout << " " << juce::String (db, 0).toRawUTF8();
            }
            std::cout << "\n";
        }

        std::cout << "  Decay rates per band (dB/s):\n";
        for (int b = 0; b < Material::kNumBands; ++b)
        {
            const auto banded = octaveBandpass (d, n, b, sr);
            bool valid = false;
            const float slope = schroederSlopeDbPerSec (banded, (int) sr, -5.0f, -35.0f, valid);
            std::cout << "    " << std::setw (7) << kBandCentres[(size_t) b] << " Hz: "
                      << (valid ? juce::String (slope, 1).toRawUTF8() : "  --  (insufficient decay)") << "\n";
        }

        // Estimated RT60 from the broadband Schroeder decay (T30 extrapolation).
        std::vector<float> bb (d, d + n);
        bool bbValid = false;
        const float broadband = schroederSlopeDbPerSec (bb, (int) sr, -5.0f, -35.0f, bbValid);
        if (bbValid && broadband < 0.0f)
            std::cout << "  Estimated RT60: " << juce::String (60.0f / std::abs (broadband), 2).toRawUTF8() << "s\n";
        else
            std::cout << "  Estimated RT60: -- (insufficient decay / dry)\n";
        std::cout << std::flush;
    }

    bool loadWav (const juce::File& f, juce::AudioBuffer<float>& out, double& srOut)
    {
        juce::AudioFormatManager fm; fm.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (f));
        if (r == nullptr || r->lengthInSamples <= 0) return false;
        out.setSize ((int) r->numChannels, (int) r->lengthInSamples);
        r->read (&out, 0, (int) r->lengthInSamples, 0, true, true);
        srOut = r->sampleRate;
        return true;
    }

    bool writeWav (const juce::File& f, const juce::AudioBuffer<float>& buf, double sr)
    {
        f.getParentDirectory().createDirectory();
        f.deleteFile();
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::OutputStream> stream = f.createOutputStream();
        if (stream == nullptr) return false;
        if (auto writer = wav.createWriterFor (stream,
                              juce::AudioFormatWriterOptions{}
                                  .withSampleRate (sr).withNumChannels (1).withBitsPerSample (24)))
            return writer->writeFromAudioSampleBuffer (buf, 0, buf.getNumSamples());
        return false;
    }

    // Bakes DistanceModel pre-delay + attenuation into an IR (for offline auditioning).
    juce::AudioBuffer<float> applyDistance (const juce::AudioBuffer<float>& ir, double sr,
                                            float distance, float accuracy)
    {
        DistanceModel::Settings s; s.accuracy = accuracy;
        DistanceModel dm (s);
        const int preDelay = (int) std::lround (dm.computePreDelaySamples (distance, sr));
        const float gain = dm.computeAttenuationGain (distance);

        juce::AudioBuffer<float> out (1, ir.getNumSamples() + preDelay);
        out.clear();
        out.copyFrom (0, preDelay, ir, 0, 0, ir.getNumSamples());
        out.applyGain (gain);
        return out;
    }
}

int main (int argc, char** argv)
{
    if (argc < 2)
    {
        std::cout << "Usage: IRInspect analyze --input <file.wav>\n"
                     "       IRInspect render --scene <name> [--rays N] [--bounces N] [--seed N]\n"
                     "                        [--no-tail] [--distance <m>] [--accuracy <0..1>] --output <file.wav>\n";
        return 1;
    }

    const juce::String cmd (argv[1]);

    if (cmd == "analyze")
    {
        const juce::File in (juce::File::getCurrentWorkingDirectory()
                                 .getChildFile (optVal (argc, argv, "--input", "")));
        juce::AudioBuffer<float> ir; double sr = 48000.0;
        if (! loadWav (in, ir, sr)) { std::cerr << "Failed to read " << in.getFullPathName() << "\n"; return 1; }
        analyze (ir, sr, in.getFullPathName());
        return 0;
    }

    if (cmd == "render")
    {
        const juce::String sceneName = optVal (argc, argv, "--scene", "gymnasium");
        const int rays    = optVal (argc, argv, "--rays",    "50000").getIntValue();
        const int bounces = optVal (argc, argv, "--bounces", juce::String (kMaxBouncesFull)).getIntValue();
        const int seed    = optVal (argc, argv, "--seed",    "42").getIntValue();
        const bool noTail = hasFlag (argc, argv, "--no-tail");
        const float distance = optVal (argc, argv, "--distance", "-1").getFloatValue();
        const float accuracy = optVal (argc, argv, "--accuracy", "0.4").getFloatValue();

        bool ok = false;
        Scene scene = TestScenes::byName (sceneName, ok);
        if (! ok) { std::cerr << "Unknown scene: " << sceneName << "\n"; return 1; }

        constexpr int sr = 48000;
        RayTracer tracer;
        RayTracer::Settings rs; rs.numRays = rays; rs.maxBounces = bounces; rs.randomSeed = seed;
        const auto result = tracer.trace (scene, rs, sr);

        IRBuilder builder;
        IRBuilder::Settings is; is.sampleRate = sr; is.synthesizeLateTail = ! noTail;
        auto ir = builder.build (result, is);

        if (distance >= 0.0f)
        {
            std::cout << "(baking distance " << distance << "m, accuracy " << accuracy << " into output)\n";
            ir = applyDistance (ir, sr, distance, accuracy);
        }

        const juce::String outPath = optVal (argc, argv, "--output",
            juce::File::getSpecialLocation (juce::File::tempDirectory)
                .getChildFile ("irinspect_" + sceneName + ".wav").getFullPathName());
        const juce::File out (juce::File::getCurrentWorkingDirectory().getChildFile (outPath));
        if (! writeWav (out, ir, sr)) { std::cerr << "Failed to write " << out.getFullPathName() << "\n"; return 1; }

        analyze (ir, sr, out.getFullPathName());
        return 0;
    }

    std::cerr << "Unknown command: " << cmd << "\n";
    return 1;
}
