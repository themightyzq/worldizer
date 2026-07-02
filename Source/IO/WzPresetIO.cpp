#include "WzPresetIO.h"
#include "../Model/MaterialResolver.h"
#include <map>
#include <cstring>

namespace Worldizer
{
namespace
{
    juce::var vec3ToJson (Vec3 v)
    {
        juce::Array<juce::var> a;
        a.add ((double) v.x); a.add ((double) v.y); a.add ((double) v.z);
        return a;
    }

    Vec3 vec3FromJson (const juce::var& v)
    {
        if (v.isArray() && v.size() >= 3)
            return { (float) (double) v[0], (float) (double) v[1], (float) (double) v[2] };
        return {};
    }

    // === Pattern / configuration <-> string (geometry v2) ===
    const char* sourcePatternToStr (SourcePattern) { return "omnidirectional"; } // only one for MVP

    SourcePattern sourcePatternFromStr (const juce::String&) { return SourcePattern::Omnidirectional; }

    const char* micPatternToStr (MicPattern p)
    {
        return p == MicPattern::Shotgun ? "shotgun" : "omnidirectional";
    }

    MicPattern micPatternFromStr (const juce::String& s)
    {
        return s == "shotgun" ? MicPattern::Shotgun : MicPattern::Omnidirectional;
    }

    const char* micConfigToStr (MicArray::Configuration c)
    {
        switch (c)
        {
            case MicArray::Configuration::StereoXY:   return "stereo_xy";
            case MicArray::Configuration::SpacedPair: return "spaced_pair";
            case MicArray::Configuration::Single:
            default:                                  return "single";
        }
    }

    MicArray::Configuration micConfigFromStr (const juce::String& s)
    {
        if (s == "stereo_xy")   return MicArray::Configuration::StereoXY;
        if (s == "spaced_pair") return MicArray::Configuration::SpacedPair;
        return MicArray::Configuration::Single;
    }

    juce::var micToJson (const MicNode& mic)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("position",    vec3ToJson (mic.getPosition()));
        o->setProperty ("orientation", vec3ToJson (mic.getOrientation()));
        o->setProperty ("radius",      (double) mic.getRadius());
        return juce::var (o);
    }
}

//==============================================================================
juce::var WzPresetIO::sceneToJson (const Scene& scene)
{
    auto* root = new juce::DynamicObject();
    root->setProperty ("$schema", "worldizer-geometry-v3");
    root->setProperty ("version", 3);

    const auto bounds = scene.getBounds();
    auto* b = new juce::DynamicObject();
    b->setProperty ("min", vec3ToJson (bounds.first));
    b->setProperty ("max", vec3ToJson (bounds.second));
    root->setProperty ("bounds", juce::var (b));

    // Manual / legacy brushes (Test scenes, anything not user-authored via sectors).
    // Only the original Box type is serialised here — OrientedWalls only exist as the
    // compiled output of sector geometry and round-trip via `sector_geometry` below.
    juce::Array<juce::var> brushesArr;
    for (const auto& br : scene.getBrushes())
    {
        if (br.getType() != Brush::Type::Box)
            continue;
        auto* bo = new juce::DynamicObject();
        bo->setProperty ("id", br.getId());
        bo->setProperty ("kind", br.getKind() == Brush::Kind::Additive ? "additive" : "subtractive");
        bo->setProperty ("type", "box");
        bo->setProperty ("min", vec3ToJson (br.getMin()));
        bo->setProperty ("max", vec3ToJson (br.getMax()));
        bo->setProperty ("material", br.getFaceMaterial (Brush::Face::NegX).getName());
        brushesArr.add (juce::var (bo));
    }
    root->setProperty ("brushes", brushesArr);

    // Sector geometry (v3): the user-authored path. Empty for legacy presets.
    root->setProperty ("sector_geometry", scene.getSectorGeometry().toJson());

    // source_default (v2: + orientation + pattern).
    const auto& source = scene.getSource();
    auto* src = new juce::DynamicObject();
    src->setProperty ("position",    vec3ToJson (source.getPosition()));
    src->setProperty ("orientation", vec3ToJson (source.getOrientation()));
    src->setProperty ("pattern",     sourcePatternToStr (source.getPattern()));
    root->setProperty ("source_default", juce::var (src));

    // mic_array_default (v2: configuration + uniform pattern + per-mic + XY angle).
    const auto& arr = scene.getMicArray();
    auto* micArr = new juce::DynamicObject();
    micArr->setProperty ("configuration",    micConfigToStr (arr.getConfiguration()));
    micArr->setProperty ("pattern",          micPatternToStr (arr.getPattern()));
    micArr->setProperty ("xy_angle_degrees", (double) arr.getXYAngleDegrees());
    micArr->setProperty ("xy_orientation",   vec3ToJson (arr.getXYOrientation()));

    juce::Array<juce::var> mics;
    for (int m = 0; m < arr.getNumMics(); ++m)
        mics.add (micToJson (arr.getMic (m)));
    micArr->setProperty ("mics", mics);
    root->setProperty ("mic_array_default", juce::var (micArr));

    return juce::var (root);
}

