#pragma once

#include <JuceHeader.h>
#include <array>
#include <optional>
#include "../DSP/ConvolutionEngine.h"
#include "../DSP/DistanceModel.h"
#include "../DSP/SourceCharacter.h"
#include "../DSP/MicCharacter.h"
#include "../DSP/AmbientBed.h"
#include "RenderThread.h"
#include "PresetManager.h"
#include "../Shared/Constants.h"

/**
    Worldizer — main audio processor.

    Slice 2: a real-time convolution reverb. The input is convolved with the IR of
    the selected test scene (rendered on a background thread by the ray tracer and
    crossfaded in by the ConvolutionEngine), then blended with a latency-matched dry
    path and trimmed by input/output gain. A pre-baked default IR is embedded so the
    first audio is available immediately on cold start.
*/
class WorldizerAudioProcessor : public juce::AudioProcessor,
                                private juce::AudioProcessorValueTreeState::Listener,
                                private juce::Timer
{
public:
    WorldizerAudioProcessor();
    ~WorldizerAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override                        { return true; }

    //==============================================================================
    const juce::String getName() const override            { return JucePlugin_Name; }
    bool acceptsMidi() const override                      { return false; }
    bool producesMidi() const override                     { return false; }
    bool isMidiEffect() const override                     { return false; }
    // IR tail + the wet-path time-of-flight pre-delay (up to maxDelaySeconds),
    // so a tail-trimming host doesn't clip the tail at large source/mic distances.
    double getTailLengthSeconds() const override
    {
        return (double) Worldizer::kMaxIRLengthSeconds + (double) distanceModel.getSettings().maxDelaySeconds;
    }

    //==============================================================================
    int getNumPrograms() override                          { return 1; }
    int getCurrentProgram() override                       { return 0; }
    void setCurrentProgram (int) override                  {}
    const juce::String getProgramName (int) override       { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorParameter* getBypassParameter() const override { return bypassParam; }

    //==============================================================================
    // === Preset selection (presets are .wzpreset bundles loaded by PresetManager) ===
    juce::String getCurrentPresetId() const;
    /** Loads a preset. applyCharacterDefaults=true (a USER selection) also applies
        the preset's default source/mic characters and ambient level; false (state
        restore / prepareToPlay re-apply) leaves those parameters untouched so a
        restored session keeps the user's own choices. */
    void         setCurrentPresetId (const juce::String& presetId, bool applyCharacterDefaults = true);
    juce::StringArray getAvailablePresetIds() const;
    juce::Array<Worldizer::WzPresetIO::Loaded> getAvailablePresetMetadata() const;
    Worldizer::PresetManager& getPresetManager() noexcept { return *presetManager; }
    std::optional<Worldizer::WzPresetIO::Loaded> getCurrentPresetMetadata() const;
    bool isRendering() const noexcept;

    // === Live scene editing (Slice 4 / 5) ===
    // All of these mutate the current (ephemeral) scene and re-render the IR
    // (preview quality during drag, full on release / for discrete changes). Edits
    // are NOT written back to the preset on disk.

    /** Copies an edited scene's source + mic array into the live scene and re-renders.
        Used by RoomView2D for spatial drags (position / rotation). */
    void applyEditedScene (const Worldizer::Scene& scene, bool fullQuality);

    /** Same scene copy + distance-model refresh but WITHOUT a render — used on
        state restore when the edited scene's IR was cached in the state blob
        (Soundminer: re-instantiation must not trigger re-rendering). */
    void applyEditedSceneNoRender (const Worldizer::Scene& scene);

    /** Reverts the source + mic array to the current preset's on-disk defaults
        (keeps the chosen characters / ambient level and any drawn sector
        geometry). Returns false if there's nothing to revert. Message thread. */
    bool resetPositionsToPresetDefault();

    /** Returns a copy of the live scene (source + mic array + geometry). */
    Worldizer::Scene getCurrentScene() const;

    /** Monotonic revision of the live scene — bumps on every scene mutation
        (edits, preset loads, state restore). Lets the editor detect out-of-band
        changes that keep the same preset id (e.g. a state restore with a
        different mic config) without diffing scene contents every tick. */
    int getSceneRevision() const noexcept { return sceneRevision.load(); }

    // --- Source ---
    void setSourcePosition (Worldizer::Vec3 pos, bool fullQuality);
    void setSourcePattern (Worldizer::SourcePattern pattern);
    Worldizer::SourcePattern getSourcePattern() const;
    void setSourceOrientation (Worldizer::Vec3 dir);
    Worldizer::Vec3 getSourceOrientation() const;

    // --- Mic array configuration / pattern ---
    void setMicConfiguration (Worldizer::MicArray::Configuration config);
    Worldizer::MicArray::Configuration getMicConfiguration() const;
    void setMicPattern (Worldizer::MicPattern pattern);
    Worldizer::MicPattern getMicPattern() const;
    void setXYAngleDegrees (float deg, bool fullQuality);
    float getXYAngleDegrees() const;

    // --- Per-mic position / orientation ---
    void setMicPositionImmediate (int micIndex, Worldizer::Vec3 pos, bool fullQuality);
    Worldizer::Vec3 getMicPosition (int micIndex) const;
    void setXYArrayPosition (Worldizer::Vec3 pos, bool fullQuality);
    void setMicOrientation (int micIndex, Worldizer::Vec3 dir, bool fullQuality);
    Worldizer::Vec3 getMicOrientation (int micIndex) const;
    void setXYOrientation (Worldizer::Vec3 dir, bool fullQuality);

    /** Deprecated Slice 4 single-mic helper — routes to source + primary mic. */
    [[deprecated ("Use setSourcePosition / setMicPositionImmediate / applyEditedScene")]]
    void setSourceAndMicPositions (Worldizer::Vec3 sourcePos, Worldizer::Vec3 micPos, bool fullQuality);

    // === Sidebar UI state (persisted in plugin state) ===
    bool getSidebarCollapsed() const noexcept { return sidebarCollapsed.load(); }
    void setSidebarCollapsed (bool shouldBeCollapsed) { sidebarCollapsed.store (shouldBeCollapsed); }

    // === Room-shell conversion (make the preset's own walls editable) ===
    /** Converts the current preset's brush-built room shell into an editable
        sector (SectorGeometry::convertRoomShell). Called on entering edit mode.
        Deliberately does NOT re-render: the acoustics only change on the user's
        first actual edit. Returns false when the scene already has sectors or
        has no closed shell (open-air scenes). Message thread. */
    bool convertRoomShellForEditing();

    // === Edit-mode dirty flag (Slice 6a) ===
    // Set by the editor when the user mutates GEOMETRY (sectors / vertices / linedefs
    // / sector heights / materials). Drives the "uncommitted edits" prompt on preset
    // switch, and is cleared by Save-As. Source / mic drags do NOT touch it — those
    // remain ephemeral session state.
    bool hasUncommittedEdits() const noexcept { return dirtyFlag.load(); }
    void markDirty() noexcept                 { dirtyFlag.store (true); }
    void clearDirtyFlag() noexcept            { dirtyFlag.store (false); }

    // === Save As (Slice 6a) ===
    /** Renders the current live scene to a full-quality IR (synchronous — on the
        message thread; ~0.2 s typical) and writes a `.wzpreset` bundle into the user
        library folder. On success the new preset is rescanned, selected, and the
        dirty flag is cleared. Returns true on success; on failure fills errorOut. */
    bool saveCurrentSceneAsPreset (const juce::String& presetName,
                                   const juce::String& category,
                                   const juce::String& description,
                                   const juce::StringArray& tags,
                                   bool overwriteExisting,
                                   juce::String& errorOut);

    // === Deprecated Slice 2 scene API (mapped onto presets for compatibility) ===
    [[deprecated ("Use getCurrentPresetId")]] juce::String getCurrentSceneName() const;
    [[deprecated ("Use setCurrentPresetId")]] void setCurrentSceneName (const juce::String& sceneName);

    // === Source / mic character library (Slice 5.5) ===
    // Selection is the "sourceCharacter"/"micCharacter" choice parameters (indices
    // into CharacterLibrary::speakers()/mics()). Audition-on-hover loads an IR
    // directly WITHOUT touching the parameter; ending the audition restores the
    // committed selection. All message-thread.
    void auditionSourceCharacter (int index);
    void endSourceCharacterAudition();
    void auditionMicCharacter (int index);
    void endMicCharacterAudition();

    // === Built-in audition test signals ===
    /** Triggers a built-in dry test signal to play once through the worldizing
        chain (so a scene can be auditioned without host content). Index matches
        getTestSignalNames(). RT-safe to call from the message thread. */
    void triggerTestSignal (int index);
    static juce::StringArray getTestSignalNames();

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void renderCurrentScene (bool fullQuality, float crossfadeMs);
    void loadEmbeddedDefaultIR();
    void updateDryDelayToMatchConvolutionLatency();
    static juce::String sceneNameToPresetId (const juce::String& sceneName);

    /** Re-arms the convolver with the correct IR for the CURRENT live scene
        without touching the scene itself: the R8 cached IR when its signature
        matches, else the preset's baked IR (plus a background re-render if the
        scene has real edits). Used on re-prepare, where reloading the preset
        outright would wipe restored/live edits. */
    void rearmEngineForCurrentScene (bool synchronous = false);

    /** Full-quality synchronous trace of the live scene into a room IR (message
        thread). Used by the offline path and Save-As. Empty on failure. */
    juce::AudioBuffer<float> renderLiveSceneIR() const;

    /** Offline determinism (isNonRealtime): after all IRs are loaded DIRECTLY into
        the engines, pump the chain on silence so juce::dsp::Convolution installs
        every pending engine and the crossfades finish BEFORE the first real block —
        otherwise a faster-than-realtime bounce renders its head dry / through the
        wrong IR. Blocking here is safe: no audio thread runs during prepareToPlay. */
    void warmUpChainOffline();

    // Character / ambient-bed loading (message thread). Parameter changes can
    // arrive on ANY thread including the audio thread (host automation), so
    // parameterChanged only sets a lock-free flag; a message-thread timer polls
    // it and does the (allocating) WAV decode. NOT AsyncUpdater — its
    // triggerAsyncUpdate posts to the system message queue, which takes a lock
    // (audio-thread hazard).
    void parameterChanged (const juce::String& parameterID, float newValue) override;
    void timerCallback() override;
    void loadSourceCharacterByIndex (int index);
    void loadMicCharacterByIndex (int index);
    void loadCharactersFromParams();
    void loadAmbientBedById (const juce::String& bedId);

    /** Applies an edit to the live scene under presetLock, refreshes the distance
        model from the mic-array centre, and requests a re-render. */
    void mutateSceneAndRender (const std::function<void (Worldizer::Scene&)>& edit,
                               bool fullQuality, float crossfadeMs);
    /** Recomputes the distance model + smoother targets from a source/centre spacing. */
    void updateDistanceFromSpacing (float distanceMeters);

    /** Processes ONE chunk (already <= the prepared capacity) of the worldizing
        chain, in place, with a click-free bypass crossfade. processBlock slices
        the host buffer into capacity-sized chunks and calls this per slice, so no
        inner scratch buffer ever has to grow on the audio thread. */
    void processChunk (juce::AudioBuffer<float>& buffer);

    static constexpr const char* kDefaultPreset = "small_concrete_room";

    Worldizer::ConvolutionEngine convolution;
    std::unique_ptr<Worldizer::PresetManager> presetManager;
    std::unique_ptr<Worldizer::RenderThread> renderThread;

    // === Character chain + ambient bed (Slices 5.5 / 6) ===
    Worldizer::SourceCharacter sourceCharacter;
    Worldizer::MicCharacter    micCharacter;
    Worldizer::AmbientBed      ambientBed;
    juce::AudioParameterChoice* sourceCharParam = nullptr;
    juce::AudioParameterChoice* micCharParam    = nullptr;
    std::atomic<float>* sourceDriveValue  = nullptr;
    std::atomic<float>* micNoiseValue     = nullptr;
    std::atomic<float>* ambientLevelValue = nullptr;
    std::atomic<bool> characterReloadNeeded { false };
    juce::String currentAmbientBedId; // guarded by presetLock

    // Cached full-quality IR of the current EDITED scene, persisted in plugin
    // state so restoring an edited session loads instead of re-rendering (R8).
    // Guarded by presetLock.
    Worldizer::RenderThread::RenderedIR cachedEditedIR;

    juce::dsp::Gain<float> inputGain, outputGain;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mixSmoothed;

    std::atomic<float>* bypassValue     = nullptr;
    std::atomic<float>* inputGainValue  = nullptr;
    std::atomic<float>* outputGainValue = nullptr;
    std::atomic<float>* mixValue        = nullptr;
    juce::AudioParameterBool* bypassParam = nullptr;

    // Click-free bypass: the wet chain always runs (so the convolvers keep
    // advancing — no stale tail rings out on un-bypass, and IR swaps are still
    // consumed while bypassed); the output crossfades between the processed
    // signal and the un-processed input over ~8 ms. Audio-thread only.
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> bypassSmoothed;
    juce::AudioBuffer<float> bypassScratch; // per-chunk un-processed reference
    int preparedCapacity = 8192;            // scratch length; chunk size cap in processBlock

    // Output clip/ceiling indicator (UX): set true (audio thread) whenever the
    // safety ceiling actually catches a sample; polled + cleared by the editor.
    std::atomic<bool> ceilingActive { false };
public:
    /** True if the output soft-clip ceiling caught audio recently. The editor
        polls this to light a clip indicator, then clears it. */
    bool getAndClearCeilingActive() noexcept { return ceilingActive.exchange (false); }
private:

    mutable juce::CriticalSection presetLock;
    std::atomic<int> sceneRevision { 0 };
    juce::String currentPresetId { kDefaultPreset };
    std::optional<Worldizer::WzPresetIO::Loaded> currentPresetMetadata;
    std::atomic<bool> sidebarCollapsed { false };
    std::atomic<bool> dirtyFlag { false };
    // True once the current preset's brush shell was converted to a sector.
    // Persisted in state: on restore the freshly-loaded preset still HAS its
    // shell brushes, which must be stripped before the restored sector is
    // overlaid (else the walls double up acoustically).
    std::atomic<bool> shellConverted { false };

    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::None> dryDelay { 8192 };
    int currentDryDelaySamples = 0;

    // === Distance model (Slice 4.5) — wet-path only ===
    // The room IR is distance-independent; these add the time-of-flight pre-delay
    // and the inverse-distance level to the WET signal, driven by source/mic spacing.
    Worldizer::DistanceModel distanceModel;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Lagrange3rd> wetPreDelay { 1 << 16 };
    // SmoothedValue is NOT thread-safe, so the message thread only writes the
    // atomic *targets*; the audio thread applies them via setTargetValue at the
    // top of processBlock (same pattern as drive/noise/bed level).
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>         preDelaySmoothed;   // audio thread only
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> attenuationSmoothed; // audio thread only
    std::atomic<float> preDelaySamplesTarget { 0.0f };
    std::atomic<float> attenuationTarget { 1.0f };
    std::atomic<float> currentDistanceMeters { 1.0f };

    juce::AudioBuffer<float> dryScratch;
    std::vector<float>       mixRamp;
    std::vector<float>       attenRamp;
    std::vector<float>       bypassRamp;

    // Built-in audition test signals (generated in prepareToPlay).
    void generateTestSignals (double sampleRate);
    static constexpr int kNumTestSignals = 4;
    std::array<juce::AudioBuffer<float>, (size_t) kNumTestSignals> testSignals;
    std::atomic<int> testSignalRequested { -1 };
    int activeTestSignal = -1;  // audio-thread only
    int testSignalPos    = 0;   // audio-thread only

    std::atomic<bool> prepared { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WorldizerAudioProcessor)
};
