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
