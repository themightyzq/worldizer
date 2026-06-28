/*
    StereoIRTest — verifies that 2-mic configurations produce true-stereo IRs (Slice 5).

    Stereo XY (small concrete room): asserts the IR is 2-channel, both channels have a
    direct impulse at sample 0 (coincident capsules => no inter-channel time
    difference), the two channels are NOT identical, and the L-R difference carries
    non-trivial energy (real stereo information from per-capsule reception + decorrelated
    diffuse fields).

    Spaced pair (mics 0.5 m apart along the source axis): asserts the direct arrives at
    DIFFERENT sample times in L vs R (~0.5 m path difference => ~70 samples @ 48k) — the
    natural Haas/precedence cue.
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
    juce::AudioBuffer<float> render (const Scene& scene)
    {
        RayTracer tracer;
        RayTracer::Settings rs; rs.numRays = 60000; rs.randomSeed = 12345;
        const auto result = tracer.trace (scene, rs, 48000);
        IRBuilder builder;
        IRBuilder::Settings is; is.sampleRate = 48000;
        return builder.build (result, is);
    }

    int firstNonZero (const juce::AudioBuffer<float>& ir, int ch)
    {
        const auto* d = ir.getReadPointer (ch);
        for (int i = 0; i < ir.getNumSamples(); ++i)
            if (std::abs (d[i]) > 1.0e-4f) return i; // direct is well above this
        return -1;
    }

    // RMS of channel 0 minus channel 1 (the stereo-difference / "side" signal), in dB
    // relative to the mid (0.5*(L+R)) RMS. Higher => wider stereo image.
    float sideToMidDb (const juce::AudioBuffer<float>& ir)
    {
        const int n = ir.getNumSamples();
        const auto* l = ir.getReadPointer (0);
        const auto* r = ir.getReadPointer (1);
        double side = 0.0, mid = 0.0;
        for (int i = 0; i < n; ++i)
        {
            const double s = 0.5 * ((double) l[i] - r[i]);
            const double m = 0.5 * ((double) l[i] + r[i]);
            side += s * s; mid += m * m;
        }
        return 20.0f * std::log10 ((float) (std::sqrt (side / juce::jmax (1, n)) + 1e-20)
                                 / (float) (std::sqrt (mid  / juce::jmax (1, n)) + 1e-20));
    }
}

int main()
{
    bool ok = true;

    // === Stereo XY ===
    {
        std::cout << "=== Stereo XY (small concrete room, shotgun, 90 deg) ===\n";
        Scene s = TestScenes::smallConcreteRoom();
        s.getMicArray().setConfiguration (MicArray::Configuration::StereoXY);
        s.getMicArray().setXYPosition ({ 2.0f, 0.0f, 1.5f });
        s.getMicArray().setXYOrientation ({ -1.0f, 0.0f, 0.0f }); // toward source at origin
        s.getMicArray().setXYAngleDegrees (90.0f);
        s.getMicArray().setAllPatterns (MicPattern::Shotgun);

        const auto ir = render (s);
        const int f0 = firstNonZero (ir, 0), f1 = firstNonZero (ir, 1);
        const float sideMid = sideToMidDb (ir);

        std::cout << "  channels: " << ir.getNumChannels() << "\n";
        std::cout << "  direct sample L=" << f0 << " R=" << f1 << " (coincident => equal)\n";
        std::cout << "  side/mid: " << juce::String (sideMid, 1).toRawUTF8() << " dB (higher = wider)\n";

        const bool stereo = ir.getNumChannels() == 2;
        const bool bothDirect = f0 == 0 && f1 == 0;
        const bool hasSide = sideMid > -40.0f; // non-trivial L-R energy
        if (! stereo)     { std::cout << "  FAIL: not 2-channel\n"; ok = false; }
        if (! bothDirect) { std::cout << "  FAIL: both channels should have direct at sample 0\n"; ok = false; }
        if (! hasSide)    { std::cout << "  FAIL: L-R difference too small (channels nearly identical)\n"; ok = false; }
    }

    // === Spaced pair (0.5 m apart along the source axis => inter-channel time diff) ===
    {
        std::cout << "\n=== Spaced Pair (small concrete room, mics 0.5 m apart in X) ===\n";
        Scene s = TestScenes::smallConcreteRoom();
        s.getMicArray().setConfiguration (MicArray::Configuration::SpacedPair);
        // Source at origin; mic 0 nearer (2.0 m), mic 1 farther (2.5 m). Path diff 0.5 m.
        s.getMicArray().getMic (0).setPosition ({ 2.0f, 0.0f, 1.5f });
        s.getMicArray().getMic (1).setPosition ({ 2.5f, 0.0f, 1.5f });

        const auto ir = render (s);
        const int f0 = firstNonZero (ir, 0), f1 = firstNonZero (ir, 1);
        const int expected = (int) std::lround (0.5 / 343.0 * 48000.0); // ~70 samples

        std::cout << "  channels: " << ir.getNumChannels() << "\n";
        std::cout << "  direct sample L(mic0,near)=" << f0 << " R(mic1,far)=" << f1
                  << "  (expected diff ~" << expected << ")\n";

        const bool stereo = ir.getNumChannels() == 2;
        const bool ch0First = f0 == 0;                       // nearer mic defines the reference
        const bool itd = std::abs ((f1 - f0) - expected) <= 24; // within ~0.5 ms
        if (! stereo)   { std::cout << "  FAIL: not 2-channel\n"; ok = false; }
        if (! ch0First) { std::cout << "  FAIL: nearest channel direct should be at sample 0\n"; ok = false; }
        if (! itd)      { std::cout << "  FAIL: inter-channel time difference off expected ~" << expected << "\n"; ok = false; }
    }

    std::cout << "\n" << (ok ? "PASS" : "FAIL") << "\n";
    return ok ? 0 : 1;
}
