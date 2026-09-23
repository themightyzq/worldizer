#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "WorldizerBinaryData.h"
#include "../DSP/RayTracer.h"
#include "../DSP/IRBuilder.h"
#include "../DSP/CharacterLibrary.h"
#include "../UI/RoomView2D.h"
#include <cmath>

using namespace Worldizer;

namespace
{
    // The room a fresh instance opens with, so inserting the plugin does something
    // audible and representative rather than nothing. "hallway" is the archetypal
    // worldizing space: obviously a real room, but moderate enough not to swamp the
    // source. Change this one id to pick a different opening room; any directory name
    // under Resources/Presets/ (minus the .wzpreset suffix) is valid.
    const juce::String kDefaultPresetId { "hallway" };

    // Transparent output safety ceiling. Below the knee (~-1.5 dBFS) it is bit-exact
    // (returns the input unchanged) so it never colours normal-level material; above
    // the knee it soft-saturates and asymptotes to +-1.0, so convolution peaks / dense
    // overlapping tails can't push the output past full scale and "overwhelm the
    // mixer". Instantaneous waveshaping (no time-varying gain) => it CANNOT pump, and
    // adds no latency — unlike a compressor/limiter, which is exactly what we don't want.
    constexpr float kSoftClipKnee = 0.84f; // ~-1.5 dBFS; above this the ceiling engages
    inline float softClip (float x) noexcept
    {
        constexpr float knee = kSoftClipKnee;
        const float a = std::abs (x);
        if (a <= knee)
            return x;
        const float s = (a - knee) / (1.0f - knee);
        return std::copysign (knee + (1.0f - knee) * std::tanh (s), x);
    }

    // Normalises a mono buffer to a target RMS measured over its ACTIVE region
    // (samples above -26 dB of the peak). This matches perceived loudness across
    // signals of different density — sparse clicks vs. a sustained sweep/noise —
    // far better than peak normalisation would. A peak ceiling prevents clipping.
    void normalizeActiveRms (juce::AudioBuffer<float>& b, float targetRms, float peakCeiling)
    {
        if (b.getNumSamples() <= 0)
            return;

        auto* d = b.getWritePointer (0);
        const int n = b.getNumSamples();

        float peak = 0.0f;
        for (int i = 0; i < n; ++i)
            peak = juce::jmax (peak, std::abs (d[i]));
        if (peak <= 0.0f)
            return;

        const float threshold = peak * 0.05f; // -26 dB of peak counts as "active"
        double sumSq = 0.0;
        int count = 0;
        for (int i = 0; i < n; ++i)
            if (std::abs (d[i]) > threshold) { sumSq += (double) d[i] * (double) d[i]; ++count; }

        if (count == 0)
            return;

        const float rms = (float) std::sqrt (sumSq / (double) count);
        if (rms <= 0.0f)
            return;

        float gain = targetRms / rms;
        if (peak * gain > peakCeiling) // safety: never let a transient clip
            gain = peakCeiling / peak;

        b.applyGain (gain);
    }

    // True if two scenes differ in any ray-traced interactive property (source pos /
    // orientation / pattern, or mic-array config / pattern / angle / per-mic pos /
    // orientation). Used on state restore to decide whether the baked IR is still
    // valid (skip render) or the scene was edited (re-render).
    bool interactiveStateDiffers (const Worldizer::Scene& a, const Worldizer::Scene& b)
    {
        auto vecClose = [] (Worldizer::Vec3 u, Worldizer::Vec3 v) { return (u - v).length() < 1.0e-3f; };

        const auto& sa = a.getSource();
        const auto& sb = b.getSource();
        if (! vecClose (sa.getPosition(), sb.getPosition())) return true;
        if (! vecClose (sa.getOrientation(), sb.getOrientation())) return true;
        if (sa.getPattern() != sb.getPattern()) return true;

        const auto& aa = a.getMicArray();
        const auto& ab = b.getMicArray();
        if (aa.getConfiguration() != ab.getConfiguration()) return true;
        if (aa.getPattern() != ab.getPattern()) return true;
        if (aa.getNumMics() != ab.getNumMics()) return true;
        if (std::abs (aa.getXYAngleDegrees() - ab.getXYAngleDegrees()) > 0.1f) return true;

        for (int m = 0; m < aa.getNumMics(); ++m)
        {
            if (! vecClose (aa.getMic (m).getPosition(),    ab.getMic (m).getPosition()))    return true;
            if (! vecClose (aa.getMic (m).getOrientation(), ab.getMic (m).getOrientation())) return true;
        }
        return false;
    }
}

//==============================================================================
WorldizerAudioProcessor::WorldizerAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    bypassParam = dynamic_cast<juce::AudioParameterBool*> (apvts.getParameter ("bypass"));
    jassert (bypassParam != nullptr);

    sourceCharParam = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter ("sourceCharacter"));
    micCharParam    = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter ("micCharacter"));
    jassert (sourceCharParam != nullptr && micCharParam != nullptr);
    apvts.addParameterListener ("sourceCharacter", this);
    apvts.addParameterListener ("micCharacter", this);

    presetManager = std::make_unique<PresetManager>();
    presetManager->rescan();

    renderThread = std::make_unique<RenderThread> (convolution);

    // Open on a real room. Without this a fresh instance has an empty Scene, so the ray
    // trace returns nothing, the IR degenerates to a single silent sample, and with mix
    // defaulting to 100% wet the plugin is simply silent on insert with no indication why.
    //
    // applyCharacterDefaults=false deliberately, matching the state-restore and
    // prepareToPlay paths: a default load is not a user preset choice and must not push
    // parameter changes to the host from a constructor. If a session is restored,
    // setStateInformation runs after this and overrides it.
    if (presetManager->hasPreset (kDefaultPresetId))
        setCurrentPresetId (kDefaultPresetId, false);

    startTimer (30); // message-thread poll for characterReloadNeeded (see parameterChanged)
}

WorldizerAudioProcessor::~WorldizerAudioProcessor()
{
    stopTimer();
    apvts.removeParameterListener ("sourceCharacter", this);
    apvts.removeParameterListener ("micCharacter", this);
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout WorldizerAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "bypass", 1 }, "Bypass", false));

    auto dbString = [] (float v, int) { return juce::String (v, 1) + " dB"; };

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "inputGain", 1 }, "Input Gain",
        juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction (dbString)));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "outputGain", 1 }, "Output Gain",
        juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction (dbString)));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "mix", 1 }, "Mix",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction (
            [] (float v, int) { return juce::String (v, 1) + " %"; })));

    auto pctString = [] (float v, int) { return juce::String (v, 1) + " %"; };

    // === Slice 5.5: source/mic character chain ===
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "sourceCharacter", 1 }, "Speaker",
        Worldizer::CharacterLibrary::speakerNames(), 0));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "sourceDrive", 1 }, "Drive",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction (pctString)));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "micCharacter", 1 }, "Microphone",
        Worldizer::CharacterLibrary::micNames(), 0));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "micNoise", 1 }, "Self-Noise",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction (pctString)));

    // === Slice 6: ambient bed level ===
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "ambientLevel", 1 }, "Ambient",
        juce::NormalisableRange<float> (-60.0f, 0.0f, 0.1f), -20.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction (
            [] (float v, int) { return v <= -59.5f ? juce::String ("Off") : juce::String (v, 1) + " dB"; })));

    return layout;
}

//==============================================================================
// === Character library loading (Slice 5.5) + ambient bed (Slice 6) ===

void WorldizerAudioProcessor::parameterChanged (const juce::String&, float)
{
    // May fire on the audio thread (host automation) — a lock-free flag is all
    // that is safe here. The 30 ms message-thread timer does the actual load.
    characterReloadNeeded.store (true);
}

void WorldizerAudioProcessor::timerCallback()
{
    if (characterReloadNeeded.exchange (false))
        loadCharactersFromParams();
}

void WorldizerAudioProcessor::loadSourceCharacterByIndex (int index)
{
    const auto& defs = Worldizer::CharacterLibrary::speakers();
    juce::AudioBuffer<float> ir;
    double sr = 48000.0;
    if (index > 0 && index < (int) defs.size()
        && Worldizer::CharacterLibrary::loadSpeakerIR (defs[(size_t) index].id, ir, sr))
        sourceCharacter.setIR (std::move (ir), sr);
    else
        sourceCharacter.setNone();
}

void WorldizerAudioProcessor::loadMicCharacterByIndex (int index)
{
    const auto& defs = Worldizer::CharacterLibrary::mics();
    juce::AudioBuffer<float> ir;
    double sr = 48000.0;
    if (index > 0 && index < (int) defs.size()
        && Worldizer::CharacterLibrary::loadMicIR (defs[(size_t) index].id, ir, sr))
        micCharacter.setIR (std::move (ir), sr);
    else
        micCharacter.setNone();
}

