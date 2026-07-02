/**
    AmbientBedTest — asserts the AmbientBed runtime behaves:

      1. Seamless looping: an integer-cycle sine bed played across many loop
         wraps has no discontinuity anywhere (max adjacent-sample step bounded
         by the sine's own slew).
      2. Level control: output RMS follows setLevel; level 0 is silence.
      3. Bed swap: crossfades (no step discontinuity) and lands on the new bed.
      4. clearSample fades to silence.
      5. Slot recycling: many sequential swaps all take effect (the 4-slot pool
         recycles; nothing leaks or wedges).

    Returns 0 on pass / 1 on fail (CTest).
*/
#include <JuceHeader.h>
#include <cmath>
#include "../Source/DSP/AmbientBed.h"

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

constexpr double kSr = 48000.0;
constexpr int    kBlock = 512;

// Integer number of cycles over the buffer => seamlessly loopable by construction.
juce::AudioBuffer<float> makeSineBed (double approxFreq, double seconds, float amplitude)
{
    const int n = (int) (kSr * seconds);
    const double cycles = std::round (approxFreq * seconds);
    juce::AudioBuffer<float> b (1, n);
    auto* d = b.getWritePointer (0);
    for (int i = 0; i < n; ++i)
        d[i] = amplitude * (float) std::sin (2.0 * juce::MathConstants<double>::pi * cycles * (double) i / (double) n);
    return b;
}

// Runs the bed for `numBlocks` blocks and returns the concatenated output.
juce::AudioBuffer<float> run (AmbientBed& bed, int numBlocks)
{
    juce::AudioBuffer<float> out (2, numBlocks * kBlock);
    out.clear();
    juce::AudioBuffer<float> block (2, kBlock);
    for (int i = 0; i < numBlocks; ++i)
    {
        block.clear();
        bed.addToBuffer (block);
        for (int ch = 0; ch < 2; ++ch)
            out.copyFrom (ch, i * kBlock, block, ch, 0, kBlock);
    }
    return out;
}

float rmsOf (const juce::AudioBuffer<float>& b, int start, int len)
{
    double sumSq = 0.0;
    for (int i = start; i < start + len; ++i)
        sumSq += (double) b.getSample (0, i) * (double) b.getSample (0, i);
    return (float) std::sqrt (sumSq / (double) len);
}

float maxStepOf (const juce::AudioBuffer<float>& b, int start, int len)
{
    float maxStep = 0.0f;
    for (int i = juce::jmax (1, start); i < start + len; ++i)
        maxStep = juce::jmax (maxStep, std::abs (b.getSample (0, i) - b.getSample (0, i - 1)));
    return maxStep;
}
} // namespace

