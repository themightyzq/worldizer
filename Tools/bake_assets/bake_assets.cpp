/**
    BakeAssets — synthesizes the shipped placeholder character/bed WAVs:

        Resources/Speakers/<id>.wav    (speaker character IRs, ~85 ms mono)
        Resources/Mics/<id>.wav        (mic character IRs, ~43 ms mono)
        Resources/RoomTones/<id>.wav   (ambient beds, ~7.75 s mono, loop-ready)

    Ids come from CharacterLibrary (the single source of truth shared with the
    plugin); recipes live in AssetSynth.h (shared with CharacterTest). Re-run
    after changing either, then rebuild — CMake's CONFIGURE_DEPENDS glob embeds
    the WAVs as binary data (WORLDIZER_HAS_CHARACTERS=1).

    Usage: BakeAssets [outputRootDir]   (default: ./Resources relative to cwd)
*/
#include <JuceHeader.h>
#include "AssetSynth.h"
#include "../../Source/DSP/CharacterLibrary.h"
#include "../../Source/Model/MaterialResolver.h"

namespace
{
bool writeWav (const juce::File& file, const juce::AudioBuffer<float>& buffer, double sr)
{
    if (buffer.getNumSamples() == 0)
        return false;

    file.deleteFile();
    std::unique_ptr<juce::OutputStream> stream = file.createOutputStream();
    if (stream == nullptr)
        return false;

    juce::WavAudioFormat wav;
    if (auto writer = wav.createWriterFor (stream,
                          juce::AudioFormatWriterOptions()
                              .withSampleRate (sr)
                              .withNumChannels (juce::jmax (1, buffer.getNumChannels()))
                              .withBitsPerSample (24)))
        return writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples());

    return false;
}
} // namespace

int main (int argc, char* argv[])
{
    const auto root = argc > 1 ? juce::File::getCurrentWorkingDirectory().getChildFile (argv[1])
                               : juce::File::getCurrentWorkingDirectory().getChildFile ("Resources");

    constexpr double sr = 48000.0;
    int written = 0, failed = 0;

    auto bakeList = [&] (const std::vector<Worldizer::CharacterDef>& defs,
                         const juce::String& subDir,
                         juce::AudioBuffer<float> (*synth) (const juce::String&, double))
    {
        const auto dir = root.getChildFile (subDir);
        dir.createDirectory();
        for (const auto& def : defs)
        {
            const juce::String id (def.id);
            if (id == "none")
                continue;

            const auto buffer = synth (id, sr);
            const auto out    = dir.getChildFile (id + ".wav");
            if (buffer.getNumSamples() > 0 && writeWav (out, buffer, sr))
            {
                std::cout << "  " << out.getFullPathName() << "  ("
                          << buffer.getNumSamples() << " samples)\n";
                ++written;
            }
            else
            {
                std::cerr << "FAILED: " << out.getFullPathName() << "\n";
                ++failed;
            }
        }
    };

    // Wrap the 3-arg room-tone synth to match the 2-arg signature.
    struct ToneSynth
    {
        static juce::AudioBuffer<float> synth (const juce::String& id, double sampleRate)
        {
            return WorldizerAssetSynth::synthesizeRoomTone (id, sampleRate);
        }
    };

    // Export the material library (code is the source of truth — MaterialResolver;
    // this JSON is the human-readable/interop mirror in Resources/Materials/).
    {
        const auto matDir = root.getChildFile ("Materials");
        matDir.createDirectory();
        juce::Array<juce::var> mats;
        for (const auto& name : Worldizer::MaterialResolver::getKnownNames())
        {
            const auto m = Worldizer::MaterialResolver::resolve (name);
            auto* o = new juce::DynamicObject();
            o->setProperty ("name", name);
            juce::Array<juce::var> abs;
            for (int b = 0; b < Worldizer::Material::kNumBands; ++b)
                abs.add (m.getAbsorption (b));
            o->setProperty ("absorption", abs);       // 62.5, 125, 250, 500, 1k, 2k, 4k, 8k Hz
            o->setProperty ("scattering", m.getScattering());
            mats.add (juce::var (o));
        }
        auto* rootObj = new juce::DynamicObject();
        rootObj->setProperty ("format", "worldizer-materials-v1");
        rootObj->setProperty ("bands_hz", [] { juce::Array<juce::var> b;
            for (auto f : { 62.5, 125.0, 250.0, 500.0, 1000.0, 2000.0, 4000.0, 8000.0 }) b.add (f);
            return b; }());
        rootObj->setProperty ("materials", mats);
        const auto out = matDir.getChildFile ("materials.json");
        out.replaceWithText (juce::JSON::toString (juce::var (rootObj)));
        std::cout << "Wrote " << out.getFullPathName() << " ("
                  << Worldizer::MaterialResolver::getKnownNames().size() << " materials)\n";
    }

    std::cout << "Baking speaker character IRs...\n";
    bakeList (Worldizer::CharacterLibrary::speakers(), "Speakers", &WorldizerAssetSynth::synthesizeSpeakerIR);
    std::cout << "Baking mic character IRs...\n";
    bakeList (Worldizer::CharacterLibrary::mics(), "Mics", &WorldizerAssetSynth::synthesizeMicIR);
    std::cout << "Baking ambient room-tone beds...\n";
    bakeList (Worldizer::CharacterLibrary::roomTones(), "RoomTones", &ToneSynth::synth);

    std::cout << written << " assets written";
    if (failed > 0)
        std::cout << ", " << failed << " FAILED";
    std::cout << "\nRe-run cmake / rebuild to embed them (CONFIGURE_DEPENDS glob).\n";
    return failed == 0 ? 0 : 1;
}