void WorldizerAudioProcessor::loadCharactersFromParams()
{
    loadSourceCharacterByIndex (sourceCharParam != nullptr ? sourceCharParam->getIndex() : 0);
    loadMicCharacterByIndex    (micCharParam    != nullptr ? micCharParam->getIndex()    : 0);
}

void WorldizerAudioProcessor::loadAmbientBedById (const juce::String& bedId)
{
    juce::AudioBuffer<float> bed;
    double sr = 48000.0;
    if (bedId.isNotEmpty() && Worldizer::CharacterLibrary::loadRoomTone (bedId, bed, sr))
        ambientBed.setSample (bed, sr);
    else
        ambientBed.clearSample();
}

void WorldizerAudioProcessor::auditionSourceCharacter (int index)    { loadSourceCharacterByIndex (index); }
void WorldizerAudioProcessor::endSourceCharacterAudition()           { loadSourceCharacterByIndex (sourceCharParam != nullptr ? sourceCharParam->getIndex() : 0); }
void WorldizerAudioProcessor::auditionMicCharacter (int index)       { loadMicCharacterByIndex (index); }
void WorldizerAudioProcessor::endMicCharacterAudition()              { loadMicCharacterByIndex (micCharParam != nullptr ? micCharParam->getIndex() : 0); }

//==============================================================================
juce::String WorldizerAudioProcessor::sceneNameToPresetId (const juce::String& s)
{
    if (s == "smallConcreteRoom") return "small_concrete_room";
    if (s == "forestClearing")    return "forest_clearing";
    if (s == "hallway" || s == "gymnasium" || s == "anechoic") return s;
    return s; // assume it is already a preset id
}

juce::String WorldizerAudioProcessor::getCurrentPresetId() const
{
    const juce::ScopedLock sl (presetLock);
    return currentPresetId;
}

juce::StringArray WorldizerAudioProcessor::getAvailablePresetIds() const
{
    return presetManager != nullptr ? presetManager->getAvailablePresetIds() : juce::StringArray {};
}

juce::Array<Worldizer::WzPresetIO::Loaded> WorldizerAudioProcessor::getAvailablePresetMetadata() const
{
    return presetManager != nullptr ? presetManager->getAvailablePresetMetadata()
                                    : juce::Array<Worldizer::WzPresetIO::Loaded> {};
}

std::optional<Worldizer::WzPresetIO::Loaded> WorldizerAudioProcessor::getCurrentPresetMetadata() const
{
    const juce::ScopedLock sl (presetLock);
    return currentPresetMetadata;
}

void WorldizerAudioProcessor::setCurrentPresetId (const juce::String& presetId, bool applyCharacterDefaults)
{
    if (presetManager == nullptr || ! presetManager->hasPreset (presetId))
    {
        juce::Logger::writeToLog ("Preset not found: " + presetId);
        return;
    }

    juce::String err;
    auto loaded = presetManager->loadPreset (presetId, err);
    if (! loaded.has_value())
    {
        juce::Logger::writeToLog ("Failed to load preset " + presetId + ": " + err);
        return;
    }

    {
        const juce::ScopedLock sl (presetLock);
        currentPresetId = presetId;
        currentPresetMetadata = loaded->withoutIR();
        currentAmbientBedId = loaded->ambientBed;
    }
    sceneRevision.fetch_add (1);
    // Fresh preset load => no uncommitted edits relative to its on-disk defaults.
    dirtyFlag.store (false);
    shellConverted.store (false);

    // Ambient bed follows the preset (crossfades in the audio thread). Character
    // defaults are only applied on USER preset selection — never on state restore
    // or prepareToPlay re-apply, which must preserve the session's own choices.
    loadAmbientBedById (loaded->ambientBed);
    if (applyCharacterDefaults)
    {
        auto setChoice = [] (juce::AudioParameterChoice* param, int index)
        {
            if (param != nullptr && index >= 0)
                param->setValueNotifyingHost (param->convertTo0to1 ((float) index));
        };
        if (loaded->defaultSourceCharacter.isNotEmpty())
            setChoice (sourceCharParam, Worldizer::CharacterLibrary::speakerIndexForId (loaded->defaultSourceCharacter));
        if (loaded->defaultMicCharacter.isNotEmpty())
            setChoice (micCharParam, Worldizer::CharacterLibrary::micIndexForId (loaded->defaultMicCharacter));

        if (loaded->ambientBed.isNotEmpty())
            if (auto* p = apvts.getParameter ("ambientLevel"))
                p->setValueNotifyingHost (p->getNormalisableRange().convertTo0to1 (
                    juce::jlimit (-60.0f, 0.0f, loaded->ambientLevelDb)));
    }

    // Seed the distance model from the preset's default source/mic spacing. The
    // attenuation REFERENCE is set to this default distance, so the preset plays at
    // unity (0 dB) as designed and dragging the mic changes level RELATIVE to that
    // design point — rather than referencing a fixed 1 m, which left every preset
    // (mics are metres away) attenuated by 10-25 dB and feeling too quiet.
    {
        const auto s = loaded->scene.getSource().getPosition();
        const auto m = loaded->scene.getMicArray().getCenterPosition();
        const float distance = (m - s).length();

        auto settings = distanceModel.getSettings();
        settings.referenceDistance = juce::jmax (settings.minDistance, distance);
        distanceModel.setSettings (settings);

        updateDistanceFromSpacing (distance);
    }

    if (loaded->ir.getNumSamples() > 0)
    {
        // A preset load is a file read + convolver swap — no tracing. Normally it
        // goes THROUGH the render thread so the engine only ever has one loading
        // thread (single-producer command queue). During an offline render,
        // though, that async hop would let the bounce head render dry — so load
        // directly (the render thread is drained in prepareToPlay and no audio
        // runs concurrently, so single-producer still holds).
        if (isNonRealtime())
            convolution.loadIR (loaded->ir, loaded->irSampleRate, 5.0f);
        else
            renderThread->requestIRLoad (loaded->ir, loaded->irSampleRate, 80.0f);
    }
    else
    {
        // Recovery path: no baked IR — render from the loaded geometry.
        juce::Logger::writeToLog ("Preset " + presetId + " has no rendered.wav; rendering from geometry");
        if (isNonRealtime())
        {
            const auto ir = renderLiveSceneIR();
            if (ir.getNumSamples() > 0)
                convolution.loadIR (ir, 48000.0, 5.0f);
        }
        else
            renderCurrentScene (true, 80.0f);
    }
}

bool WorldizerAudioProcessor::isRendering() const noexcept
{
    return renderThread != nullptr && renderThread->isRendering();
}

void WorldizerAudioProcessor::notifyCurrentPresetRenamed (const juce::String& oldId, const juce::String& newId,
                                                          const juce::String& newName)
{
    const juce::ScopedLock sl (presetLock);
    if (currentPresetId != oldId)
        return; // the user switched away from it while the rename dialog was open

    currentPresetId = newId;
    if (currentPresetMetadata.has_value())
    {
        currentPresetMetadata->presetId = newId;
        currentPresetMetadata->name     = newName;
    }
}

// Deprecated Slice 2 shims (mapped onto the preset system).
juce::String WorldizerAudioProcessor::getCurrentSceneName() const { return getCurrentPresetId(); }
void WorldizerAudioProcessor::setCurrentSceneName (const juce::String& sceneName) { setCurrentPresetId (sceneNameToPresetId (sceneName)); }

void WorldizerAudioProcessor::renderCurrentScene (bool fullQuality, float crossfadeMs)
{
    if (renderThread == nullptr)
        return;

    RenderThread::Job job;
    {
        const juce::ScopedLock sl (presetLock);
        if (! currentPresetMetadata.has_value())
            return;
        job.scene = currentPresetMetadata->scene;
    }
    job.quality     = fullQuality ? RenderThread::Job::Quality::Full : RenderThread::Job::Quality::Preview;
    job.crossfadeMs = crossfadeMs;
    renderThread->requestRender (job);
}

void WorldizerAudioProcessor::updateDistanceFromSpacing (float distance)
{
    // Distance cues live on the wet path (the IR is distance-independent). This can
    // run on the message thread while audio is processing, so only the ATOMIC
    // targets are written here; processBlock feeds them to the (audio-thread-only)
    // smoothers. SmoothedValue::setTargetValue is not thread-safe.
    currentDistanceMeters.store (distance);
    const double sr = getSampleRate();
    if (sr > 0.0)
    {
        preDelaySamplesTarget.store (distanceModel.computePreDelaySamples (distance, sr));
        attenuationTarget.store (juce::jmax (1.0e-4f, distanceModel.computeAttenuationGain (distance)));
    }
}