Scene WzPresetIO::sceneFromJson (const juce::var& v, juce::String& errorOut)
{
    Scene scene;
    if (! v.isObject())
    {
        errorOut = "geometry.json: root is not an object";
        return scene;
    }

    // Legacy / Test-preset brushes (v1+). A v3 preset may omit `brushes` entirely if
    // it's pure-sector — accept that. (v1/v2 always had a brushes array.)
    if (auto* brushArr = v["brushes"].getArray())
    {
        for (auto& bv : *brushArr)
        {
            const juce::String id    = bv["id"].toString();
            const juce::String kindS = bv["kind"].toString();
            const auto kind = (kindS == "subtractive") ? Brush::Kind::Subtractive : Brush::Kind::Additive;

            Brush brush (id, vec3FromJson (bv["min"]), vec3FromJson (bv["max"]), kind);
            brush.setAllFaceMaterials (MaterialResolver::resolve (bv["material"].toString()));
            scene.addBrush (std::move (brush));
        }
    }

    // Sector geometry (v3+). Missing => empty (legacy preset path).
    if (v.hasProperty ("sector_geometry"))
    {
        juce::String sErr;
        scene.getSectorGeometry().fromJson (v["sector_geometry"], sErr);
        // (fromJson never fails today — a missing/empty block is valid.)
    }

    // --- Source (v2 adds orientation + pattern; v1 only had position) ---
    const auto src = v["source_default"];
    if (src.isObject())
    {
        scene.getSource().setPosition (vec3FromJson (src["position"]));
        if (src.hasProperty ("orientation"))
            scene.getSource().setOrientation (vec3FromJson (src["orientation"]));
        if (src["pattern"].isString())
            scene.getSource().setPattern (sourcePatternFromStr (src["pattern"].toString()));
    }

    // --- Mic array: v2 has "mic_array_default"; v1 had a single "mic_default" ---
    auto& arr = scene.getMicArray();
    const auto micArr = v["mic_array_default"];
    if (micArr.isObject())
    {
        // v2.
        const auto config  = micConfigFromStr (micArr["configuration"].toString());
        const auto pattern = micPatternFromStr (micArr["pattern"].toString());

        auto* mics = micArr["mics"].getArray();
        auto readMic = [&] (int idx, MicNode& dest)
        {
            if (mics != nullptr && idx < mics->size())
            {
                const auto& mv = (*mics)[idx];
                dest.setPosition (vec3FromJson (mv["position"]));
                if (mv.hasProperty ("orientation")) dest.setOrientation (vec3FromJson (mv["orientation"]));
                if (mv.hasProperty ("radius"))      dest.setRadius ((float) (double) mv["radius"]);
            }
        };

        arr.setConfiguration (config);
        switch (config)
        {
            case MicArray::Configuration::Single:
                readMic (0, arr.getMic (0));
                break;

            case MicArray::Configuration::StereoXY:
            {
                if (micArr.hasProperty ("xy_angle_degrees"))
                    arr.setXYAngleDegrees ((float) (double) micArr["xy_angle_degrees"]);
                // Coincident: take position from mic 0; facing from xy_orientation
                // (orientations are derived from facing + splay).
                if (mics != nullptr && mics->size() > 0)
                    arr.setXYPosition (vec3FromJson ((*mics)[0]["position"]));
                if (micArr.hasProperty ("xy_orientation"))
                    arr.setXYOrientation (vec3FromJson (micArr["xy_orientation"]));
                break;
            }

            case MicArray::Configuration::SpacedPair:
                readMic (0, arr.getMic (0));
                readMic (1, arr.getMic (1));
                break;
        }
        arr.setAllPatterns (pattern);
    }
    else
    {
        // v1 migration: a single, omnidirectional mic.
        const auto mic = v["mic_default"];
        arr.setConfiguration (MicArray::Configuration::Single);
        arr.setAllPatterns (MicPattern::Omnidirectional);
        if (mic.isObject())
        {
            arr.getMic (0).setPosition (vec3FromJson (mic["position"]));
            if (mic.hasProperty ("radius"))
                arr.getMic (0).setRadius ((float) (double) mic["radius"]);
        }
    }

    errorOut.clear();
    return scene;
}

