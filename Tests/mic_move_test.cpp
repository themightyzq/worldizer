/*
    MicMoveTest — verifies the IR architecture across mic-array configurations:

      1. Moving the mic still changes the rendered room-response IR (different
         reflection pattern from a different listening point) — for Single, Stereo XY
         (move the array), and Spaced Pair (move one mic).
      2. The direct sound is at sample 0 in the nearest channel (distance lives on the
         wet path, not in the IR).
      3. DistanceModel pre-delay and attenuation change with source/mic distance.
      4. Stereo configurations produce 2-channel IRs.
*/
#include <JuceHeader.h>
#include <iostream>
#include <cmath>

#include "../Source/Model/TestScenes.h"
#include "../Source/DSP/RayTracer.h"
#include "../Source/DSP/IRBuilder.h"
#include "../Source/DSP/DistanceModel.h"

using namespace Worldizer;

namespace
{
    juce::AudioBuffer<float> renderScene (const Scene& scene)
    {
        RayTracer tracer;
        RayTracer::Settings rs; rs.numRays = 50000; rs.randomSeed = 12345;
        const auto result = tracer.trace (scene, rs, 48000);
        IRBuilder builder;
        IRBuilder::Settings is; is.sampleRate = 48000;
        return builder.build (result, is);
    }

    juce::AudioBuffer<float> renderSingle (Scene scene, Vec3 micPos)
    {
        scene.getMicArray().setConfiguration (MicArray::Configuration::Single);
        scene.getMicArray().getMic (0).setPosition (micPos);
        return renderScene (scene);
    }

    int firstNonZero (const juce::AudioBuffer<float>& ir, int ch)
    {
        const auto* d = ir.getReadPointer (ch);
        for (int i = 0; i < ir.getNumSamples(); ++i)
            if (std::abs (d[i]) > 1.0e-6f) return i;
        return -1;
    }

    void report (const juce::String& label, const juce::AudioBuffer<float>& ir)
    {
        const int n = ir.getNumSamples();
        const auto* d = ir.getReadPointer (0);
        float peak = 0.0f; int pk = 0; double ss = 0.0;
        for (int i = 0; i < n; ++i) { const float a = std::abs (d[i]); if (a > peak) { peak = a; pk = i; } ss += (double) d[i] * d[i]; }
        const float rms = (float) std::sqrt (ss / n);
        std::cout << label.toRawUTF8() << ": ch " << ir.getNumChannels() << ", len "
                  << juce::String (n / 48000.0, 2).toRawUTF8() << "s, ch0-first-nonzero " << firstNonZero (ir, 0)
                  << ", peak@" << pk << ", rms " << juce::String (20.0 * std::log10 (rms + 1e-20), 1).toRawUTF8() << " dB\n";
    }

    // RMS difference across ALL channels (so moving mic 1 — which only changes the
    // right channel — is detected too).
    double rmsDiffDb (const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b)
    {
        const int n  = juce::jmin (a.getNumSamples(), b.getNumSamples());
        const int ch = juce::jmin (a.getNumChannels(), b.getNumChannels());
        double d = 0.0;
        for (int c = 0; c < ch; ++c)
            for (int i = 0; i < n; ++i)
            {
                const double diff = (double) a.getReadPointer (c)[i] - (double) b.getReadPointer (c)[i];
                d += diff * diff;
            }
        return 20.0 * std::log10 (std::sqrt (d / juce::jmax (1, n * ch)) + 1e-20);
    }
}