void WorldizerAudioProcessor::mutateSceneAndRender (const std::function<void (Worldizer::Scene&)>& edit,
                                                    bool fullQuality, float crossfadeMs)
{
    Worldizer::Vec3 src, center;
    {
        const juce::ScopedLock sl (presetLock);
        if (! currentPresetMetadata.has_value())
            return;
        edit (currentPresetMetadata->scene);
        src    = currentPresetMetadata->scene.getSource().getPosition();
        center = currentPresetMetadata->scene.getMicArray().getCenterPosition();
    }

    // Distance model follows the mic-array CENTRE (single mic: the mic; XY: the array;
    // spaced pair: the midpoint). Inter-channel time/level differences are in the IR.
    sceneRevision.fetch_add (1);
    updateDistanceFromSpacing ((center - src).length());
    renderCurrentScene (fullQuality, crossfadeMs);
}

Worldizer::Scene WorldizerAudioProcessor::getCurrentScene() const
{
    const juce::ScopedLock sl (presetLock);
    return currentPresetMetadata.has_value() ? currentPresetMetadata->scene : Worldizer::Scene {};
}

//==============================================================================
bool WorldizerAudioProcessor::saveCurrentSceneAsPreset (const juce::String& presetName,
                                                        const juce::String& category,
                                                        const juce::String& description,
                                                        const juce::StringArray& tags,
                                                        bool overwriteExisting,
                                                        juce::String& errorOut)
{
    if (presetManager == nullptr)            { errorOut = "no preset manager"; return false; }
    if (presetName.trim().isEmpty())         { errorOut = "preset name is empty"; return false; }

    // Filesystem-safe id from the display name — shared with PresetManager::renameUserPreset
    // so renaming a preset to the same name a Save-As would have used lands on the same id.
    const juce::String id = Worldizer::WzPresetIO::makeSafeId (presetName);

    const auto folder    = Worldizer::PresetManager::getUserPresetsFolder();
    folder.createDirectory(); // lazily create the user library on first write
    const auto bundleDir = folder.getChildFile (id + ".wzpreset");
    if (bundleDir.exists() && ! overwriteExisting) { errorOut = "preset already exists"; return false; }
    if (bundleDir.exists())
        bundleDir.deleteRecursively();

    // Snapshot the live scene + inherited bed/character assignments under lock.
    Worldizer::Scene snapshot;
    juce::String inheritedBed, inheritedSrcChar, inheritedMicChar;
    float inheritedBedLevel = -20.0f;
    {
        const juce::ScopedLock sl (presetLock);
        if (! currentPresetMetadata.has_value()) { errorOut = "no current scene"; return false; }
        snapshot          = currentPresetMetadata->scene;
        inheritedBed      = currentPresetMetadata->ambientBed;
        inheritedBedLevel = currentPresetMetadata->ambientLevelDb;
        inheritedSrcChar  = currentPresetMetadata->defaultSourceCharacter;
        inheritedMicChar  = currentPresetMetadata->defaultMicCharacter;
    }

    // Refuse pathological geometry rather than freeze the UI on a synchronous
    // trace (the tracer is O(brushes) per bounce with no acceleration structure).
    if ((int) snapshot.getAllBrushesForTracing().size() > Worldizer::kMaxTraceBrushes)
    {
        errorOut = "scene is too complex to render (over " + juce::String (Worldizer::kMaxTraceBrushes)
                 + " surfaces) — simplify the geometry and try again";
        return false;
    }

    // Synchronous full-quality render (~0.2 s typical) on the message thread.
    // Same seed as RenderThread so the saved rendered.wav is sample-identical to
    // what the user has been auditioning.
    Worldizer::RayTracer tracer;
    Worldizer::RayTracer::Settings rt;
    rt.numRays = 50000; rt.maxBounces = 32; rt.randomSeed = 12345;
    const auto traceResult = tracer.trace (snapshot, rt, 48000);

    Worldizer::IRBuilder builder;
    Worldizer::IRBuilder::Settings irs; irs.sampleRate = 48000;
    const auto ir = builder.build (traceResult, irs);

    // Metadata for the bundle.
    Worldizer::WzPresetIO::Loaded meta;
    meta.presetId    = id;
    meta.name        = presetName;
    meta.category    = category.isNotEmpty() ? category : juce::String ("Indoor");
    meta.description = description;
    meta.author      = "User";
    meta.tags        = tags;
    // Inherit the source preset's bed/character assignments — without this, the
    // room tone the user was hearing goes silent the moment they hit Save (the
    // new preset would load with no bed).
    meta.ambientBed             = inheritedBed;
    meta.ambientLevelDb         = inheritedBedLevel;
    meta.defaultSourceCharacter = inheritedSrcChar;
    meta.defaultMicCharacter    = inheritedMicChar;
    meta.renderNumRays    = rt.numRays;
    meta.renderMaxBounces = rt.maxBounces;
    meta.renderSampleRate = 48000;
    meta.renderSeed       = rt.randomSeed;
    meta.renderedAt       = juce::Time::getCurrentTime().toISO8601 (true);
    meta.thumbnail        = Worldizer::renderSceneThumbnail (snapshot, 128, 128);

    juce::String wErr;
    if (! Worldizer::WzPresetIO::writeBundle (bundleDir, snapshot, ir, 48000.0, meta, wErr))
    {
        errorOut = "write failed: " + wErr;
        return false;
    }

    // Rescan + swap to the newly saved preset (loads its baked IR; clears dirty).
    presetManager->rescan();
    setCurrentPresetId (id);
    clearDirtyFlag();
    return true;
}

bool WorldizerAudioProcessor::convertRoomShellForEditing()
{
    bool converted = false;
    {
        const juce::ScopedLock sl (presetLock);
        if (currentPresetMetadata.has_value())
            converted = Worldizer::SectorGeometry::convertRoomShell (currentPresetMetadata->scene);
    }
    if (converted)
    {
        shellConverted.store (true);
        sceneRevision.fetch_add (1);
        // No render request: the compiled sector is acoustically equivalent to the
        // shell it replaced; the baked IR stays valid until the first real edit.
    }
    return converted;
}

// --- Whole-scene edit (RoomView2D spatial drags + sector edits) ---
void WorldizerAudioProcessor::applyEditedScene (const Worldizer::Scene& scene, bool fullQuality)
{
    // Whole-scene assignment (not member-selective): the shell->sector conversion
    // means edited scenes can differ from the live one in their BRUSH list too
    // (shell removed) — copying only source/mics/sectors would leave the live
    // scene with both the shell brushes AND the compiled sector walls.
    mutateSceneAndRender ([&scene] (Worldizer::Scene& s) { s = scene; },
                          fullQuality, fullQuality ? 100.0f : 30.0f);
}

bool WorldizerAudioProcessor::resetPositionsToPresetDefault()
{
    if (presetManager == nullptr)
        return false;

    juce::String id;
    {
        const juce::ScopedLock sl (presetLock);
        if (! currentPresetMetadata.has_value())
            return false;
        id = currentPresetId;
    }

    juce::String err;
    auto def = presetManager->loadPreset (id, err);
    if (! def.has_value())
        return false;

    // Restore the preset's designed source + mic array; keep the live scene's
    // sector geometry (a drawn/edited room stays), and the characters/bed params
    // are untouched (they're APVTS state, not scene state).
    Worldizer::Scene edited = getCurrentScene();
    edited.getSource()   = def->scene.getSource();
    edited.getMicArray() = def->scene.getMicArray();
    applyEditedScene (edited, true);
    return true;
}

void WorldizerAudioProcessor::applyEditedSceneNoRender (const Worldizer::Scene& scene)
{
    Worldizer::Vec3 src, center;
    {
        const juce::ScopedLock sl (presetLock);
        if (! currentPresetMetadata.has_value())
            return;
        currentPresetMetadata->scene = scene; // whole-scene: see applyEditedScene
        src    = scene.getSource().getPosition();
        center = scene.getMicArray().getCenterPosition();
    }
    sceneRevision.fetch_add (1);
    updateDistanceFromSpacing ((center - src).length());
}

// --- Source ---
void WorldizerAudioProcessor::setSourcePosition (Worldizer::Vec3 pos, bool fullQuality)
{
    mutateSceneAndRender ([pos] (Worldizer::Scene& s) { s.getSource().setPosition (pos); },
                          fullQuality, fullQuality ? 100.0f : 30.0f);
}

void WorldizerAudioProcessor::setSourcePattern (Worldizer::SourcePattern pattern)
{
    mutateSceneAndRender ([pattern] (Worldizer::Scene& s) { s.getSource().setPattern (pattern); }, true, 100.0f);
}

Worldizer::SourcePattern WorldizerAudioProcessor::getSourcePattern() const
{
    const juce::ScopedLock sl (presetLock);
    return currentPresetMetadata.has_value() ? currentPresetMetadata->scene.getSource().getPattern()
                                             : Worldizer::SourcePattern::Omnidirectional;
}

void WorldizerAudioProcessor::setSourceOrientation (Worldizer::Vec3 dir)
{
    mutateSceneAndRender ([dir] (Worldizer::Scene& s) { s.getSource().setOrientation (dir); }, true, 100.0f);
}

