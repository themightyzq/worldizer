/**
    CharacterTest — asserts physical/spectral properties of the character library:

      1. Registry sanity: unique ids, "none" at index 0, every non-none id
         synthesizes a non-empty, correctly normalised IR.
      2. Spectral character: band-limited characters actually attenuate out-of-band
         energy (telephone, megaphone, cab, ribbon...) and the reference characters
         stay near-flat — measured from the synthesized IRs by FFT.
      3. Distinctness: every speaker (and mic) differs from every other by >= 3 dB
         in at least one octave band — "switching characters is audible".
      4. Room tones: loop-ready (seam step comparable to typical adjacent-sample
         motion), correct peak, non-silent.

    Uses the same AssetSynth recipes the BakeAssets tool bakes from, so it needs
    no baked files. Returns 0 on pass / 1 on fail (CTest).
*/
#include <JuceHeader.h>
#include <map>
#include "../Tools/bake_assets/AssetSynth.h"
#include "../Source/DSP/CharacterLibrary.h"
#include "../Source/DSP/ConvolutionEngine.h"

using namespace Worldizer;

namespace
{
int failures = 0;

void check (bool condition, const juce::String& what)
{
    if (condition)
        std::cout << "  PASS: " << what << "\n";
    else
    {
        std::cout << "  FAIL: " << what << "\n";
        ++failures;
    }
}

// Magnitude (dB) of the IR's transfer function at `freq`, via zero-padded FFT.
float magnitudeDbAt (const juce::AudioBuffer<float>& ir, double sr, float freq)
{
    constexpr int order = 15; // 32768 points
    constexpr int n = 1 << order;
    std::vector<float> data ((size_t) n * 2, 0.0f);
    const int len = juce::jmin (ir.getNumSamples(), n);
    for (int i = 0; i < len; ++i)
        data[(size_t) i] = ir.getSample (0, i);

    juce::dsp::FFT fft (order);
    fft.performRealOnlyForwardTransform (data.data());

    const int bin = juce::jlimit (0, n / 2 - 1, (int) std::lround (freq / (sr / (double) n)));
    const float re = data[(size_t) (2 * bin)];
    const float im = data[(size_t) (2 * bin + 1)];
    return juce::Decibels::gainToDecibels (std::sqrt (re * re + im * im), -120.0f);
}

// 8-octave-band signature (relative dB, mean-removed) for distinctness checks.
std::array<float, 8> bandSignature (const juce::AudioBuffer<float>& ir, double sr)
{
    static const float centers[8] = { 62.5f, 125.0f, 250.0f, 500.0f, 1000.0f, 2000.0f, 4000.0f, 8000.0f };
    std::array<float, 8> sig {};
    float mean = 0.0f;
    for (int b = 0; b < 8; ++b)
    {
        sig[(size_t) b] = magnitudeDbAt (ir, sr, centers[b]);
        mean += sig[(size_t) b];
    }
    mean /= 8.0f;
    for (auto& v : sig)
        v -= mean;
    return sig;
}

float maxBandDifference (const std::array<float, 8>& a, const std::array<float, 8>& b)
{
    float maxDiff = 0.0f;
    for (size_t i = 0; i < 8; ++i)
        maxDiff = juce::jmax (maxDiff, std::abs (a[i] - b[i]));
    return maxDiff;
}

float peakOf (const juce::AudioBuffer<float>& b)
{
    float peak = 0.0f;
    for (int i = 0; i < b.getNumSamples(); ++i)
        peak = juce::jmax (peak, std::abs (b.getSample (0, i)));
    return peak;
}
} // namespace

