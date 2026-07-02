#include "CharacterLibrary.h"

#ifndef WORLDIZER_HAS_CHARACTERS
 #define WORLDIZER_HAS_CHARACTERS 0
#endif

#if WORLDIZER_HAS_CHARACTERS
 #include "WorldizerCharacters.h"
#endif

namespace Worldizer
{
// Ids double as filename stems (Resources/<kind>/<id>.wav) and binary-data symbol
// stems (<id>_wav). The spk_/mic_/amb_ prefixes keep the symbols collision-free
// inside the single WorldizerCharacters binary-data target.

const std::vector<CharacterDef>& CharacterLibrary::speakers()
{
    static const std::vector<CharacterDef> defs = {
        { "none",                "None (Direct)",     "No reproducer coloration - the source feeds the room directly" },
        { "spk_fullrange_pa",    "Full-Range PA",     "Clean playback speaker; near-flat with a light cabinet signature" },
        { "spk_guitar_cab",      "Guitar Cab 1x12",   "Closed-back combo cab; thick low-mids, bite at 2.5 kHz, dark top" },
        { "spk_transistor_radio","Transistor Radio",  "Palm-sized tinny box; no lows, honky presence peak" },
        { "spk_telephone",       "Telephone Handset", "Classic 300 Hz-3.4 kHz telephone band with a 2 kHz presence lift" },
        { "spk_megaphone",       "Megaphone",         "Horn-loaded bullhorn; aggressive midrange honk, no extremes" },
        { "spk_vintage_tv",      "Vintage TV",        "60s console television; boxy lower-mids, rolled-off top" },
        { "spk_intercom",        "Intercom",          "Wall-panel talkback speaker; narrow, peaky, institutional" },
        { "spk_car_speaker",     "Car Door Speaker",  "Door-mounted coaxial heard off-axis; boomy 150 Hz, scooped mids" },
    };
    return defs;
}

const std::vector<CharacterDef>& CharacterLibrary::mics()
{
    static const std::vector<CharacterDef> defs = {
        { "none",                 "None (Direct)",     "No capture coloration - the room feeds the output directly" },
        { "mic_studio_condenser", "Studio Condenser",  "Reference large-diaphragm condenser; flat with a gentle air lift" },
        { "mic_dynamic_stage",    "Stage Dynamic",     "Handheld stage vocal mic; presence bump, rolled lows" },
        { "mic_ribbon_vintage",   "Vintage Ribbon",    "Classic ribbon; dark silky top, warm low-mids" },
        { "mic_lavalier",         "Lavalier",          "Chest-worn lav; mid dip from clothing, crisped consonants" },
        { "mic_carbon_telephone", "Carbon Button",     "Early telephone carbon capsule; narrow, resonant, gritty" },
        { "mic_contact",          "Contact Mic",       "Surface transducer; no air sound, strongly resonant body tones" },
    };
    return defs;
}

const std::vector<CharacterDef>& CharacterLibrary::roomTones()
{
    static const std::vector<CharacterDef> defs = {
        { "amb_room_hvac",   "HVAC Interior",    "Broadband ventilation rumble with 60/120 Hz electrical hum" },
        { "amb_small_room",  "Quiet Room",       "Near-silent domestic room tone; faint mains and air" },
        { "amb_outdoor_air", "Outdoor Air",      "Open-air presence with slow wind swells" },
        { "amb_city_rumble", "City Rumble",      "Distant traffic bed; deep rumble and blurred mid murmur" },
        { "amb_fluorescent", "Fluorescent Hum",  "120 Hz ballast buzz with harmonics and fixture hiss" },
        { "amb_tape_hiss",   "Tape Hiss",        "Analog playback-chain hiss; the re-recorded signal path itself" },
    };
    return defs;
}

juce::StringArray CharacterLibrary::speakerNames()
{
    juce::StringArray names;
    for (const auto& d : speakers())
        names.add (d.name);
    return names;
}

juce::StringArray CharacterLibrary::micNames()
{
    juce::StringArray names;
    for (const auto& d : mics())
        names.add (d.name);
    return names;
}

static int indexForIdIn (const std::vector<CharacterDef>& defs, const juce::String& id)
{
    for (size_t i = 0; i < defs.size(); ++i)
        if (id == defs[i].id)
            return (int) i;
    return -1;
}

int CharacterLibrary::speakerIndexForId (const juce::String& id)  { return indexForIdIn (speakers(),  id); }
int CharacterLibrary::micIndexForId (const juce::String& id)      { return indexForIdIn (mics(),      id); }
int CharacterLibrary::roomToneIndexForId (const juce::String& id) { return indexForIdIn (roomTones(), id); }

bool CharacterLibrary::loadEmbeddedWav (const juce::String& resourceStem,
                                        juce::AudioBuffer<float>& out, double& sampleRateOut)
{
#if WORLDIZER_HAS_CHARACTERS
    const auto symbol = resourceStem + "_wav";
    int dataSize = 0;
    const char* data = WorldizerCharacters::getNamedResource (symbol.toRawUTF8(), dataSize);
    if (data == nullptr || dataSize <= 0)
    {
        juce::Logger::writeToLog ("CharacterLibrary: no embedded asset for '" + resourceStem + "'");
        return false;
    }

    juce::WavAudioFormat wav;
    auto* stream = new juce::MemoryInputStream (data, (size_t) dataSize, false);
    std::unique_ptr<juce::AudioFormatReader> reader (wav.createReaderFor (stream, true));
    if (reader == nullptr || reader->lengthInSamples <= 0)
        return false;

    out.setSize ((int) reader->numChannels, (int) reader->lengthInSamples);
    reader->read (&out, 0, (int) reader->lengthInSamples, 0, true, true);
    sampleRateOut = reader->sampleRate;
    return true;
#else
    juce::ignoreUnused (resourceStem, out, sampleRateOut);
    return false;
#endif
}

static bool loadFromList (const std::vector<CharacterDef>& defs, const juce::String& id,
                          juce::AudioBuffer<float>& out, double& sampleRateOut,
                          bool (*loader) (const juce::String&, juce::AudioBuffer<float>&, double&))
{
    if (id == "none" || indexForIdIn (defs, id) < 0)
        return false;
    return loader (id, out, sampleRateOut);
}

bool CharacterLibrary::loadSpeakerIR (const juce::String& id, juce::AudioBuffer<float>& out, double& sampleRateOut)
{
    return loadFromList (speakers(), id, out, sampleRateOut, &CharacterLibrary::loadEmbeddedWav);
}

bool CharacterLibrary::loadMicIR (const juce::String& id, juce::AudioBuffer<float>& out, double& sampleRateOut)
{
    return loadFromList (mics(), id, out, sampleRateOut, &CharacterLibrary::loadEmbeddedWav);
}

bool CharacterLibrary::loadRoomTone (const juce::String& id, juce::AudioBuffer<float>& out, double& sampleRateOut)
{
    return loadFromList (roomTones(), id, out, sampleRateOut, &CharacterLibrary::loadEmbeddedWav);
}
} // namespace Worldizer