Worldizer::Vec3 WorldizerAudioProcessor::getSourceOrientation() const
{
    const juce::ScopedLock sl (presetLock);
    return currentPresetMetadata.has_value() ? currentPresetMetadata->scene.getSource().getOrientation()
                                             : Worldizer::Vec3 { 1.0f, 0.0f, 0.0f };
}

// --- Mic array configuration / pattern (discrete => full render) ---
void WorldizerAudioProcessor::setMicConfiguration (Worldizer::MicArray::Configuration config)
{
    mutateSceneAndRender ([config] (Worldizer::Scene& s) { s.getMicArray().setConfiguration (config); }, true, 100.0f);
}

Worldizer::MicArray::Configuration WorldizerAudioProcessor::getMicConfiguration() const
{
    const juce::ScopedLock sl (presetLock);
    return currentPresetMetadata.has_value() ? currentPresetMetadata->scene.getMicArray().getConfiguration()
                                             : Worldizer::MicArray::Configuration::Single;
}

void WorldizerAudioProcessor::setMicPattern (Worldizer::MicPattern pattern)
{
    mutateSceneAndRender ([pattern] (Worldizer::Scene& s) { s.getMicArray().setAllPatterns (pattern); }, true, 100.0f);
}

Worldizer::MicPattern WorldizerAudioProcessor::getMicPattern() const
{
    const juce::ScopedLock sl (presetLock);
    return currentPresetMetadata.has_value() ? currentPresetMetadata->scene.getMicArray().getPattern()
                                             : Worldizer::MicPattern::Omnidirectional;
}

void WorldizerAudioProcessor::setXYAngleDegrees (float deg, bool fullQuality)
{
    mutateSceneAndRender ([deg] (Worldizer::Scene& s) { s.getMicArray().setXYAngleDegrees (deg); },
                          fullQuality, fullQuality ? 100.0f : 30.0f);
}

float WorldizerAudioProcessor::getXYAngleDegrees() const
{
    const juce::ScopedLock sl (presetLock);
    return currentPresetMetadata.has_value() ? currentPresetMetadata->scene.getMicArray().getXYAngleDegrees() : 90.0f;
}

// --- Per-mic position / orientation ---
void WorldizerAudioProcessor::setMicPositionImmediate (int micIndex, Worldizer::Vec3 pos, bool fullQuality)
{
    mutateSceneAndRender ([micIndex, pos] (Worldizer::Scene& s)
    {
        auto& arr = s.getMicArray();
        // XY capsules are coincident by definition — reject individual moves (§15).
        if (arr.getConfiguration() == Worldizer::MicArray::Configuration::StereoXY)
            return;
        if (micIndex >= 0 && micIndex < arr.getNumMics())
            arr.getMic (micIndex).setPosition (pos);
    }, fullQuality, fullQuality ? 100.0f : 30.0f);
}

Worldizer::Vec3 WorldizerAudioProcessor::getMicPosition (int micIndex) const
{
    const juce::ScopedLock sl (presetLock);
    if (currentPresetMetadata.has_value())
    {
        const auto& arr = currentPresetMetadata->scene.getMicArray();
        if (micIndex >= 0 && micIndex < arr.getNumMics())
            return arr.getMic (micIndex).getPosition();
    }
    return {};
}

void WorldizerAudioProcessor::setXYArrayPosition (Worldizer::Vec3 pos, bool fullQuality)
{
    mutateSceneAndRender ([pos] (Worldizer::Scene& s) { s.getMicArray().setXYPosition (pos); },
                          fullQuality, fullQuality ? 100.0f : 30.0f);
}

void WorldizerAudioProcessor::setMicOrientation (int micIndex, Worldizer::Vec3 dir, bool fullQuality)
{
    mutateSceneAndRender ([micIndex, dir] (Worldizer::Scene& s)
    {
        auto& arr = s.getMicArray();
        if (micIndex >= 0 && micIndex < arr.getNumMics())
            arr.getMic (micIndex).setOrientation (dir);
    }, fullQuality, fullQuality ? 100.0f : 30.0f);
}

Worldizer::Vec3 WorldizerAudioProcessor::getMicOrientation (int micIndex) const
{
    const juce::ScopedLock sl (presetLock);
    if (currentPresetMetadata.has_value())
    {
        const auto& arr = currentPresetMetadata->scene.getMicArray();
        if (micIndex >= 0 && micIndex < arr.getNumMics())
            return arr.getMic (micIndex).getOrientation();
    }
    return { -1.0f, 0.0f, 0.0f };
}

void WorldizerAudioProcessor::setXYOrientation (Worldizer::Vec3 dir, bool fullQuality)
{
    mutateSceneAndRender ([dir] (Worldizer::Scene& s) { s.getMicArray().setXYOrientation (dir); },
                          fullQuality, fullQuality ? 100.0f : 30.0f);
}

// Deprecated Slice 4 single-mic helper.
JUCE_BEGIN_IGNORE_DEPRECATION_WARNINGS
void WorldizerAudioProcessor::setSourceAndMicPositions (Worldizer::Vec3 sourcePos,
                                                        Worldizer::Vec3 micPos, bool fullQuality)
{
    mutateSceneAndRender ([sourcePos, micPos] (Worldizer::Scene& s)
    {
        s.getSource().setPosition (sourcePos);
        s.getMic().setPosition (micPos); // primary mic
    }, fullQuality, fullQuality ? 100.0f : 30.0f);
}
JUCE_END_IGNORE_DEPRECATION_WARNINGS

//==============================================================================
juce::StringArray WorldizerAudioProcessor::getTestSignalNames()
{
    // 0 = single Click (best for hearing one clean tail decay), 1 = Clicks (4 transients,
    // reveals reflection density / flutter), 2 = Sweep, 3 = Noise.
    return { "Click", "Clicks", "Sweep", "Noise" };
}

void WorldizerAudioProcessor::triggerTestSignal (int index)
{
    if (index >= 0 && index < kNumTestSignals)
        testSignalRequested.store (index);
}

void WorldizerAudioProcessor::generateTestSignals (double sampleRate)
{
    const int sr = (int) sampleRate;
    juce::Random rng (1234);

    // A single ~9 ms decaying-noise transient. Broadband and percussive — the best
    // probe for hearing ONE clean tail decay (no overlapping tails to muddy it).
    auto makeClick = [&] (juce::AudioBuffer<float>& b, double at)
    {
        const int start = (int) (at * sr);
        const int n     = (int) (0.009 * sr);
        auto* d = b.getWritePointer (0);
        for (int i = 0; i < n && start + i < b.getNumSamples(); ++i)
            d[start + i] += std::exp (-(float) i / (0.0025f * (float) sr)) * (rng.nextFloat() * 2.0f - 1.0f);
    };

    // 0: Click — a SINGLE transient. Short buffer; the convolver rings the tail out
    //    afterwards on its own, so this isolates one decay.
    {
        const int len = juce::jmax (1, (int) (0.3 * sr));
        auto& b = testSignals[0]; b.setSize (1, len); b.clear();
        makeClick (b, 0.02);
    }

    // 1: Clicks — four transients ~0.28 s apart (reveals reflection density / flutter;
    //    in long reverbs the tails overlap, so use single Click to judge a tail).
    {
        const int len = juce::jmax (1, (int) (1.1 * sr));
        auto& b = testSignals[1]; b.setSize (1, len); b.clear();
        makeClick (b, 0.05); makeClick (b, 0.33); makeClick (b, 0.61); makeClick (b, 0.89);
    }

    // 2: Sweep — 3 s exponential 20 Hz -> 20 kHz (reveals frequency response).
    {
        const double T = 3.0, f1 = 20.0, f2 = 20000.0;
        const int len = juce::jmax (1, (int) (T * sr));
        auto& b = testSignals[2]; b.setSize (1, len); b.clear();
        auto* d = b.getWritePointer (0);
        const double w1 = 2.0 * juce::MathConstants<double>::pi * f1;
        const double L  = std::log (f2 / f1);
        const double K  = w1 * T / L;
        for (int i = 0; i < len; ++i)
        {
            const double t = (double) i / sr;
            d[i] = 0.5f * (float) std::sin (K * (std::exp (t / T * L) - 1.0));
        }
        const int fi = (int) (0.02 * sr), fo = (int) (0.03 * sr);
        for (int i = 0; i < fi && i < len; ++i) d[i]           *= (float) i / (float) fi;
        for (int i = 0; i < fo && i < len; ++i) d[len - 1 - i] *= (float) i / (float) fo;
    }

    // 3: Noise — 1.0 s broadband burst, smooth in, hard stop (exposes the tail;
    //    longer than before for more perceived presence vs the sustained sweep).
    {
        const int len = juce::jmax (1, (int) (1.0 * sr));
        auto& b = testSignals[3]; b.setSize (1, len); b.clear();
        auto* d = b.getWritePointer (0);
        for (int i = 0; i < len; ++i) d[i] = 0.4f * (rng.nextFloat() * 2.0f - 1.0f);
        const int fi = (int) (0.02 * sr);
        for (int i = 0; i < fi && i < len; ++i) d[i] *= (float) i / (float) fi;
        const int fo = juce::jmax (1, (int) (0.001 * sr));
        for (int i = 0; i < fo && i < len; ++i) d[len - 1 - i] *= (float) i / (float) fo;
    }

    // Loudness balance (by ear, since RMS != perceived loudness here): the sustained
    // sweep reads much louder per unit energy than transient clicks / broadband noise,
    // so the sweep comes down a touch and the others go up. The click is transient-
    // limited — maxed to the peak ceiling, which is as loud as a short click can get.
    normalizeActiveRms (testSignals[0], 0.60f, 0.95f); // Click  (single) -> peak-ceiling limited
    normalizeActiveRms (testSignals[1], 0.60f, 0.95f); // Clicks (4)      -> peak-ceiling limited
    normalizeActiveRms (testSignals[2], 0.20f, 0.95f); // Sweep           -> up ~+2.5 dB
    normalizeActiveRms (testSignals[3], 0.29f, 0.95f); // Noise           -> up ~+2.4 dB
}