int main()
{
    constexpr double sr = 48000.0;

    std::cout << "=== Registry sanity ===\n";
    {
        auto checkRegistry = [] (const std::vector<CharacterDef>& defs, const juce::String& kind, bool hasNone)
        {
            juce::StringArray seen;
            for (const auto& d : defs)
            {
                check (! seen.contains (d.id), kind + " id unique: " + d.id);
                seen.add (d.id);
            }
            if (hasNone)
                check (juce::String (defs[0].id) == "none", kind + " index 0 is none");
        };
        checkRegistry (CharacterLibrary::speakers(),  "speaker", true);
        checkRegistry (CharacterLibrary::mics(),      "mic", true);
        checkRegistry (CharacterLibrary::roomTones(), "roomtone", false);
        check (CharacterLibrary::speakers().size() == 9,  "8 speakers + none");
        check (CharacterLibrary::mics().size() == 7,      "6 mics + none");
        check (CharacterLibrary::roomTones().size() == 6, "6 room tones");
    }

    std::cout << "=== Synthesis: every id produces a normalised IR ===\n";
    std::map<juce::String, juce::AudioBuffer<float>> speakerIRs, micIRs;
    // Character IRs are normalised to UNIT ENERGY (sum h^2 == 1) so the runtime
    // loads them with Normalise::no and every character — including the "none"
    // unit delta, whose energy is exactly 1 — passes at consistent unity
    // loudness. (JUCE's Normalise::yes scales even a delta to 0.125 = -18 dB.)
    auto energyOf = [] (const juce::AudioBuffer<float>& b)
    {
        double e = 0.0;
        for (int i = 0; i < b.getNumSamples(); ++i)
            e += (double) b.getSample (0, i) * (double) b.getSample (0, i);
        return e;
    };
    for (const auto& d : CharacterLibrary::speakers())
    {
        if (juce::String (d.id) == "none") continue;
        auto ir = WorldizerAssetSynth::synthesizeSpeakerIR (d.id, sr);
        check (ir.getNumSamples() > 0, juce::String (d.id) + " synthesizes");
        const double e = energyOf (ir);
        check (std::abs (e - 1.0) < 1.0e-3, juce::String (d.id) + " unit energy (got " + juce::String (e, 6) + ")");
        speakerIRs[d.id] = std::move (ir);
    }
    for (const auto& d : CharacterLibrary::mics())
    {
        if (juce::String (d.id) == "none") continue;
        auto ir = WorldizerAssetSynth::synthesizeMicIR (d.id, sr);
        check (ir.getNumSamples() > 0, juce::String (d.id) + " synthesizes");
        const double e = energyOf (ir);
        check (std::abs (e - 1.0) < 1.0e-3, juce::String (d.id) + " unit energy (got " + juce::String (e, 6) + ")");
        micIRs[d.id] = std::move (ir);
    }

    std::cout << "=== Spectral character (band-limited characters attenuate out-of-band) ===\n";
    {
        auto rel = [&] (const juce::AudioBuffer<float>& ir, float fRef, float fTest)
        { return magnitudeDbAt (ir, sr, fRef) - magnitudeDbAt (ir, sr, fTest); };

        const auto& tel = speakerIRs["spk_telephone"];
        check (rel (tel, 1000.0f, 100.0f)  > 20.0f, "telephone: 1 kHz >> 100 Hz (+20 dB)");
        check (rel (tel, 1000.0f, 8000.0f) > 20.0f, "telephone: 1 kHz >> 8 kHz (+20 dB)");

        const auto& mega = speakerIRs["spk_megaphone"];
        check (rel (mega, 1100.0f, 200.0f) > 20.0f, "megaphone: honk 1.1 kHz >> 200 Hz");

        const auto& cab = speakerIRs["spk_guitar_cab"];
        check (rel (cab, 200.0f, 10000.0f) > 20.0f, "guitar cab: 200 Hz >> 10 kHz (dark top)");

        const auto& pa = speakerIRs["spk_fullrange_pa"];
        check (std::abs (rel (pa, 1000.0f, 4000.0f)) < 6.0f, "full-range PA: near-flat 1-4 kHz");

        const auto& ribbon = micIRs["mic_ribbon_vintage"];
        check (rel (ribbon, 1000.0f, 12000.0f) > 8.0f, "ribbon: dark top (1 kHz >> 12 kHz)");

        const auto& cond = micIRs["mic_studio_condenser"];
        check (std::abs (rel (cond, 500.0f, 5000.0f)) < 4.0f, "condenser: near-flat 0.5-5 kHz");

        const auto& carbon = micIRs["mic_carbon_telephone"];
        check (rel (carbon, 1800.0f, 150.0f) > 20.0f, "carbon button: resonance >> lows");
    }

    std::cout << "=== Distinctness: every pair differs by >= 3 dB in some band ===\n";
    {
        auto checkDistinct = [&] (const std::map<juce::String, juce::AudioBuffer<float>>& irs, const juce::String& kind)
        {
            std::vector<std::pair<juce::String, std::array<float, 8>>> sigs;
            for (const auto& [id, ir] : irs)
                sigs.push_back ({ id, bandSignature (ir, sr) });
            for (size_t i = 0; i < sigs.size(); ++i)
                for (size_t j = i + 1; j < sigs.size(); ++j)
                {
                    const float diff = maxBandDifference (sigs[i].second, sigs[j].second);
                    check (diff >= 3.0f, kind + " " + sigs[i].first + " vs " + sigs[j].first
                                          + " differ (" + juce::String (diff, 1) + " dB)");
                }
        };
        checkDistinct (speakerIRs, "speaker");
        checkDistinct (micIRs, "mic");
    }

    std::cout << "=== Room tones: loop-ready, correct level ===\n";
    for (const auto& d : CharacterLibrary::roomTones())
    {
        auto bed = WorldizerAssetSynth::synthesizeRoomTone (d.id, sr);
        check (bed.getNumSamples() > (int) sr, juce::String (d.id) + " at least 1 s long");

        const float peakDb = juce::Decibels::gainToDecibels (peakOf (bed), -120.0f);
        check (std::abs (peakDb - (-20.0f)) < 0.1f, juce::String (d.id) + " peak -20 dBFS");

        // Seam: |x[0] - x[N-1]| should look like a typical adjacent-sample step,
        // not a discontinuity. Allow 8x the mean step (noise beds are jumpy).
        const auto* s = bed.getReadPointer (0);
        const int n = bed.getNumSamples();
        double meanStep = 0.0;
        for (int i = 1; i < n; ++i)
            meanStep += std::abs (s[i] - s[i - 1]);
        meanStep /= (double) (n - 1);
        const double seamStep = std::abs (s[0] - s[n - 1]);
        check (seamStep < 8.0 * meanStep,
               juce::String (d.id) + " seam continuous (seam " + juce::String (seamStep, 6)
               + " vs mean step " + juce::String (meanStep, 6) + ")");
    }

    std::cout << "=== Runtime level: 'none' delta and unit-energy IRs pass at unity ===\n";
    {
        // Regression pin for the -18 dB bug: JUCE's Normalise::yes scales even a
        // unit delta to 0.125, so the character path loads with normalise=false
        // and unit-energy IRs. Verify end-to-end through the actual engine.
        auto steadyGainThrough = [&] (const juce::AudioBuffer<float>& irIn)
        {
            Worldizer::ConvolutionEngine engine;
            engine.prepare (48000.0, 512, 2);
            engine.loadIR (irIn, 48000.0, 5.0f, /*normalise*/ false);

            // juce::dsp::Convolution installs the IR from a background thread
            // that polls every 10 ms — a tight processing loop outruns it and
            // measures the pre-install state. Give it wall-clock time.
            juce::Thread::sleep (200);

            juce::AudioBuffer<float> block (2, 512);
            juce::Random rng (99);
            double sumIn = 0.0, sumOut = 0.0;
            for (int b = 0; b < 200; ++b)
            {
                for (int i = 0; i < 512; ++i)
                {
                    const float s = 0.25f * (rng.nextFloat() * 2.0f - 1.0f);
                    block.setSample (0, i, s);
                    block.setSample (1, i, s);
                    if (b >= 100) sumIn += (double) s * (double) s;
                }
                juce::dsp::AudioBlock<float> ab (block);
                engine.process (ab);
                if (b >= 100)
                    for (int i = 0; i < 512; ++i)
                        sumOut += (double) block.getSample (0, i) * (double) block.getSample (0, i);
            }
            return std::sqrt (sumOut / sumIn);
        };

        juce::AudioBuffer<float> delta (1, 1);
        delta.setSample (0, 0, 1.0f);
        const double noneGain = steadyGainThrough (delta);
        check (std::abs (noneGain - 1.0) < 0.02,
               "'none' delta is a true bypass (gain " + juce::String (noneGain, 4) + ", want 1.0)");

        const double paGain = steadyGainThrough (speakerIRs["spk_fullrange_pa"]);
        check (std::abs (paGain - 1.0) < 0.35,
               "unit-energy full-range PA passes near unity on broadband noise (gain "
               + juce::String (paGain, 4) + ")");
    }

    std::cout << (failures == 0 ? "\nALL PASS\n" : "\n" + juce::String (failures) + " FAILURES\n");
    return failures == 0 ? 0 : 1;
}