//==============================================================================
juce::var WzPresetIO::metadataToJson (const Loaded& m)
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("$schema", "worldizer-metadata-v1");
    o->setProperty ("version", 1);
    o->setProperty ("name", m.name);
    o->setProperty ("category", m.category);
    o->setProperty ("description", m.description);
    o->setProperty ("author", m.author);

    juce::Array<juce::var> tags;
    for (const auto& t : m.tags)
        tags.add (t);
    o->setProperty ("tags", tags);

    o->setProperty ("ambient_bed",              m.ambientBed.isEmpty()             ? juce::var() : juce::var (m.ambientBed));
    if (m.ambientBed.isNotEmpty())
        o->setProperty ("ambient_level_db", m.ambientLevelDb);
    o->setProperty ("default_source_character", m.defaultSourceCharacter.isEmpty() ? juce::var() : juce::var (m.defaultSourceCharacter));
    o->setProperty ("default_mic_character",    m.defaultMicCharacter.isEmpty()    ? juce::var() : juce::var (m.defaultMicCharacter));
    o->setProperty ("rendered_at", m.renderedAt);

    auto* rs = new juce::DynamicObject();
    rs->setProperty ("num_rays",    m.renderNumRays);
    rs->setProperty ("max_bounces", m.renderMaxBounces);
    rs->setProperty ("sample_rate", m.renderSampleRate);
    rs->setProperty ("random_seed", m.renderSeed);
    o->setProperty ("render_settings", juce::var (rs));

    return juce::var (o);
}

void WzPresetIO::metadataFromJson (const juce::var& v, Loaded& m)
{
    m.name        = v.getProperty ("name", m.presetId).toString();
    m.category    = v.getProperty ("category", "").toString();
    m.description = v.getProperty ("description", "").toString();
    m.author      = v.getProperty ("author", "").toString();

    m.tags.clear();
    if (auto* arr = v["tags"].getArray())
        for (auto& t : *arr)
            m.tags.add (t.toString());

    if (v["ambient_bed"].isString())              m.ambientBed             = v["ambient_bed"].toString();
    if (v.hasProperty ("ambient_level_db"))       m.ambientLevelDb         = (float) (double) v["ambient_level_db"];
    if (v["default_source_character"].isString()) m.defaultSourceCharacter = v["default_source_character"].toString();
    if (v["default_mic_character"].isString())    m.defaultMicCharacter    = v["default_mic_character"].toString();

    m.renderedAt = v.getProperty ("rendered_at", "").toString();
    const auto rs = v["render_settings"];
    if (rs.isObject())
    {
        m.renderNumRays    = (int) rs.getProperty ("num_rays", 0);
        m.renderMaxBounces = (int) rs.getProperty ("max_bounces", 0);
        m.renderSampleRate = (int) rs.getProperty ("sample_rate", 0);
        m.renderSeed       = (int) rs.getProperty ("random_seed", 0);
    }
}

//==============================================================================
juce::Image WzPresetIO::makePlaceholderThumbnail (const juce::String& name)
{
    juce::Image img (juce::Image::ARGB, 128, 128, true);
    juce::Graphics g (img);
    g.fillAll (juce::Colour (0xff2a2a2a));
    g.setColour (juce::Colour (0xffffab00));
    g.drawRect (4, 4, 120, 120, 1);
    g.setFont (juce::Font (juce::FontOptions (13.0f).withStyle ("Bold")));
    g.drawFittedText (name, 10, 10, 108, 108, juce::Justification::centred, 3);
    return img;
}

bool WzPresetIO::writeWavFile (const juce::File& file, const juce::AudioBuffer<float>& ir, double sr, juce::String& errorOut)
{
    file.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> stream = file.createOutputStream();
    if (stream == nullptr)
    {
        errorOut = "cannot open " + file.getFullPathName();
        return false;
    }

    if (auto writer = wav.createWriterFor (stream,
                          juce::AudioFormatWriterOptions()
                              .withSampleRate (sr)
                              .withNumChannels (juce::jmax (1, ir.getNumChannels()))
                              .withBitsPerSample (32)
                              .withSampleFormat (juce::AudioFormatWriterOptions::SampleFormat::floatingPoint)))
    {
        if (! writer->writeFromAudioSampleBuffer (ir, 0, ir.getNumSamples()))
        {
            errorOut = "WAV write failed";
            return false;
        }
        return true;
    }

    errorOut = "could not create WAV writer";
    return false;
}