//==============================================================================
void WorldizerAudioProcessor::loadEmbeddedDefaultIR()
{
    juce::WavAudioFormat wav;
    auto* rawStream = new juce::MemoryInputStream (WorldizerBinaryData::default_ir_wav,
                                                   (size_t) WorldizerBinaryData::default_ir_wavSize,
                                                   false);
    std::unique_ptr<juce::AudioFormatReader> reader (wav.createReaderFor (rawStream, true));
    if (reader != nullptr && reader->lengthInSamples > 0)
    {
        juce::AudioBuffer<float> ir ((int) reader->numChannels, (int) reader->lengthInSamples);
        reader->read (&ir, 0, (int) reader->lengthInSamples, 0, true, true);

        // Queued (not synchronous): the render thread is the engine's ONE
        // sanctioned loading thread — a direct load here would race it (a
        // check-then-load was TOCTOU). On a true cold start the thread is idle
        // and the load lands within milliseconds; whenever a preset/scene IR is
        // also queued, the one-deep queue correctly lets it win.
        if (renderThread != nullptr)
            renderThread->requestIRLoad (ir, reader->sampleRate, 5.0f);
    }
}

//==============================================================================
void WorldizerAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const int numCh = getTotalNumOutputChannels();
    const juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) samplesPerBlock, (juce::uint32) numCh };

    // Quiesce the render thread FIRST: it is the room engine's loading thread
    // and must not be mid-load while the convolvers re-prepare underneath it.
    if (renderThread != nullptr)
        renderThread->drain();

    convolution.prepare (sampleRate, samplesPerBlock, numCh);

    // Character chain + ambient bed (wet path; see processBlock chain order).
    sourceCharacter.prepare (spec);
    micCharacter.prepare (spec);
    ambientBed.prepare (spec);
    loadCharactersFromParams(); // convolvers were just reset to identity

    inputGain.prepare (spec);   inputGain.setRampDurationSeconds (0.02);
    outputGain.prepare (spec);  outputGain.setRampDurationSeconds (0.02);
    mixSmoothed.reset (sampleRate, 0.02);
    mixSmoothed.setCurrentAndTargetValue (apvts.getRawParameterValue ("mix")->load() * 0.01f);

    dryDelay.prepare (spec);
    dryDelay.setMaximumDelayInSamples (8192);
    dryDelay.reset();

    // Wet-path pre-delay: enough for 1 s of time-of-flight at any sample rate.
    const int maxPreDelaySamples = juce::jmax (1, (int) (sampleRate * distanceModel.getSettings().maxDelaySeconds) + 4);
    wetPreDelay.setMaximumDelayInSamples (maxPreDelaySamples);
    wetPreDelay.prepare (spec);
    wetPreDelay.reset();

    preDelaySmoothed.reset (sampleRate, 0.05);    // 50 ms ramp (avoids zipper on drag)
    attenuationSmoothed.reset (sampleRate, 0.05);
    const float dist0 = currentDistanceMeters.load();
    const float preDelay0 = distanceModel.computePreDelaySamples (dist0, sampleRate);
    const float atten0    = juce::jmax (1.0e-4f, distanceModel.computeAttenuationGain (dist0));
    preDelaySamplesTarget.store (preDelay0);   // re-seed: pre-delay is in SAMPLES, so it is rate-dependent
    attenuationTarget.store (atten0);
    preDelaySmoothed.setCurrentAndTargetValue (preDelay0);
    attenuationSmoothed.setCurrentAndTargetValue (atten0);

    const int scratchLen = juce::jmax (samplesPerBlock, 8192);
    preparedCapacity = scratchLen;
    dryScratch.setSize (numCh, scratchLen, false, false, true);
    bypassScratch.setSize (numCh, scratchLen, false, false, true);
    mixRamp.assign ((size_t) scratchLen, 0.0f);
    attenRamp.assign ((size_t) scratchLen, 1.0f);
    bypassRamp.assign ((size_t) scratchLen, 0.0f);

    bypassSmoothed.reset (sampleRate, 0.008); // ~8 ms click-free bypass crossfade
    bypassSmoothed.setCurrentAndTargetValue (
        apvts.getRawParameterValue ("bypass")->load() >= 0.5f ? 1.0f : 0.0f);

    generateTestSignals (sampleRate);
    activeTestSignal = -1;
    testSignalRequested.store (-1);

    bypassValue      = apvts.getRawParameterValue ("bypass");
    inputGainValue   = apvts.getRawParameterValue ("inputGain");
    outputGainValue  = apvts.getRawParameterValue ("outputGain");
    mixValue         = apvts.getRawParameterValue ("mix");
    sourceDriveValue = apvts.getRawParameterValue ("sourceDrive");
    micNoiseValue    = apvts.getRawParameterValue ("micNoise");
    ambientLevelValue = apvts.getRawParameterValue ("ambientLevel");

    // Instant first audio: load the embedded default IR synchronously.
    loadEmbeddedDefaultIR();
    updateDryDelayToMatchConvolutionLatency();

    prepared.store (true);

    // First prepare vs re-prepare — the distinction matters:
    //  - No live scene yet (cold start, or prepare-before-setState hosts): load
    //    the current preset fresh.
    //  - A live scene EXISTS (sample-rate change, re-activation, or a state
    //    restore that already ran): do NOT reload the preset — setCurrentPresetId
    //    replaces the live scene with on-disk defaults, clears the dirty flag,
    //    and reseeds the distance model, silently wiping restored session edits
    //    and live drags (and making an offline bounce differ from what the user
    //    auditioned). Instead, re-arm the engine IR (loads attempted before
    //    prepare were dropped by the engine's prepared gate) and reload the bed
    //    (its pool and resample rate were just reset).
    bool haveScene = false;
    juce::String desired, bedId;
    {
        const juce::ScopedLock sl (presetLock);
        haveScene = currentPresetMetadata.has_value();
        desired   = currentPresetId;
        bedId     = currentAmbientBedId;
    }

    const bool offline = isNonRealtime();
    if (! haveScene)
    {
        setCurrentPresetId (desired, false); // keep the session's character/level choices
                                             // (loads the room IR directly when offline)
    }
    else
    {
        rearmEngineForCurrentScene (offline);
        loadAmbientBedById (bedId);
    }

    // Offline determinism: install every queued IR before the first real block so a
    // faster-than-realtime bounce doesn't render its head dry / through the wrong IR.
    if (offline)
        warmUpChainOffline();
}

