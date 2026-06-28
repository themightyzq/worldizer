/*
    RenderTestScene — Worldizer Slice 1 headless ray-tracer tool.

    Loads a hardcoded scene, runs the ray tracer, builds an impulse response, and
    writes it to a WAV file. Used to validate the offline IR-generation pipeline.

    Usage:
      RenderTestScene --scene <name> --rays <N> --bounces <N> --output <path> --seed <N>

    Scenes: smallConcreteRoom, hallway, forestClearing, gymnasium, anechoic
*/

#include <JuceHeader.h>
#include <iostream>
#include <cmath>

#include "../Source/Model/TestScenes.h"
#include "../Source/DSP/RayTracer.h"
#include "../Source/DSP/IRBuilder.h"
#include "../Source/Shared/Constants.h"

using namespace Worldizer;

namespace
{
    juce::String fmtVec (Vec3 v)
    {
        return "(" + juce::String (v.x, 2) + ", " + juce::String (v.y, 2) + ", " + juce::String (v.z, 2) + ")";
    }
}

int main (int argc, char* argv[])
{
    // Supports both "--key value" and "--key=value".
    auto opt = [argc, argv] (const juce::String& name, juce::String defaultValue) -> juce::String
    {
        for (int i = 1; i < argc; ++i)
        {
            const juce::String a (argv[i]);
            if (a == name)
                return (i + 1 < argc) ? juce::String (argv[i + 1]) : defaultValue;
            if (a.startsWith (name + "="))
                return a.fromFirstOccurrenceOf ("=", false, false);
        }
        return defaultValue;
    };

    const juce::String sceneName = opt ("--scene", "smallConcreteRoom");
    const int rays    = opt ("--rays",    "20000").getIntValue();
    const int bounces = opt ("--bounces", juce::String (kMaxBouncesFull)).getIntValue();
    const int seed    = opt ("--seed",    "12345").getIntValue();

    const juce::File defaultOut = juce::File::getSpecialLocation (juce::File::tempDirectory)
                                      .getChildFile ("worldizer_" + sceneName + ".wav");
    const juce::String outPath = opt ("--output", defaultOut.getFullPathName());

    std::cout << "Worldizer RenderTestScene v" << kVersionString << "\n";

    bool ok = false;
    Scene scene = TestScenes::byName (sceneName, ok);
    if (! ok)
    {
        std::cout << "ERROR: unknown scene '" << sceneName << "'\n"
                  << "Valid scenes: smallConcreteRoom, hallway, forestClearing, gymnasium, anechoic\n";
        return 1;
    }

    const auto bounds = scene.getBounds();

    std::cout << "Scene: " << sceneName << "\n"
              << "  Brushes: " << (int) scene.getNumBrushes() << "\n"
              << "  Bounds: " << fmtVec (bounds.first) << " to " << fmtVec (bounds.second) << "\n"
              << "  Source: " << fmtVec (scene.getSource().getPosition()) << "\n"
              << "  Mic:    " << fmtVec (scene.getMic().getPosition()) << "\n";

    RayTracer::Settings settings;
    settings.numRays    = rays;
    settings.maxBounces = bounces;
    settings.randomSeed = seed;

    constexpr int sampleRate = 48000;

    std::cout << "Settings:\n"
              << "  Rays: " << rays << "\n"
              << "  Max bounces: " << bounces << "\n"
              << "  Sample rate: " << sampleRate << "\n"
              << "  Seed: " << seed << "\n";

    RayTracer tracer;
    std::cout << "Tracing... " << std::flush;
    const double traceStart = juce::Time::getMillisecondCounterHiRes();
    const auto result = tracer.trace (scene, settings, sampleRate);
    const double traceMs = juce::Time::getMillisecondCounterHiRes() - traceStart;
    std::cout << "done (" << juce::String (traceMs / 1000.0, 2) << "s)\n";

    const auto& direct0 = result.directPerMic[0];
    std::cout << "Result:\n"
              << "  Mics: "            << result.getNumMics() << "\n"
              << "  Direct distance: " << juce::String (direct0.distance, 3) << " m\n"
              << "  Direct arrival: "  << juce::String (direct0.arrivalTime * 1000.0f, 2)
              << " ms (sample " << (int) std::lround (direct0.arrivalTime * sampleRate) << ")\n"
              << "  Direct visible: "  << (direct0.visible ? "yes" : "no") << "\n"
              << "  Total rays: "      << result.numRays << "\n"
              << "  Mic hits (mic 0): "<< result.hitCountPerMic[0] << "\n";

    IRBuilder builder;
    IRBuilder::Settings irSettings;
    irSettings.sampleRate = sampleRate;

    std::cout << "Building IR... " << std::flush;
    const double buildStart = juce::Time::getMillisecondCounterHiRes();
    const auto ir = builder.build (result, irSettings);
    const double buildMs = juce::Time::getMillisecondCounterHiRes() - buildStart;
    std::cout << "done (" << juce::String (buildMs / 1000.0, 2) << "s)\n";

    const int numSamples = ir.getNumSamples();
    const auto* data = ir.getReadPointer (0);

    float peak = 0.0f;
    int peakSample = 0;
    double sumSquares = 0.0;
    for (int i = 0; i < numSamples; ++i)
    {
        const float a = std::abs (data[i]);
        if (a > peak) { peak = a; peakSample = i; }
        sumSquares += (double) data[i] * (double) data[i];
    }
    const float rms = numSamples > 0 ? (float) std::sqrt (sumSquares / numSamples) : 0.0f;

    // Write the IR as a 24-bit PCM WAV (peak-normalised to -1 dBFS, so within range).
    juce::File outFile (outPath);
    outFile.getParentDirectory().createDirectory();
    outFile.deleteFile();

    bool wrote = false;
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> stream = outFile.createOutputStream();
    if (stream != nullptr)
    {
        if (auto writer = wav.createWriterFor (stream,
                                               juce::AudioFormatWriterOptions{}
                                                   .withSampleRate ((double) sampleRate)
                                                   .withNumChannels (1)
                                                   .withBitsPerSample (24)))
        {
            wrote = writer->writeFromAudioSampleBuffer (ir, 0, numSamples);
        }
    }

    std::cout << "IR:\n"
              << "  Length: " << numSamples << " samples ("
              << juce::String ((double) numSamples / sampleRate, 2) << "s)\n"
              << "  Peak: " << juce::String (juce::Decibels::gainToDecibels (peak), 2)
              << " dBFS at sample " << peakSample << "\n"
              << "  RMS: " << juce::String (juce::Decibels::gainToDecibels (rms), 1) << " dBFS\n";

    if (wrote)
        std::cout << "  Wrote: " << outFile.getFullPathName() << "\n";
    else
        std::cout << "  ERROR: failed to write " << outFile.getFullPathName() << "\n";

    return wrote ? 0 : 1;
}
