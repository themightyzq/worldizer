/*
    DistanceTest — validates the DistanceModel curves (Slice 4.5).

    Prints the accurate vs musical pre-delay (samples @ 48k) and attenuation (dB) at
    a range of distances, and asserts the physics: accurate = d/c delay and 1/r
    level (-6 dB/doubling); musical = 0.6x delay and 1/r^1.5 level (-9 dB/doubling).
*/
#include <JuceHeader.h>
#include <iostream>
#include <cmath>

#include "../Source/DSP/DistanceModel.h"

using namespace Worldizer;

namespace
{
    bool approx (float a, float b, float tol) { return std::abs (a - b) <= tol; }
}

int main()
{
    constexpr double sr = 48000.0;
    DistanceModel accurate ({ 343.0f, 1.0f, 1.0f, 0.3f, 1.0f, 1.0e9f });
    DistanceModel musical  ({ 343.0f, 0.0f, 1.0f, 0.3f, 1.0f, 1.0e9f });

    bool pass = true;

    for (float d : { 1.0f, 2.0f, 4.0f, 8.0f, 16.0f })
    {
        const float apd = accurate.computePreDelaySamples (d, sr);
        const float mpd = musical.computePreDelaySamples (d, sr);
        const float aGainDb = juce::Decibels::gainToDecibels (accurate.computeAttenuationGain (d));
        const float mGainDb = juce::Decibels::gainToDecibels (musical.computeAttenuationGain (d));

        std::cout << "Distance " << juce::String (d, 1).toRawUTF8() << "m:\n";
        std::cout << "  Accurate pre-delay (samples @ 48k): " << juce::String (apd, 1).toRawUTF8() << "\n";
        std::cout << "  Musical  pre-delay (samples @ 48k): " << juce::String (mpd, 1).toRawUTF8() << "\n";
        std::cout << "  Accurate gain (dB):                 " << juce::String (aGainDb, 2).toRawUTF8() << "\n";
        std::cout << "  Musical  gain (dB):                 " << juce::String (mGainDb, 2).toRawUTF8() << "\n";

        // Expected physics.
        const float expAccPd = d / 343.0f * (float) sr;
        const float expMusPd = expAccPd * 0.6f;
        const float expAccDb = juce::Decibels::gainToDecibels (1.0f / d);            // 1/r
        const float expMusDb = juce::Decibels::gainToDecibels (std::pow (1.0f / d, 1.5f)); // 1/r^1.5

        if (! approx (apd, expAccPd, 0.5f))   { std::cout << "  FAIL accurate pre-delay\n"; pass = false; }
        if (! approx (mpd, expMusPd, 0.5f))   { std::cout << "  FAIL musical pre-delay\n";  pass = false; }
        if (! approx (aGainDb, expAccDb, 0.1f)) { std::cout << "  FAIL accurate gain\n";     pass = false; }
        if (! approx (mGainDb, expMusDb, 0.1f)) { std::cout << "  FAIL musical gain\n";      pass = false; }
    }

    // Spot-check the -6 / -9 dB-per-doubling rule and the 0-dB reference at 1 m.
    pass = pass && approx (juce::Decibels::gainToDecibels (accurate.computeAttenuationGain (1.0f)), 0.0f, 0.01f);
    pass = pass && approx (juce::Decibels::gainToDecibels (musical.computeAttenuationGain (1.0f)),  0.0f, 0.01f);
    pass = pass && approx (juce::Decibels::gainToDecibels (accurate.computeAttenuationGain (2.0f)), -6.02f, 0.05f);
    pass = pass && approx (juce::Decibels::gainToDecibels (musical.computeAttenuationGain (2.0f)),  -9.03f, 0.05f);

    // Default plugin model (accuracy 0.4) sits between the two.
    DistanceModel deflt; // accuracy 0.4
    const float dDb = juce::Decibels::gainToDecibels (deflt.computeAttenuationGain (2.0f));
    std::cout << "\nDefault model (accuracy 0.4) gain at 2m: " << juce::String (dDb, 2).toRawUTF8() << " dB\n";
    pass = pass && (dDb < -6.02f && dDb > -9.03f);

    std::cout << "\n" << (pass ? "PASS: distance curves are mathematically correct." : "FAIL") << "\n";
    return pass ? 0 : 1;
}
