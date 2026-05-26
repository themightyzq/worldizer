/*
    MicMoveTest — verifies that moving the mic (or source) produces a different
    rendered IR, i.e. that "move the mic, then audition" will sound different.
    This is the engine half of the drag-to-audition feature.
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
    juce::AudioBuffer<float> render (Scene scene, Vec3 micPos)
    {
        scene.getMic().setPosition (micPos);
        RayTracer tracer;
        RayTracer::Settings rs; rs.numRays = 50000; rs.randomSeed = 12345;
        const auto result = tracer.trace (scene, rs, 48000);
        IRBuilder builder;
        IRBuilder::Settings is; is.sampleRate = 48000;
        return builder.build (result, is);
    }

    void report (const juce::String& label, const juce::AudioBuffer<float>& ir)
    {
        const int n = ir.getNumSamples();
        const auto* d = ir.getReadPointer (0);
        float peak = 0.0f; int pk = 0; double ss = 0.0;
        for (int i = 0; i < n; ++i) { const float a = std::abs (d[i]); if (a > peak) { peak = a; pk = i; } ss += (double) d[i] * d[i]; }
        const float rms = (float) std::sqrt (ss / n);
        std::cout << label.toRawUTF8() << ": peak@sample " << pk << " (" << juce::String (pk / 48.0, 2).toRawUTF8()
                  << " ms), rms " << juce::String (20.0 * std::log10 (rms + 1e-20), 1).toRawUTF8() << " dB\n";
    }

    double rmsDiffDb (const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b)
    {
        const int n = juce::jmin (a.getNumSamples(), b.getNumSamples());
        double d = 0.0;
        for (int i = 0; i < n; ++i)
        {
            const double diff = (double) a.getReadPointer (0)[i] - (double) b.getReadPointer (0)[i];
            d += diff * diff;
        }
        return 20.0 * std::log10 (std::sqrt (d / n) + 1e-20);
    }
}

int main()
{
    struct Case { const char* name; Scene scene; Vec3 a; Vec3 b; };
    std::vector<Case> cases = {
        { "Small Concrete Room", TestScenes::smallConcreteRoom(), { 2.0f, 0.0f, 1.5f }, { 2.6f, 1.6f, 1.5f } },
        { "Gymnasium",           TestScenes::gymnasium(),         { 5.0f, 0.0f, 1.5f }, { -10.0f, -7.0f, 1.5f } },
    };

    bool allDiffer = true;
    for (auto& c : cases)
    {
        std::cout << "\n=== " << c.name << " ===\n";
        const auto irA = render (c.scene, c.a);
        const auto irB = render (c.scene, c.b);
        report ("mic A", irA);
        report ("mic B", irB);
        const double diff = rmsDiffDb (irA, irB);
        std::cout << "RMS of (A - B): " << juce::String (diff, 1).toRawUTF8() << " dB  -> "
                  << (diff > -60.0 ? "DIFFERENT (audible)" : "identical") << "\n";
        allDiffer = allDiffer && diff > -60.0;
    }

    std::cout << "\n" << (allDiffer ? "PASS: moving the mic changes the IR." : "FAIL: IRs identical") << "\n";
    return allDiffer ? 0 : 1;
}