//==============================================================================
std::optional<WzPresetIO::Loaded> WzPresetIO::readBundle (const juce::File& bundleDir, juce::String& errorOut)
{
    if (! bundleDir.isDirectory())
    {
        errorOut = bundleDir.getFullPathName() + ": not a directory";
        return {};
    }

    Loaded out;
    out.presetId = bundleDir.getFileNameWithoutExtension();

    const auto metaFile = bundleDir.getChildFile ("metadata.json");
    if (metaFile.existsAsFile())
    {
        const auto mv = juce::JSON::parse (metaFile.loadFileAsString());
        if (mv.isObject())
            metadataFromJson (mv, out);
    }
    if (out.name.isEmpty())
        out.name = out.presetId;

    const auto geoFile = bundleDir.getChildFile ("geometry.json");
    if (! geoFile.existsAsFile())
    {
        errorOut = out.presetId + ": missing geometry.json";
        return {};
    }
    juce::String gErr;
    out.scene = sceneFromJson (juce::JSON::parse (geoFile.loadFileAsString()), gErr);
    if (gErr.isNotEmpty())
    {
        errorOut = out.presetId + ": " + gErr;
        return {};
    }

    const auto wavFile = bundleDir.getChildFile ("rendered.wav");
    if (wavFile.existsAsFile())
    {
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::AudioFormatReader> reader (wav.createReaderFor (new juce::FileInputStream (wavFile), true));
        if (reader != nullptr && reader->lengthInSamples > 0)
        {
            out.ir.setSize ((int) juce::jmax ((juce::uint32) 1, reader->numChannels), (int) reader->lengthInSamples);
            reader->read (&out.ir, 0, (int) reader->lengthInSamples, 0, true, true);
            out.irSampleRate = reader->sampleRate;
        }
    }
    else
    {
        juce::Logger::writeToLog ("Preset " + out.presetId + ": missing rendered.wav (will need re-render)");
    }

    const auto thumbFile = bundleDir.getChildFile ("thumbnail.png");
    if (thumbFile.existsAsFile())
        out.thumbnail = juce::ImageFileFormat::loadFrom (thumbFile);

    errorOut.clear();
    return out;
}

std::optional<WzPresetIO::Loaded> WzPresetIO::readMetadataOnly (const juce::File& bundleDir, juce::String& errorOut)
{
    if (! bundleDir.isDirectory())
    {
        errorOut = bundleDir.getFullPathName() + ": not a directory";
        return {};
    }

    Loaded out;
    out.presetId = bundleDir.getFileNameWithoutExtension();

    const auto metaFile = bundleDir.getChildFile ("metadata.json");
    if (metaFile.existsAsFile())
    {
        const auto mv = juce::JSON::parse (metaFile.loadFileAsString());
        if (mv.isObject())
            metadataFromJson (mv, out);
    }
    if (out.name.isEmpty())
        out.name = out.presetId;

    // Also load the small bits the browser needs (geometry + thumbnail), but NOT
    // the large rendered.wav — that loads on demand via readBundle().
    const auto geoFile = bundleDir.getChildFile ("geometry.json");
    if (geoFile.existsAsFile())
    {
        juce::String gErr;
        out.scene = sceneFromJson (juce::JSON::parse (geoFile.loadFileAsString()), gErr);
    }

    const auto thumbFile = bundleDir.getChildFile ("thumbnail.png");
    if (thumbFile.existsAsFile())
        out.thumbnail = juce::ImageFileFormat::loadFrom (thumbFile);

    errorOut.clear();
    return out;
}

bool WzPresetIO::writeBundle (const juce::File& bundleDir, const Scene& scene,
                              const juce::AudioBuffer<float>& ir, double irSampleRate,
                              const Loaded& metadata, juce::String& errorOut)
{
    if (! bundleDir.createDirectory())
    {
        errorOut = "cannot create " + bundleDir.getFullPathName();
        return false;
    }

    if (! bundleDir.getChildFile ("geometry.json").replaceWithText (juce::JSON::toString (sceneToJson (scene), false)))
    {
        errorOut = "write geometry.json failed";
        return false;
    }

    Loaded m = metadata;
    if (m.presetId.isEmpty())   m.presetId = bundleDir.getFileNameWithoutExtension();
    if (m.name.isEmpty())       m.name = m.presetId;
    if (m.renderedAt.isEmpty()) m.renderedAt = juce::Time::getCurrentTime().toISO8601 (true);

    if (! bundleDir.getChildFile ("metadata.json").replaceWithText (juce::JSON::toString (metadataToJson (m), false)))
    {
        errorOut = "write metadata.json failed";
        return false;
    }

    if (! writeWavFile (bundleDir.getChildFile ("rendered.wav"), ir, irSampleRate, errorOut))
        return false;

    {
        const auto img = metadata.thumbnail.isValid() ? metadata.thumbnail : makePlaceholderThumbnail (m.name);
        auto pf = bundleDir.getChildFile ("thumbnail.png");
        pf.deleteFile();
        if (auto os = pf.createOutputStream())
        {
            juce::PNGImageFormat png;
            png.writeImageToStream (img, *os);
        }
    }

    errorOut.clear();
    return true;
}

