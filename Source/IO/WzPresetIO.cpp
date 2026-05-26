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
}

//==============================================================================
juce::var WzPresetIO::sceneToJson (const Scene& scene)
{
    auto* root = new juce::DynamicObject();
    root->setProperty ("$schema", "worldizer-geometry-v1");
    root->setProperty ("version", 1);

    const auto bounds = scene.getBounds();
    auto* b = new juce::DynamicObject();
    b->setProperty ("min", vec3ToJson (bounds.first));
    b->setProperty ("max", vec3ToJson (bounds.second));
    root->setProperty ("bounds", juce::var (b));

    juce::Array<juce::var> brushes;
    for (const auto& br : scene.getBrushes())
    {
        auto* bo = new juce::DynamicObject();
        bo->setProperty ("id", br.getId());
        bo->setProperty ("kind", br.getKind() == Brush::Kind::Additive ? "additive" : "subtractive");
        bo->setProperty ("type", "box");
        bo->setProperty ("min", vec3ToJson (br.getMin()));
        bo->setProperty ("max", vec3ToJson (br.getMax()));
        bo->setProperty ("material", br.getFaceMaterial (Brush::Face::NegX).getName());
        brushes.add (juce::var (bo));
    }
    root->setProperty ("brushes", brushes);

    auto* src = new juce::DynamicObject();
    src->setProperty ("position", vec3ToJson (scene.getSource().getPosition()));
    root->setProperty ("source_default", juce::var (src));

    auto* mic = new juce::DynamicObject();
    mic->setProperty ("position", vec3ToJson (scene.getMic().getPosition()));
    mic->setProperty ("radius", (double) scene.getMic().getRadius());
    root->setProperty ("mic_default", juce::var (mic));

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

    auto* brushes = v["brushes"].getArray();
    if (brushes == nullptr)
    {
        errorOut = "geometry.json: missing 'brushes' array";
        return scene;
    }

    for (auto& bv : *brushes)
    {
        const juce::String id    = bv["id"].toString();
        const juce::String kindS = bv["kind"].toString();
        const auto kind = (kindS == "subtractive") ? Brush::Kind::Subtractive : Brush::Kind::Additive;

        Brush brush (id, vec3FromJson (bv["min"]), vec3FromJson (bv["max"]), kind);
        brush.setAllFaceMaterials (MaterialResolver::resolve (bv["material"].toString()));
        scene.addBrush (std::move (brush));
    }

    const auto src = v["source_default"];
    if (src.isObject())
        scene.getSource().setPosition (vec3FromJson (src["position"]));

    const auto mic = v["mic_default"];
    if (mic.isObject())
    {
        scene.getMic().setPosition (vec3FromJson (mic["position"]));
        if (mic.hasProperty ("radius"))
            scene.getMic().setRadius ((float) (double) mic["radius"]);
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
