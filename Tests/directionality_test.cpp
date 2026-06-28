/*
    DirectionalityTest — verifies that mic pattern weighting actually shapes the IR
    (Slice 5). Renders the gymnasium with a single mic at a fixed position and four
    pattern/orientation cases:

      1. Omni                       (baseline)
      2. Shotgun facing the source  (tight: direct full, reflections suppressed)
      3. Shotgun 90 deg off-axis    (direct substantially reduced)
      4. Shotgun 180 deg (away)     (direct near the rear-rejection floor)

    The direct-to-reverberant ratio (DRR = direct peak / reverb RMS) is the
    normalisation-invariant figure of merit: a shotgun on-axis raises it (tighter),
    rotating off-axis collapses it (the source goes distant/muffled). Writes the four
    IRs to /tmp/dir_test_*.wav for ear inspection.
*/
#include <JuceHeader.h>
#include <iostream>
#include <cmath>

#include "../Source/Model/TestScenes.h"
#include "../Source/DSP/RayTracer.h"
#include "../Source/DSP/IRBuilder.h"

using namespace Worldizer;

namespace
{
    juce::AudioBuffer<float> render (Scene scene)
    {
        RayTracer tracer;
        RayTracer::Settings rs; rs.numRays = 80000; rs.randomSeed = 12345;
        const auto result = tracer.trace (scene, rs, 48000);
        IRBuilder builder;
        IRBuilder::Settings is; is.sampleRate = 48000;
        return builder.build (result, is);
    }

    // Direct peak over [0, 2ms]; reverb RMS over [10ms, end]. DRR in dB.
    float drrDb (const juce::AudioBuffer<float>& ir)
    {
        const int n  = ir.getNumSamples();
        const auto* d = ir.getReadPointer (0);
        const int directEnd = juce::jmin (n, (int) (0.002 * 48000));
        const int revStart  = juce::jmin (n, (int) (0.010 * 48000));

        float directPeak = 0.0f;
        for (int i = 0; i < directEnd; ++i) directPeak = juce::jmax (directPeak, std::abs (d[i]));

        double ss = 0.0; int c = 0;
        for (int i = revStart; i < n; ++i) { ss += (double) d[i] * d[i]; ++c; }
        const float revRms = c > 0 ? (float) std::sqrt (ss / c) : 1.0e-12f;

        return 20.0f * std::log10 ((directPeak + 1.0e-12f) / (revRms + 1.0e-12f));
    }

    void writeWav (const juce::String& name, const juce::AudioBuffer<float>& ir)
    {
        const juce::File f (juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile (name));
        f.deleteFile();
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::OutputStream> stream = f.createOutputStream();
        if (stream == nullptr)
            return;
        if (auto w = wav.createWriterFor (stream, juce::AudioFormatWriterOptions{}
                         .withSampleRate (48000.0)
                         .withNumChannels (ir.getNumChannels())
                         .withBitsPerSample (24)))
            w->writeFromAudioSampleBuffer (ir, 0, ir.getNumSamples());
    }

    Scene gymWithMic (MicPattern pattern, Vec3 orientation)
    {
        Scene s = TestScenes::gymnasium();
        // Source (-5,0,1.5), mic (5,0,1.5): the source is in the -X direction from the
        // mic, so "facing the source" = orientation (-1,0,0).
        s.getMicArray().setConfiguration (MicArray::Configuration::Single);
        s.getMicArray().setAllPatterns (pattern);
        s.getMicArray().getMic (0).setOrientation (orientation);
        return s;
    }
}

int main()
{
    const auto irOmni     = render (gymWithMic (MicPattern::Omnidirectional, { -1.0f, 0.0f, 0.0f }));
    const auto irOnAxis   = render (gymWithMic (MicPattern::Shotgun,         { -1.0f, 0.0f, 0.0f })); // toward source
    const auto irOffAxis  = render (gymWithMic (MicPattern::Shotgun,         {  0.0f, 1.0f, 0.0f })); // 90 deg
    const auto irRear     = render (gymWithMic (MicPattern::Shotgun,         {  1.0f, 0.0f, 0.0f })); // away

    writeWav ("dir_test_1_omni.wav",        irOmni);
    writeWav ("dir_test_2_shotgun_on.wav",  irOnAxis);
    writeWav ("dir_test_3_shotgun_90.wav",  irOffAxis);
    writeWav ("dir_test_4_shotgun_180.wav", irRear);

    const float drrOmni = drrDb (irOmni);
    const float drrOn    = drrDb (irOnAxis);
    const float drrOff   = drrDb (irOffAxis);
    const float drrRear  = drrDb (irRear);

    auto first = [] (const juce::AudioBuffer<float>& ir)
    {
        const auto* d = ir.getReadPointer (0);
        for (int i = 0; i < ir.getNumSamples(); ++i) if (std::abs (d[i]) > 1.0e-6f) return i;
        return -1;
    };

    std::cout << "Direct-to-reverberant ratio (dB):\n";
    std::cout << "  1 Omni:            " << juce::String (drrOmni, 1).toRawUTF8() << "\n";
    std::cout << "  2 Shotgun on-axis: " << juce::String (drrOn,   1).toRawUTF8() << "\n";
    std::cout << "  3 Shotgun 90 off:  " << juce::String (drrOff,  1).toRawUTF8() << "\n";
    std::cout << "  4 Shotgun 180:     " << juce::String (drrRear, 1).toRawUTF8() << "\n";
    std::cout << "Direct at sample 0 (all four): "
              << ((first (irOmni) == 0 && first (irOnAxis) == 0 && first (irOffAxis) == 0 && first (irRear) == 0) ? "yes" : "NO") << "\n";

    bool ok = true;
    // Shotgun on-axis tightens vs omni (higher DRR).
    if (! (drrOn > drrOmni + 2.0f)) { std::cout << "FAIL: shotgun on-axis should be tighter than omni\n"; ok = false; }
    // Rotating 90 deg off collapses the DRR substantially (source goes distant).
    if (! (drrOn - drrOff > 10.0f)) { std::cout << "FAIL: 90-deg rotation should drop DRR >10 dB\n"; ok = false; }
    // Facing away rejects the direct at least as much as 90-deg off.
    if (! (drrRear <= drrOff + 2.0f)) { std::cout << "FAIL: rear should reject the direct >= 90-deg\n"; ok = false; }
    // The four direct arrivals are at sample 0 (mic position unchanged).
    if (! (first (irOmni) == 0 && first (irOnAxis) == 0)) { std::cout << "FAIL: direct not at sample 0\n"; ok = false; }

    std::cout << "\n" << (ok ? "PASS" : "FAIL") << "  (IRs in /tmp/dir_test_*.wav)\n";
    return ok ? 0 : 1;
}