juce::Array<juce::File> WzPresetIO::findPresetsIn (const juce::File& directory)
{
    juce::Array<juce::File> out;
    if (directory.isDirectory())
        for (auto& f : directory.findChildFiles (juce::File::findDirectories, false, "*.wzpreset"))
            out.add (f);
    return out;
}

//==============================================================================
bool WzPresetIO::packBundle (const juce::File& bundleDir, const juce::File& outPkgFile, juce::String& errorOut)
{
    const char* names[] = { "geometry.json", "metadata.json", "rendered.wav", "thumbnail.png" };

    juce::MemoryOutputStream mos;
    mos.write ("WZP1", 4);
    mos.writeInt ((int) 4);

    for (auto* n : names)
    {
        juce::MemoryBlock data;
        const auto f = bundleDir.getChildFile (n);
        if (f.existsAsFile())
            f.loadFileAsData (data);

        const juce::String nm (n);
        mos.writeInt ((int) nm.getNumBytesAsUTF8());
        mos.write (nm.toRawUTF8(), nm.getNumBytesAsUTF8());
        mos.writeInt ((int) data.getSize());
        if (data.getSize() > 0)
            mos.write (data.getData(), data.getSize());
    }

    outPkgFile.deleteFile();
    if (! outPkgFile.replaceWithData (mos.getData(), mos.getDataSize()))
    {
        errorOut = "pack write failed: " + outPkgFile.getFullPathName();
        return false;
    }
    return true;
}

std::optional<WzPresetIO::Loaded> WzPresetIO::readFromBinaryData (const void* data, size_t size, juce::String& errorOut)
{
    juce::MemoryInputStream in (data, size, false);

    char magic[4] = {};
    if (in.read (magic, 4) != 4 || std::memcmp (magic, "WZP1", 4) != 0)
    {
        errorOut = "bad .wzpkg magic";
        return {};
    }

    const int numEntries = in.readInt();
    std::map<juce::String, juce::MemoryBlock> entries;
    for (int i = 0; i < numEntries; ++i)
    {
        const int nameLen = in.readInt();
        juce::MemoryBlock nameBytes;
        in.readIntoMemoryBlock (nameBytes, nameLen);
        const juce::String name = juce::String::fromUTF8 ((const char*) nameBytes.getData(), (int) nameBytes.getSize());

        const int dataLen = in.readInt();
        juce::MemoryBlock blk;
        if (dataLen > 0)
            in.readIntoMemoryBlock (blk, dataLen);
        entries[name] = std::move (blk);
    }

    Loaded out;

    if (entries.count ("metadata.json"))
    {
        const auto mv = juce::JSON::parse (entries["metadata.json"].toString());
        if (mv.isObject())
            metadataFromJson (mv, out);
    }

    if (entries.count ("geometry.json"))
    {
        juce::String gErr;
        out.scene = sceneFromJson (juce::JSON::parse (entries["geometry.json"].toString()), gErr);
    }

    if (entries.count ("rendered.wav") && entries["rendered.wav"].getSize() > 0)
    {
        auto& blk = entries["rendered.wav"];
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::AudioFormatReader> reader (
            wav.createReaderFor (new juce::MemoryInputStream (blk.getData(), blk.getSize(), false), true));
        if (reader != nullptr && reader->lengthInSamples > 0)
        {
            out.ir.setSize ((int) juce::jmax ((juce::uint32) 1, reader->numChannels), (int) reader->lengthInSamples);
            reader->read (&out.ir, 0, (int) reader->lengthInSamples, 0, true, true);
            out.irSampleRate = reader->sampleRate;
        }
    }

    if (entries.count ("thumbnail.png") && entries["thumbnail.png"].getSize() > 0)
    {
        auto& blk = entries["thumbnail.png"];
        out.thumbnail = juce::ImageFileFormat::loadFrom (blk.getData(), blk.getSize());
    }

    if (out.name.isEmpty())
        out.name = out.presetId;

    errorOut.clear();
    return out;
}
} // namespace Worldizer