void WorldizerAudioProcessor::rearmEngineForCurrentScene (bool synchronous)
{
    if (renderThread == nullptr || presetManager == nullptr)
        return;

    // Offline: load the room IR straight into the convolver (the render thread is
    // drained and no audio runs during prepareToPlay), instead of the async
    // RenderThread hop that would let a bounce head render dry.
    auto loadRoom = [this, synchronous] (const juce::AudioBuffer<float>& ir, double sr, float xf)
    {
        if (synchronous) convolution.loadIR (ir, sr, xf);
        else             renderThread->requestIRLoad (ir, sr, xf);
    };

    Worldizer::Scene liveScene;
    juce::String presetId;
    Worldizer::RenderThread::RenderedIR cached;
    {
        const juce::ScopedLock sl (presetLock);
        if (! currentPresetMetadata.has_value())
            return;
        liveScene = currentPresetMetadata->scene;
        presetId  = currentPresetId;
        cached.ir.makeCopyOf (cachedEditedIR.ir);
        cached.sampleRate     = cachedEditedIR.sampleRate;
        cached.sceneSignature = cachedEditedIR.sceneSignature;
    }

    const auto liveSig = Worldizer::RenderThread::sceneSignature (liveScene);

    // Exact match: the cached full-quality IR of this very scene (R8).
    if (cached.ir.getNumSamples() > 0 && cached.sceneSignature == liveSig)
    {
        loadRoom (cached.ir, cached.sampleRate, 30.0f);
        return;
    }

    juce::String err;
    auto loaded = presetManager->loadPreset (presetId, err);
    const bool sceneIsPresetDefault =
        loaded.has_value() && Worldizer::RenderThread::sceneSignature (loaded->scene) == liveSig;

    if (sceneIsPresetDefault && loaded->ir.getNumSamples() > 0)
    {
        loadRoom (loaded->ir, loaded->irSampleRate, 30.0f);
    }
    else if (synchronous)
    {
        // Offline edited scene with no cache: trace inline (QA-9) so the bounce
        // uses the right acoustics instead of a stale/starved background render.
        const auto ir = renderLiveSceneIR();
        if (ir.getNumSamples() > 0)
            convolution.loadIR (ir, 48000.0, 5.0f);
    }
    else
    {
        renderCurrentScene (true, 80.0f); // edited scene with no cache (or unreadable
                                          // preset): background re-render from the
                                          // live geometry — the documented fallback
    }
}

juce::AudioBuffer<float> WorldizerAudioProcessor::renderLiveSceneIR() const
{
    Worldizer::Scene snapshot;
    {
        const juce::ScopedLock sl (presetLock);
        if (! currentPresetMetadata.has_value())
            return {};
        snapshot = currentPresetMetadata->scene;
    }
    if ((int) snapshot.getAllBrushesForTracing().size() > Worldizer::kMaxTraceBrushes)
        return {}; // too complex for a synchronous render; caller keeps prior IR
    Worldizer::RayTracer tracer;
    Worldizer::RayTracer::Settings rt;
    rt.numRays = 50000; rt.maxBounces = 32; rt.randomSeed = 12345; // match RenderThread
    const auto traceResult = tracer.trace (snapshot, rt, 48000);

    Worldizer::IRBuilder builder;
    Worldizer::IRBuilder::Settings irs; irs.sampleRate = 48000;
    return builder.build (traceResult, irs);
}

void WorldizerAudioProcessor::warmUpChainOffline()
{
    // All IRs have been loaded DIRECTLY into their engines. juce::dsp::Convolution
    // installs a queued engine only when process() runs, and our own crossfades
    // advance per block — so pump the whole chain on silence until everything is
    // live. ~48 short blocks with brief sleeps is far more than juce's loader
    // needs; negligible wall-clock, and it only happens on an offline render.
    const int numCh = juce::jmax (1, getTotalNumOutputChannels());
    juce::AudioBuffer<float> silence (numCh, preparedCapacity);
    for (int i = 0; i < 48; ++i)
    {
        silence.clear();
        processChunk (silence);
        juce::Thread::sleep (2);
    }
}

void WorldizerAudioProcessor::releaseResources()
{
    prepared.store (false);
    convolution.reset();
    sourceCharacter.reset();
    micCharacter.reset();
    ambientBed.reset();
}

bool WorldizerAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;

    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void WorldizerAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    if (! prepared.load())
        return; // not yet prepared — pass audio through untouched

    const int numSamples = buffer.getNumSamples();
    const int numCh      = buffer.getNumChannels();

    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, numSamples);

    // Built-in audition: inject the generated test signal (replacing the input)
    // over the whole buffer. It plays THROUGH the chain (and, when bypassed, dry),
    // so bypass A/Bs the effect rather than muting the source.
    const int req = testSignalRequested.exchange (-1);
    if (req >= 0)
    {
        activeTestSignal = req;
        testSignalPos    = 0;
    }
    if (activeTestSignal >= 0)
    {
        const auto& sig  = testSignals[(size_t) activeTestSignal];
        const int   sigLen = sig.getNumSamples();
        const auto* src  = sig.getReadPointer (0);
        for (int i = 0; i < numSamples; ++i)
        {
            const float s = (testSignalPos < sigLen) ? src[(size_t) testSignalPos] : 0.0f;
            for (int ch = 0; ch < numCh; ++ch)
                buffer.getWritePointer (ch)[i] = s;
            ++testSignalPos;
        }
        if (testSignalPos >= sigLen)
            activeTestSignal = -1;
    }

    // Slice the host buffer into chunks no larger than the prepared scratch
    // capacity, so no inner buffer ever has to grow on the audio thread even if a
    // host violates its own maximumBlockSize contract. Chunks are non-owning
    // views (no allocation).
    for (int offset = 0; offset < numSamples; offset += preparedCapacity)
    {
        const int n = juce::jmin (preparedCapacity, numSamples - offset);
        juce::AudioBuffer<float> chunk (buffer.getArrayOfWritePointers(), numCh, offset, n);
        processChunk (chunk);
    }
}

void WorldizerAudioProcessor::processChunk (juce::AudioBuffer<float>& buffer)
{
    const int numSamples = buffer.getNumSamples();
    const int numCh      = buffer.getNumChannels();

    // Bypass reference = the chunk AS RECEIVED (post test-signal, pre-worldizing).
    // Kept for a click-free crossfade at the end; the wet chain still runs while
    // bypassed so the convolvers keep advancing (no stale tail on un-bypass) and
    // queued IR swaps are still consumed.
    for (int ch = 0; ch < numCh; ++ch)
        bypassScratch.copyFrom (ch, 0, buffer, ch, 0, numSamples);
    bypassSmoothed.setTargetValue (bypassValue->load() >= 0.5f ? 1.0f : 0.0f);

    inputGain.setGainDecibels (inputGainValue->load());
    outputGain.setGainDecibels (outputGainValue->load());
    mixSmoothed.setTargetValue (mixValue->load() * 0.01f);
    // Distance targets are written atomically by the message thread (drags /
    // preset loads); the smoothers themselves are audio-thread-only.
    preDelaySmoothed.setTargetValue (preDelaySamplesTarget.load());
    attenuationSmoothed.setTargetValue (attenuationTarget.load());

    juce::dsp::AudioBlock<float> block (buffer);

    // 1. Input gain (smoothed, all channels).
    inputGain.process (juce::dsp::ProcessContextReplacing<float> (block));

    // 2. Capture the dry signal (post input-gain, pre-convolution).
    for (int ch = 0; ch < numCh; ++ch)
        dryScratch.copyFrom (ch, 0, buffer, ch, 0, numSamples);

    // 2b. Source (speaker) character: light drive + speaker IR. The reproducer
    //     plays INTO the room, so this precedes the room convolution — order
    //     matters because the drive stage is nonlinear.
    sourceCharacter.setDrive (sourceDriveValue->load() * 0.01f);
    sourceCharacter.process (block);

    // 3. Wet path: convolution in place.
    convolution.process (block);

    // 3a. Wet-only time-of-flight pre-delay (room IR carries no propagation delay).
    //     Stepped in small SLICES (not once per block) so the 50 ms glide stays
    //     click-free even at large host block sizes / offline bounce; Lagrange
    //     interpolation smooths the sub-sample motion within each slice.
    {
        constexpr int kDelaySlice = 32;
        const float maxDelay = (float) (wetPreDelay.getMaximumDelayInSamples() - 1);
        for (int off = 0; off < numSamples; off += kDelaySlice)
        {
            const int n = juce::jmin (kDelaySlice, numSamples - off);
            wetPreDelay.setDelay (juce::jlimit (0.0f, maxDelay, preDelaySmoothed.getCurrentValue()));
            auto sub = block.getSubBlock ((size_t) off, (size_t) n);
            wetPreDelay.process (juce::dsp::ProcessContextReplacing<float> (sub));
            preDelaySmoothed.skip (n);
        }
    }

    // 3b. Wet-only inverse-distance attenuation (smoothed, same ramp for all channels).
    for (int i = 0; i < numSamples; ++i)
        attenRamp[(size_t) i] = attenuationSmoothed.getNextValue();
    for (int ch = 0; ch < numCh; ++ch)
    {
        auto* w = buffer.getWritePointer (ch);
        for (int i = 0; i < numSamples; ++i)
            w[i] *= attenRamp[(size_t) i];
    }

    // 3c. Mic character: mic IR + optional self-noise floor. The mic hears the
    //     room, so this ends the wet chain. Noise is added post-attenuation
    //     (capsule/electronics noise does not scale with source distance) but IS
    //     inside the wet/dry mix — deliberate: mix = "how much of the worldized
    //     signal, capture chain included"; the bed (the SPACE's own tone) is the
    //     one element that escapes the mix, below.
    micCharacter.setSelfNoise (micNoiseValue->load() * 0.01f);
    micCharacter.process (block);

    // 4. Dry path: latency-match to the convolver (usually zero -> skip).
    if (currentDryDelaySamples > 0)
    {
        auto dryBlock = juce::dsp::AudioBlock<float> (dryScratch)
                            .getSubsetChannelBlock (0, (size_t) numCh)
                            .getSubBlock (0, (size_t) numSamples);
        dryDelay.process (juce::dsp::ProcessContextReplacing<float> (dryBlock));
    }

    // 5. Mix dry + wet with a smoothed blend.
    for (int i = 0; i < numSamples; ++i)
        mixRamp[(size_t) i] = mixSmoothed.getNextValue();

    for (int ch = 0; ch < numCh; ++ch)
    {
        auto* wet = buffer.getWritePointer (ch);
        const auto* dry = dryScratch.getReadPointer (ch);
        for (int i = 0; i < numSamples; ++i)
        {
            const float m = mixRamp[(size_t) i];
            wet[i] = wet[i] * m + dry[i] * (1.0f - m);
        }
    }

    // 5b. Ambient bed: the space's own room tone, layered under the mix. Not
    //     scaled by the mix knob (the bed belongs to the SPACE, not the wet/dry
    //     blend of the source) but rides output gain + the safety ceiling.
    {
        const float bedDb = ambientLevelValue->load();
        ambientBed.setLevel (bedDb <= -59.5f ? 0.0f : juce::Decibels::decibelsToGain (bedDb));
        ambientBed.addToBuffer (buffer);
    }

    // 6. Output gain.
    outputGain.process (juce::dsp::ProcessContextReplacing<float> (block));

    // 7. Transparent safety ceiling + click-free bypass crossfade. The ceiling
    //    stops convolution peaks / dense tails from clipping the output (bit-exact
    //    below ~-1.5 dBFS, soft-saturates to +-1.0 above; stateless => never
    //    pumps). Bypass then crossfades the ceilinged output against the
    //    un-processed reference so engage/release are click-free.
    for (int i = 0; i < numSamples; ++i)
        bypassRamp[(size_t) i] = bypassSmoothed.getNextValue(); // per-sample, shared across channels

    bool caught = false;
    for (int ch = 0; ch < numCh; ++ch)
    {
        auto* w = buffer.getWritePointer (ch);
        const auto* ref = bypassScratch.getReadPointer (ch);
        for (int i = 0; i < numSamples; ++i)
        {
            const float pre  = w[i];
            const float post = softClip (pre);
            const float b = bypassRamp[(size_t) i];
            // The ceiling engaged iff |pre| exceeded the knee; count it only when
            // the ceilinged signal is actually reaching the output (not bypassed).
            if (std::abs (pre) > kSoftClipKnee && b < 0.5f) caught = true;
            w[i] = post * (1.0f - b) + ref[i] * b;
        }
    }
    if (caught)
        ceilingActive.store (true);
}