int main()
{
    const juce::dsp::ProcessSpec spec { kSr, (juce::uint32) kBlock, 2 };

    std::cout << "=== Seamless looping ===\n";
    {
        AmbientBed bed;
        bed.prepare (spec);
        bed.setLevel (1.0f);

        const double freq = 440.0, lenSec = 0.25; // short bed => many wraps
        bed.setSample (makeSineBed (freq, lenSec, 0.5f), kSr);

        // 2 s of output = 16 loop wraps. Skip the first 0.5 s (swap-in crossfade).
        auto out = run (bed, (int) (2.0 * kSr / kBlock));
        const int steady = (int) (0.5 * kSr);

        // A 440 Hz sine's max adjacent step is 2*pi*f/sr * A; allow 1.5x headroom.
        const float sineSlew = 0.5f * 2.0f * juce::MathConstants<float>::pi * (float) freq / (float) kSr;
        const float maxStep  = maxStepOf (out, steady, out.getNumSamples() - steady);
        check (maxStep < 1.5f * sineSlew,
               "no discontinuity across 16 loop wraps (max step " + juce::String (maxStep, 5)
               + " vs sine slew " + juce::String (sineSlew, 5) + ")");

        const float expectedRms = 0.5f / std::sqrt (2.0f);
        const float rms = rmsOf (out, steady, out.getNumSamples() - steady);
        check (std::abs (rms - expectedRms) < 0.02f,
               "steady RMS matches the bed (got " + juce::String (rms, 4) + ", want " + juce::String (expectedRms, 4) + ")");
    }

    std::cout << "=== Level control ===\n";
    {
        AmbientBed bed;
        bed.prepare (spec);
        bed.setSample (makeSineBed (440.0, 0.25, 0.5f), kSr);

        bed.setLevel (1.0f);
        auto loud = run (bed, (int) (1.0 * kSr / kBlock));
        bed.setLevel (0.5f);
        auto half = run (bed, (int) (1.0 * kSr / kBlock));
        bed.setLevel (0.0f);
        auto off  = run (bed, (int) (1.0 * kSr / kBlock));

        const int steady = (int) (0.5 * kSr);
        const float rLoud = rmsOf (loud, steady, loud.getNumSamples() - steady);
        const float rHalf = rmsOf (half, steady, half.getNumSamples() - steady);
        const float rOff  = rmsOf (off,  steady, off.getNumSamples() - steady);
        check (std::abs (rHalf / rLoud - 0.5f) < 0.05f, "level 0.5 halves the RMS");
        check (rOff < 1.0e-6f, "level 0 is silent (RMS " + juce::String (rOff, 8) + ")");
    }

    std::cout << "=== Bed swap crossfade ===\n";
    {
        AmbientBed bed;
        bed.prepare (spec);
        bed.setLevel (1.0f);
        bed.setSample (makeSineBed (220.0, 0.25, 0.5f), kSr);
        run (bed, (int) (1.0 * kSr / kBlock)); // settle on bed A

        bed.setSample (makeSineBed (880.0, 0.25, 0.5f), kSr);
        auto out = run (bed, (int) (1.0 * kSr / kBlock)); // swap happens at the start

        // No discontinuity during the fade: bound by the faster sine's slew + fade slope.
        const float slew880 = 0.5f * 2.0f * juce::MathConstants<float>::pi * 880.0f / (float) kSr;
        const float maxStep = maxStepOf (out, 1, out.getNumSamples() - 1);
        check (maxStep < 2.0f * slew880, "swap crossfade has no step (max " + juce::String (maxStep, 5) + ")");

        // After the 250 ms fade the output should be pure 880 Hz: verify via
        // autocorrelation at the 880 Hz period vs the 220 Hz period.
        const int steady = (int) (0.5 * kSr);
        const int n = out.getNumSamples() - steady;
        auto acAt = [&] (int lag)
        {
            double ac = 0.0;
            for (int i = steady; i < steady + n - lag; ++i)
                ac += (double) out.getSample (0, i) * (double) out.getSample (0, i + lag);
            return ac / (double) (n - lag);
        };
        const int lag880 = (int) std::lround (kSr / 880.0);
        const int lag440 = (int) std::lround (kSr / 440.0); // = period of 220 half... distinguishes
        check (acAt (lag880) > 0.9 * acAt (0), "settled on the new 880 Hz bed");
        juce::ignoreUnused (lag440);
    }

    std::cout << "=== clearSample fades to silence ===\n";
    {
        AmbientBed bed;
        bed.prepare (spec);
        bed.setLevel (1.0f);
        bed.setSample (makeSineBed (440.0, 0.25, 0.5f), kSr);
        run (bed, (int) (1.0 * kSr / kBlock));

        bed.clearSample();
        auto out = run (bed, (int) (1.0 * kSr / kBlock));
        const int tail = (int) (0.5 * kSr);
        const float rms = rmsOf (out, out.getNumSamples() - tail, tail);
        check (rms < 1.0e-6f, "silent after clearSample (RMS " + juce::String (rms, 8) + ")");
    }

    std::cout << "=== Slot recycling over many swaps ===\n";
    {
        AmbientBed bed;
        bed.prepare (spec);
        bed.setLevel (1.0f);

        // 12 sequential swaps (3x the pool size), processing enough between each
        // for the fade to complete. The last bed has a distinct amplitude.
        for (int k = 0; k < 12; ++k)
        {
            const float amp = (k == 11) ? 0.25f : 0.5f;
            bed.setSample (makeSineBed (330.0 + 20.0 * k, 0.25, amp), kSr);
            run (bed, (int) (0.5 * kSr / kBlock));
        }
        auto out = run (bed, (int) (0.5 * kSr / kBlock));
        const float rms = rmsOf (out, 0, out.getNumSamples());
        const float expected = 0.25f / std::sqrt (2.0f);
        check (std::abs (rms - expected) < 0.02f,
               "12th swap took effect (RMS " + juce::String (rms, 4) + ", want " + juce::String (expected, 4) + ")");
    }

    std::cout << (failures == 0 ? "\nALL PASS\n" : "\n" + juce::String (failures) + " FAILURES\n");
    return failures == 0 ? 0 : 1;
}