int main()
{
    bool ok = true;

    // --- 1 & 2: single-mic IR changes with mic position; direct at sample 0. ---
    struct Case { const char* name; Scene scene; Vec3 a; Vec3 b; };
    std::vector<Case> cases = {
        { "Small Concrete Room", TestScenes::smallConcreteRoom(), { 2.0f, 0.0f, 1.5f }, { 2.6f, 1.6f, 1.5f } },
        { "Gymnasium",           TestScenes::gymnasium(),         { 5.0f, 0.0f, 1.5f }, { -10.0f, -7.0f, 1.5f } },
    };

    for (auto& c : cases)
    {
        std::cout << "\n=== " << c.name << " (Single) ===\n";
        const auto irA = renderSingle (c.scene, c.a);
        const auto irB = renderSingle (c.scene, c.b);
        report ("mic A", irA);
        report ("mic B", irB);

        const double diff = rmsDiffDb (irA, irB);
        const bool differ = diff > -60.0;
        std::cout << "RMS of (A - B): " << juce::String (diff, 1).toRawUTF8() << " dB  -> "
                  << (differ ? "DIFFERENT (audible)" : "identical") << "\n";

        const bool directA0 = firstNonZero (irA, 0) == 0 && firstNonZero (irB, 0) == 0;
        std::cout << "Direct at sample 0 (both positions): " << (directA0 ? "yes" : "NO") << "\n";
        ok = ok && differ && directA0;
    }

    // --- 3: Stereo XY — moving the array changes the IR; 2 channels. ---
    {
        std::cout << "\n=== Gymnasium (Stereo XY: move array) ===\n";
        Scene s = TestScenes::gymnasium();
        s.getMicArray().setConfiguration (MicArray::Configuration::StereoXY);
        s.getMicArray().setXYPosition ({ 5.0f, 0.0f, 1.5f });
        const auto irA = renderScene (s);
        s.getMicArray().setXYPosition ({ -8.0f, 6.0f, 1.5f });
        const auto irB = renderScene (s);
        report ("array A", irA);
        report ("array B", irB);
        const bool stereo = irA.getNumChannels() == 2 && irB.getNumChannels() == 2;
        const bool differ = rmsDiffDb (irA, irB) > -60.0;
        std::cout << "2-channel: " << (stereo ? "yes" : "NO") << "; moving array changes IR: " << (differ ? "yes" : "NO") << "\n";
        ok = ok && stereo && differ;
    }

    // --- 4: Spaced Pair — moving one mic changes the IR; 2 channels. ---
    {
        std::cout << "\n=== Gymnasium (Spaced Pair: move mic 1) ===\n";
        Scene s = TestScenes::gymnasium();
        s.getMicArray().setConfiguration (MicArray::Configuration::Single);
        s.getMicArray().getMic (0).setPosition ({ 5.0f, -1.0f, 1.5f });
        s.getMicArray().setConfiguration (MicArray::Configuration::SpacedPair);
        const auto irA = renderScene (s);
        s.getMicArray().getMic (1).setPosition ({ 5.0f, 4.0f, 1.5f });
        const auto irB = renderScene (s);
        report ("pair A", irA);
        report ("pair B", irB);
        const bool stereo = irA.getNumChannels() == 2 && irB.getNumChannels() == 2;
        const bool differ = rmsDiffDb (irA, irB) > -60.0;
        std::cout << "2-channel: " << (stereo ? "yes" : "NO") << "; moving mic 1 changes IR: " << (differ ? "yes" : "NO") << "\n";
        ok = ok && stereo && differ;
    }

    // --- 5: DistanceModel output changes with distance. ---
    std::cout << "\n=== DistanceModel responds to distance ===\n";
    DistanceModel dm; // default plugin model (accuracy 0.4)
    const float near = 2.0f, far = 20.0f;
    const float pdNear = dm.computePreDelaySamples (near, 48000.0);
    const float pdFar  = dm.computePreDelaySamples (far,  48000.0);
    const float gNear  = dm.computeAttenuationGain (near);
    const float gFar   = dm.computeAttenuationGain (far);
    std::cout << "  pre-delay  2m=" << juce::String (pdNear, 1).toRawUTF8()
              << " samples, 20m=" << juce::String (pdFar, 1).toRawUTF8() << " samples\n";
    std::cout << "  attenuation 2m=" << juce::String (juce::Decibels::gainToDecibels (gNear), 1).toRawUTF8()
              << " dB, 20m=" << juce::String (juce::Decibels::gainToDecibels (gFar), 1).toRawUTF8() << " dB\n";
    const bool distanceWorks = pdFar > pdNear + 100.0f && gFar < gNear * 0.5f;
    std::cout << "  farther => longer delay AND quieter: " << (distanceWorks ? "yes" : "NO") << "\n";
    ok = ok && distanceWorks;

    std::cout << "\n" << (ok ? "PASS" : "FAIL") << "\n";
    return ok ? 0 : 1;
}