//==============================================================================
void WorldizerAudioProcessor::updateDryDelayToMatchConvolutionLatency()
{
    const int latency = convolution.getLatencySamples();
    if (latency != currentDryDelaySamples)
    {
        currentDryDelaySamples = latency;
        dryDelay.setDelay (static_cast<float> (latency));
        setLatencySamples (latency);
    }
}

//==============================================================================
juce::AudioProcessorEditor* WorldizerAudioProcessor::createEditor()
{
    return new WorldizerAudioProcessorEditor (*this);
}

//==============================================================================
void WorldizerAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();

    // Stale-property hygiene: replaceState() imported every custom property of a
    // previously RESTORED state into apvts.state, and the writes below are
    // conditional — without clearing first, deleted sector geometry resurrects
    // on the next restore and a dead cachedIRData blob (up to 8 MB) rides along
    // in every subsequent save.
    for (const char* p : { "sectorGeometry", "cachedIRChannels", "cachedIRSamples",
                           "cachedIRSampleRate", "cachedIRSignature", "cachedIRData",
                           "currentScene", "shellConverted" })
        state.removeProperty (p, nullptr);

    // Fetch the render thread's latest full render BEFORE taking presetLock
    // (each has its own lock; keep the ordering trivial).
    const auto lastRender = renderThread != nullptr ? renderThread->getLastFullRender()
                                                    : Worldizer::RenderThread::RenderedIR {};
    {
        const juce::ScopedLock sl (presetLock);
        state.setProperty ("currentPreset", currentPresetId, nullptr);
        if (currentPresetMetadata.has_value())
        {
            const auto& scene = currentPresetMetadata->scene;
            const auto& src   = scene.getSource();
            const auto& arr   = scene.getMicArray();

            auto setVec = [&] (const char* px, const char* py, const char* pz, Worldizer::Vec3 v)
            {
                state.setProperty (px, v.x, nullptr);
                state.setProperty (py, v.y, nullptr);
                state.setProperty (pz, v.z, nullptr);
            };

            // Source.
            setVec ("sourceX", "sourceY", "sourceZ", src.getPosition());
            setVec ("sourceOX", "sourceOY", "sourceOZ", src.getOrientation());
            state.setProperty ("sourcePattern", (int) src.getPattern(), nullptr);

            // Mic array.
            state.setProperty ("micConfig",  (int) arr.getConfiguration(), nullptr);
            state.setProperty ("micPattern", (int) arr.getPattern(), nullptr);
            state.setProperty ("xyAngle",    arr.getXYAngleDegrees(), nullptr);
            setVec ("xyFacingX", "xyFacingY", "xyFacingZ", arr.getXYOrientation());
            setVec ("mic0X", "mic0Y", "mic0Z", arr.getMic (0).getPosition());
            setVec ("mic0OX", "mic0OY", "mic0OZ", arr.getMic (0).getOrientation());
            // mic1 is only meaningful for 2-mic configs, but always store it (the
            // .getMic(1) accessor reads the always-allocated slot) so a round-trip
            // through Single->Spaced->Single is lossless within a session.
            const auto& mic1 = arr.getNumMics() > 1 ? arr.getMic (1) : arr.getPrimary();
            setVec ("mic1X", "mic1Y", "mic1Z", mic1.getPosition());
            setVec ("mic1OX", "mic1OY", "mic1OZ", mic1.getOrientation());

            // Legacy mirror (so a Slice 4 build could still read the primary mic).
            setVec ("micX", "micY", "micZ", arr.getPrimary().getPosition());

            // Sector geometry (Slice 6a): persist the user-edited sectors as JSON so
            // re-opening the session restores them. Empty for legacy / Test presets.
            if (! scene.getSectorGeometry().isEmpty())
                state.setProperty ("sectorGeometry",
                                   juce::JSON::toString (scene.getSectorGeometry().toJson(), true), nullptr);

            // R8: cache the edited scene's full-quality IR in the state blob so a
            // session restore loads it instead of re-rendering. Only written when a
            // render exists whose scene signature matches the CURRENT live scene —
            // a scene at preset defaults never rendered, so nothing is written and
            // restore stays a plain baked-preset load.
            {
                const auto liveSig = Worldizer::RenderThread::sceneSignature (scene);
                const auto* match =
                    (lastRender.ir.getNumSamples() > 0 && lastRender.sceneSignature == liveSig)
                        ? &lastRender
                        : (cachedEditedIR.ir.getNumSamples() > 0 && cachedEditedIR.sceneSignature == liveSig)
                              ? &cachedEditedIR
                              : nullptr;

                constexpr int kMaxCachedBytes = 8 * 1024 * 1024;
                if (match != nullptr)
                {
                    const auto& ir = match->ir;
                    const int bytes = ir.getNumChannels() * ir.getNumSamples() * (int) sizeof (float);
                    if (bytes > 0 && bytes <= kMaxCachedBytes)
                    {
                        juce::MemoryBlock raw ((size_t) bytes);
                        auto* dst = static_cast<float*> (raw.getData());
                        for (int ch = 0; ch < ir.getNumChannels(); ++ch)
                            std::memcpy (dst + (size_t) ch * (size_t) ir.getNumSamples(),
                                         ir.getReadPointer (ch),
                                         (size_t) ir.getNumSamples() * sizeof (float));

                        state.setProperty ("cachedIRChannels",   ir.getNumChannels(), nullptr);
                        state.setProperty ("cachedIRSamples",    ir.getNumSamples(), nullptr);
                        state.setProperty ("cachedIRSampleRate", match->sampleRate, nullptr);
                        state.setProperty ("cachedIRSignature",  match->sceneSignature, nullptr);
                        state.setProperty ("cachedIRData",       raw.toBase64Encoding(), nullptr);
                    }
                }
            }
        }
    }
    state.setProperty ("sidebarCollapsed", sidebarCollapsed.load(), nullptr);
    state.setProperty ("dirty",             dirtyFlag.load(),       nullptr);
    state.setProperty ("shellConverted",    shellConverted.load(),  nullptr);
    // Editor window size (QoL): 0x0 if never resized — the editor treats that
    // as "use the default size". Session state, not preset data.
    state.setProperty ("editor_width",  editorWidth.load(),  nullptr);
    state.setProperty ("editor_height", editorHeight.load(), nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void WorldizerAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        auto state = juce::ValueTree::fromXml (*xml);
        if (state.isValid() && state.hasType (apvts.state.getType()))
        {
            apvts.replaceState (state);

            if (state.hasProperty ("sidebarCollapsed"))
                sidebarCollapsed.store ((bool) state["sidebarCollapsed"]);

            // Editor window size (QoL). Missing in .beep-derived/older sessions,
            // which leaves the default 0x0 ("use editor default"); the editor
            // reads this back in its constructor.
            setEditorSize ((int) state.getProperty ("editor_width", 0),
                           (int) state.getProperty ("editor_height", 0));

            juce::String presetId;
            if (state.hasProperty ("currentPreset"))
                presetId = state["currentPreset"].toString();
            else if (state.hasProperty ("currentScene"))     // migrate Slice 2 sessions
                presetId = sceneNameToPresetId (state["currentScene"].toString());

            // Missing preset (deleted user preset): fall back to the default and
            // SKIP the scene overlay — restoring transforms/sectors onto the wrong
            // room would be a silent franken-state.
            if (presetId.isNotEmpty() && presetManager != nullptr && ! presetManager->hasPreset (presetId))
            {
                juce::Logger::writeToLog ("State references missing preset '" + presetId
                                          + "'; falling back to " + kDefaultPreset);
                setCurrentPresetId (kDefaultPreset, false);
                presetId.clear();
            }

            if (presetId.isNotEmpty())
            {
                // Load the baked IR at default geometry. false: apvts.replaceState
                // above already restored the session's character/ambient choices —
                // the preset's defaults must not clobber them.
                setCurrentPresetId (presetId, false);

                // Reconstruct the saved source + mic array onto the (default) live
                // scene, then only re-render if it differs from the preset default
                // (otherwise the baked IR is correct — stay instant).
                auto getVec = [&] (const char* px, const char* py, const char* pz, Worldizer::Vec3 fallback)
                {
                    return state.hasProperty (px)
                        ? Worldizer::Vec3 { (float) state[px], (float) state[py], (float) state[pz] }
                        : fallback;
                };

                const Worldizer::Scene defaultScene = getCurrentScene();
                Worldizer::Scene edited = defaultScene;

                if (state.hasProperty ("micConfig"))
                {
                    // === Slice 5 format ===
                    auto& src = edited.getSource();
                    src.setPosition    (getVec ("sourceX",  "sourceY",  "sourceZ",  src.getPosition()));
                    src.setOrientation (getVec ("sourceOX", "sourceOY", "sourceOZ", src.getOrientation()));
                    // Clamp enum ints from state to valid ranges (corrupt/hostile
                    // state can hold anything); setConfiguration also self-clamps.
                    if (state.hasProperty ("sourcePattern"))
                        src.setPattern (Worldizer::SourcePattern::Omnidirectional); // only value today
                    auto& arr = edited.getMicArray();
                    arr.setConfiguration ((Worldizer::MicArray::Configuration) (int) state["micConfig"]);
                    if (state.hasProperty ("micPattern"))
                        arr.setAllPatterns ((int) state["micPattern"] == (int) Worldizer::MicPattern::Shotgun
                                                ? Worldizer::MicPattern::Shotgun
                                                : Worldizer::MicPattern::Omnidirectional);

                    const auto mic0Pos = getVec ("mic0X", "mic0Y", "mic0Z", arr.getMic (0).getPosition());
                    const auto mic0Dir = getVec ("mic0OX", "mic0OY", "mic0OZ", arr.getMic (0).getOrientation());

                    switch (arr.getConfiguration())
                    {
                        case Worldizer::MicArray::Configuration::Single:
                            arr.getMic (0).setPosition (mic0Pos);
                            arr.getMic (0).setOrientation (mic0Dir);
                            break;

                        case Worldizer::MicArray::Configuration::StereoXY:
                            if (state.hasProperty ("xyAngle"))   arr.setXYAngleDegrees ((float) state["xyAngle"]);
                            arr.setXYOrientation (getVec ("xyFacingX", "xyFacingY", "xyFacingZ", arr.getXYOrientation()));
                            arr.setXYPosition (mic0Pos); // coincident; orientations derived from facing+angle
                            break;

                        case Worldizer::MicArray::Configuration::SpacedPair:
                            arr.getMic (0).setPosition (mic0Pos);
                            arr.getMic (0).setOrientation (mic0Dir);
                            arr.getMic (1).setPosition    (getVec ("mic1X", "mic1Y", "mic1Z", arr.getMic (1).getPosition()));
                            arr.getMic (1).setOrientation (getVec ("mic1OX", "mic1OY", "mic1OZ", arr.getMic (1).getOrientation()));
                            break;
                    }
                }
                else if (state.hasProperty ("sourceX"))
                {
                    // === Legacy Slice 4 format: single omni mic ===
                    edited.getSource().setPosition (getVec ("sourceX", "sourceY", "sourceZ", edited.getSource().getPosition()));
                    edited.getMicArray().setConfiguration (Worldizer::MicArray::Configuration::Single);
                    edited.getMicArray().getMic (0).setPosition (getVec ("micX", "micY", "micZ", edited.getMic().getPosition()));
                }

                // Sector geometry (Slice 6a). If present in state, parse and overlay.
                bool sectorsRestored = false;
                if (state.hasProperty ("sectorGeometry"))
                {
                    const auto sg = juce::JSON::parse (state["sectorGeometry"].toString());
                    juce::String sErr;
                    sectorsRestored = edited.getSectorGeometry().fromJson (sg, sErr) && ! edited.getSectorGeometry().isEmpty();
                }

                // Shell conversion: the sector superseded the preset's shell
                // brushes — strip them from the freshly-loaded default scene or
                // the walls double up. (Pre-conversion sessions that drew a
                // sector INSIDE a brush room keep both, as they always did.)
                const bool wasShellConverted = sectorsRestored && (bool) state["shellConverted"];
                if (wasShellConverted)
                    Worldizer::SectorGeometry::removeRoomShell (edited);

                if (sectorsRestored || interactiveStateDiffers (edited, defaultScene))
                {
                    // R8: prefer the cached IR from the state blob — restoring an
                    // edited session must not re-render (Soundminer requirement).
                    // The signature check guarantees the cache matches THIS scene.
                    Worldizer::RenderThread::RenderedIR cached;
                    if (state.hasProperty ("cachedIRData"))
                    {
                        // Bounds-check with 64-bit math BEFORE decoding: JUCE's
                        // fromBase64Encoding allocates by the embedded size prefix,
                        // so a corrupted/hostile property could otherwise trigger a
                        // huge allocation (or int overflow) during project load.
                        const int channels = (int) state["cachedIRChannels"];
                        const int samples  = (int) state["cachedIRSamples"];
                        const juce::int64 expectedBytes =
                            (juce::int64) channels * (juce::int64) samples * (juce::int64) sizeof (float);
                        const auto b64 = state["cachedIRData"].toString();
                        const juce::int64 declaredBytes =
                            b64.upToFirstOccurrenceOf (".", false, false).getLargeIntValue();

                        constexpr juce::int64 kMaxCachedBytes = 8 * 1024 * 1024;
                        juce::MemoryBlock raw;
                        if (channels > 0 && channels <= 2 && samples > 0
                            && expectedBytes <= kMaxCachedBytes
                            && declaredBytes == expectedBytes
                            && raw.fromBase64Encoding (b64)
                            && (juce::int64) raw.getSize() == expectedBytes)
                        {
                            cached.ir.setSize (channels, samples);
                            const auto* srcData = static_cast<const float*> (raw.getData());
                            for (int ch = 0; ch < channels; ++ch)
                                cached.ir.copyFrom (ch, 0, srcData + (size_t) ch * (size_t) samples, samples);
                            cached.sampleRate     = (double) state["cachedIRSampleRate"];
                            cached.sceneSignature = state["cachedIRSignature"].toString();
                        }
                    }

                    if (cached.ir.getNumSamples() > 0
                        && cached.sceneSignature == Worldizer::RenderThread::sceneSignature (edited))
                    {
                        applyEditedSceneNoRender (edited);
                        renderThread->requestIRLoad (cached.ir, cached.sampleRate, 80.0f);
                        const juce::ScopedLock sl (presetLock);
                        cachedEditedIR = std::move (cached);
                    }
                    else
                    {
                        applyEditedScene (edited, true);
                    }
                }

                // The edit-dirty and shell-converted flags are session state too,
                // restored last so the apply above doesn't accidentally clear them
                // via setCurrentPresetId.
                if (state.hasProperty ("dirty"))
                    dirtyFlag.store ((bool) state["dirty"]);
                shellConverted.store (wasShellConverted);
            }
        }
    }
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new WorldizerAudioProcessor();
}
