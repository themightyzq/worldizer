# Worldizer Roadmap

This document tracks development slices for Worldizer. Each slice ends with a buildable, testable plugin. Do not start a new slice until the current one's acceptance criteria are met.

## Status Key
- `[ ]` Not started
- `[~]` In progress
- `[x]` Complete
- `[!]` Blocked or needs decision

---

## Slice 0: Foundation (this session)

**Goal:** Buildable scaffold with all foundational documents and source stubs. No DSP, no UI beyond a placeholder.

- [x] Repository structure created per `CLAUDE.md` §6
- [x] `CLAUDE.md`, `README.md`, `TODO.md`, `LICENSE`, `CONTRIBUTING.md` complete
- [x] `Docs/architecture.md` complete
- [x] `CMakeLists.txt` with correct plugin metadata, compile defs, formats
- [x] JUCE 8.x added as git submodule (pinned to 8.0.12)
- [x] All source stubs in place with API outlines (headers) and minimal bodies (`.cpp` where needed)
- [x] `PluginProcessor` passes audio through silently with bypass parameter
- [x] `PluginEditor` shows a placeholder ("Worldizer v0.0.1 — scaffold")
- [x] GitHub Actions workflow exists for macOS Universal Binary builds
- [x] `.gitignore`, issue templates, PR template in place
- [x] `cmake -B build && cmake --build build` succeeds clean
- [x] Plugin passes `pluginval --strictness-level 10`

**Acceptance:** A user can clone the repo, init submodules, run cmake, build, and load the plugin in a DAW. The plugin loads, passes audio, doesn't crash, has no audible processing.

---

## Slice 1: Ray Tracer (headless)

**Goal:** Working ray tracer that produces sensible IRs from a hardcoded scene.

- [x] `RayTracer.h/cpp` — emit N rays from a source, bounce off scene primitives, accumulate energy histogram per frequency band
- [x] `Brush.h/cpp` — axis-aligned box (slab-method ray intersection) with per-face material
- [x] `Material.h/cpp` — frequency-dependent absorption (8 bands) and scattering coefficient
- [x] `Scene.h/cpp` — collection of brushes + source + mic
- [x] `IRBuilder.h/cpp` — convert ray-tracer energy histogram to a **mono** IR (band energies summed, air-weighted, noise-shaped). Stereo deferred (Slice 1 scope is mono)
- [x] `AirAbsorption.h/cpp` — distance-dependent HF rolloff (simplified dB/100m table; full ISO 9613-1 is a v1.0 refinement)
- [x] Headless test program (`Tests/render_test_scene.cpp` → `RenderTestScene`) that builds a hardcoded scene, runs the ray tracer, writes the IR to a WAV file
- [x] Validate IRs are sensible: direct sound at correct delay, reflections at correct times, energy decay matches geometry
- [x] Engine also links into the plugin target (plugin unchanged, still silent pass-through)

**Acceptance:** Running the test program produces a WAV file. Listening to a click convolved with the WAV produces a believable room impulse. Reflection times match hand-calculated values for a simple box. **Met:** direct = sample 280 (exact), floor bounce = sample 504 (hand-calc 505), decay ordering anechoic < forest < small room < hallway < gymnasium, determinism bit-identical, gym 20k-ray trace 0.05s.

### Slice 1 retrospective

**What worked**
- Geometry/timing is exact: direct and first-reflection arrival times match hand calculations to the sample. Decay times scale correctly with room size and absorption.
- Determinism (seeded `juce::Random`) gives bit-identical WAVs for re-runs — useful for regression testing.
- Performance is far inside budget: 20k-ray gym trace ~0.05s, 500k ~1.3s; IR build ~0.02–0.03s (no BVH needed yet).
- The principled reflection normalization (`2/micRadius · √(E/numRays)`) puts reflections on the same physical scale as the `1/d` direct sound.

**IR reconstruction (FINAL — `IRBuilder`)**
The pragmatic broadband-impulse-per-bin shortcut was tried first per the prompt, but it sounded like static/crackle, so it was **replaced with proper band-filtered modulated-noise synthesis** (standard auralization):
- Per octave band (8 bands, 62.5–8000 Hz): air-weighted energy histogram → smoothed into a **time-growing-window** energy envelope (short early for reflection detail, long late so the sparse diffuse tail is continuous) → `sqrt` → amplitude-modulates **independent white noise** band-passed by **two cascaded RBJ biquads (24 dB/oct, Q≈0.9)**, normalised to unit RMS. The 8 bands sum incoherently (flat magnitude, no comb colouration) and decay independently (HF darkens before LF).
- A clean broadband **direct impulse** (`1/distance`, air-weighted) is added on top; 20 ms end fade; whole IR peak-normalised to **−1 dBFS**.
- **IR format for Slice 2:** mono `juce::AudioBuffer<float>`, 48 kHz, peak-normalised to −1 dBFS, length `min(maxLengthSamples=6·48000, numBins)`. The IR **contains the direct sound**, so a fully-wet convolution already includes direct + room. Tool writes 24-bit PCM mono WAV.

**Harder / surprising**
- `juce::Vector3D` lives in `juce_opengl`, which would drag the GUI stack into the headless tool. Used a small `Worldizer::Vec3` (`Source/Shared/Vec3.h`) instead. All engine types unified under the `Worldizer` namespace.
- A sweep ends in HF, which (by design) decays fastest and is unmodelled above ~11 kHz — so sweep *endings* sound dry even though the room isn't. Judge tail length with a broadband transient / gated noise, not a sweep.
- Brick-wall FFT band analysis grossly misreports low-band RT (sinc smearing); always measure octave decay with IIR octave filters.
- The 10 cm mic under-samples large/open scenes (gym ~37 hits at 20k, ~1000 at 500k). Hit count scales linearly with rays; envelope smoothing + dense noise carrier hide it, but renders use 100k–500k rays.

**Deferred / known limitations (Slice 1.5 / 2 / later)**
- `ImageSourceSolver` — discrete, sharp early reflections (orders 1–3); will also fix the **specular early-reflection over-spike** (small-room floor bounce ~12 dB hot) and the current smearing of early reflections into the diffuse envelope.
- Reconstruction tops out at the 8 kHz octave (~11 kHz); content above passes dry — optional 16 kHz band later.
- Statistical late-tail synthesis; stereo / multi-mic IRs; JSON scene loading; larger default mic radius or adaptive ray counts for big/open scenes.
- IR is peak-normalised (not absolute-calibrated) → Slice 2 owns wet/dry/mix gain.

---

## Slice 2: Convolution Runtime

**Goal:** Plug the ray-traced IR into the audio thread.

- [x] `ConvolutionEngine.h/cpp` — wraps `juce::dsp::Convolution`, two-convolver click-free crossfade on background-thread IR loads
- [x] `PluginProcessor` integrates `ConvolutionEngine` + a dedicated `RenderThread`
- [x] Load a baked IR at startup (embedded `default_ir.wav`, loaded synchronously for instant audio)
- [x] Wet/dry mix parameter (+ input gain, output gain, bypass) via APVTS
- [x] Bypass works correctly
- [x] No allocations in `processBlock` (pre-sized scratch; juce::dsp::Convolution load/process are wait-free)
- [x] No clicks on IR swap (linear crossfade between two parallel convolvers)
- [x] Scene selector UI (5 hardcoded scenes) + background re-render + IR crossfade
- [x] State save/restore (APVTS + `currentScene` property)
- [x] pluginval --strictness-level 10 passes; Universal Binary

**Acceptance:** Plugin loaded in a DAW with the hardcoded IR audibly worldizes input audio. Bypass A/Bs cleanly. CPU usage is reasonable. **Verified programmatically:** clean build (zero non-JUCE warnings), pluginval strictness 10, Universal Binary. **Needs user/DAW (interactive):** audible scene distinctness, crossfade-by-ear, multi-instance feel, cold-start feel.

### Slice 2 retrospective

**Crossfade strategy (as implemented):** two `juce::dsp::Convolution` instances. The background `RenderThread` resets the idle convolver and calls its (wait-free) `loadImpulseResponse`, then sets an atomic `swapRequested`. On the next `process()` the audio thread runs both convolvers on copies of the input and mixes them with a linear A→B ramp over the crossfade duration, then `std::swap`s the pointers. To avoid clobbering the idle convolver mid-crossfade, the render thread waits while `engine.isIRPending()` (swap queued OR crossfade running) before loading the next IR — this also makes rapid scene changes converge to the most-recent selection via the one-deep replacement queue.

**Embedded default IR / cold start:** `Resources/Presets/default_ir.wav` (small concrete room, 100k rays, ~563 KB) is embedded via `juce_add_binary_data` and loaded synchronously in `prepareToPlay`, so audio is worldized immediately on cold start — no wait for a render. Scene changes (and restoring a non-default scene) trigger the background ray-trace + IR crossfade (~0.1 s at 50k rays).

**IR config into juce::dsp::Convolution:** `Stereo::no` (mono IR applied per channel), `Trim::yes` (direct lands at sample 0 → dry/wet aligned, near-zero latency), `Normalise::yes` (sane, non-clipping, consistent wet level across scenes). Convolution latency is ~0, so the dry-path delay line is effectively passthrough (wired up regardless).

**Deviations:** (1) classes keep the established `WorldizerAudioProcessor`/`...Editor` names, not the prompt's generic `PluginProcessor`. (2) Used `juce::dsp::Gain` for input/output gain (clean multi-channel smoothing) instead of the prompt's manual rewind-skip smoother, and a pre-allocated `mixRamp` for the blend. (3) No "zero-pad IRs to fixed size" defensive step — unnecessary because `juce::dsp::Convolution` load/process are wait-free (allocation happens on its own loader thread, never the audio thread). (4) Version bumped to 0.0.2.

**Audition feature (added after the initial Slice 2 commit):** built-in dry test
signals — **Click** (4 transients), **Sweep** (3 s log 20 Hz–20 kHz), **Noise**
(1 s gated burst) — generated in `prepareToPlay` and injected at the top of
`processBlock` (RT-safe, *before* the bypass check so bypass plays the dry signal
and A/Bs the effect rather than muting it). Triggered from three editor buttons; no
host content needed. Loudness is matched by **active-region RMS, tuned against
BS.1770/LUFS** (equal RMS ≠ equal loudness — a sustained sweep reads far louder than
transient clicks; the click is transient-limited and maxed to a peak ceiling).

**To revisit:** scene-change crossfade quality is by-construction click-free but unverified by ear (pluginval doesn't change scenes). `getTailLengthSeconds` reports the max IR length (6 s) — conservative; could report the actual trimmed IR length later. (Slice 3 resolves the state-restore re-render via `.wzpreset` IR loading.)

---

## Slice 3: Geometry Data Model + Serialization

**Goal:** Scene state can be loaded from and saved to disk.

- [x] `WzPresetIO.h/cpp` — read/write `.wzpreset` bundles (geometry.json + rendered.wav + metadata.json + thumbnail.png) + `.wzpkg` pack/unpack for embedding
- [x] JSON schemas documented (`Docs/wzpreset_format.md`; `Docs/architecture.md` §4 updated to match)
- [x] `MaterialResolver` (name → Material), `PresetManager` (shipped + user libraries)
- [x] `BakePresets` CLI exports the five test scenes as `.wzpreset` bundles + embedded `.wzpkg`
- [x] Plugin loads presets from embedded data + user folder; selection swaps the baked IR with **no rendering**
- [x] State save/restore stores the preset id and loads the baked IR (instant; Slice 2 `currentScene` migrated)
- [x] `pluginval --strictness-level 10` passes; Universal Binary

**Acceptance:** Plugin loads a preset from disk/embedded data and audibly applies it; switching presets is a file read + crossfade, not a render. **Verified programmatically:** fresh-checkout build (HAS_SHIPPED=0) → BakePresets → rebuild (HAS_SHIPPED=1) workflow; well-formed schema-valid JSON; pluginval strictness 10; Universal Binary. **Needs user/DAW:** by-ear preset switching, instant state restore feel, user-folder workflow, Soundminer smoothness.

### Slice 3 retrospective

**Preset format / embedding:** `.wzpreset` is a directory (geometry.json + rendered.wav + thumbnail.png + metadata.json). Shipped presets are additionally packed into one **`.wzpkg` blob per preset** (length-prefixed concatenation) for binary embedding — this avoids the binary-data **symbol collisions** the prompt's per-file embedding would hit (every bundle has a `geometry.json`, etc.). `PresetManager` enumerates embedded presets via JUCE's `namedResourceList` (no hardcoded id list) and merges in user `.wzpreset` dirs from `~/Library/Application Support/ZQSFX/Worldizer/Presets/` (user overrides shipped by id).

**Render thread is now dormant:** preset selection = `PresetManager::loadPreset` (file/binary read) + `ConvolutionEngine::loadIR`. The render thread only runs as a recovery path if a preset is missing its `rendered.wav`. State restore loads the baked IR — no re-render.

**Chicken-and-egg / fresh checkout:** the generated bundles are *not* committed (they're regenerable from `TestScenes` via `BakePresets`). A fresh checkout builds with `WORLDIZER_HAS_SHIPPED_PRESETS=0` (empty preset list; the embedded `default_ir.wav` still gives instant audio). Run `BakePresets`, then rebuild — `file(GLOB ... CONFIGURE_DEPENDS)` picks the new `.wzpkg` files up and flips `HAS_SHIPPED=1`. Documented in `README.md`.

**Deviations:** (1) packed `.wzpkg` embedding instead of per-file (symbol collisions). (2) Kept the embedded `default_ir.wav` as the instant cold-start bootstrap + fresh-checkout fallback (layered under the preset system). (3) `BakePresets` links `juce_gui_basics` (graphics + `ScopedJuceInitialiser_GUI`) for thumbnail rendering, not just `juce_graphics`. (4) Slice 2 `getCurrentSceneName`/`setCurrentSceneName` kept as deprecated shims onto the preset API. (5) `bounds` serialized as `min`/`max` arrays (the Slice 0 architecture sketch used `x`/`y`/`z`); architecture.md updated.

**To revisit:** thumbnails are name-on-dark placeholders (real geometry render in Slice 4). The editor's preset combo populates once (added user presets need a plugin reopen — live rescan is a Slice 4 polish). Subtractive brushes are parsed but not yet ray-traced. Float coordinates serialize at full precision (verbose but valid).

---

---

## Slice 4: Browse Mode UI (no editor yet)

**Goal:** First real UI. Preset browser + draggable source/mic on top-down geometry view.

- [ ] `WorldizerLookAndFeel` with full color system
- [ ] `RoomView2D` component — paints brushes as outlines, source/mic as icons, supports drag
- [ ] `PresetBrowser` component — categorized list, click-to-load
- [ ] Standard controls panel: Distance, Source character (placeholder dropdown), Mic character (placeholder dropdown), Ambient bed level, Mix, Output gain
- [ ] Source/mic drag in the top-down view triggers low-quality preview ray trace, IR crossfades
- [ ] Drag release triggers full-quality ray trace, IR crossfades again

**Acceptance:** Plugin loads with a preset; user can drag source and mic in the room view and hear the worldizing morph in real time. Switching between 3 test presets works.

---

## Slice 5: Source & Mic Character Library

**Goal:** Recorded speaker IRs and mic IRs integrated.

- [ ] `SourceCharacter.h/cpp` — convolution with selected speaker IR, optional light nonlinearity
- [ ] `MicCharacter.h/cpp` — convolution with selected mic IR, optional self-noise
- [ ] 6-8 speaker IRs in `Resources/Speakers/` (placeholder WAVs acceptable for now)
- [ ] 5-6 mic IRs in `Resources/Mics/` (placeholder WAVs acceptable for now)
- [ ] Source character browser UI
- [ ] Mic character browser UI
- [ ] Audition-on-hover: hovering a speaker/mic temporarily applies it

**Acceptance:** User can switch between speakers/mics and hear distinct character changes. Audition-on-hover feels instant.

---

## Slice 6: Ambient Bed System

**Goal:** Room tone beds layered with the worldized signal.

- [ ] `AmbientBed.h/cpp` — looped playback of a room tone WAV with crossfade at loop points
- [ ] Ambient bed level parameter
- [ ] Per-preset ambient bed assignment (in `metadata.json`)
- [ ] 4-6 ambient bed WAVs in `Resources/RoomTones/` (placeholder acceptable)

**Acceptance:** Loading a preset with an ambient bed mix gives an audible bed under the worldized signal. Adjusting the level works. The bed loops seamlessly.

---

## Slice 7: In-Plugin Editor (basic)

**Goal:** User can create and modify axis-aligned box brushes.

- [ ] Edit mode toggle in main UI
- [ ] Click-drag on top-down view in edit mode to add a brush
- [ ] Click brush to select; inspector shows height, material, additive/subtractive
- [ ] Drag brush to move; drag corner to resize
- [ ] Delete key removes selected brush
- [ ] Material picker (list of materials from `Resources/Materials/materials.json`)
- [ ] "Save As Preset" — bakes IR, writes `.wzpreset` to user library folder
- [ ] Edit triggers background ray trace at preview quality during drag, full quality on debounce
- [ ] Convolver crossfades cleanly

**Acceptance:** User can build a simple room (4 walls + floor + ceiling), place source and mic, hear it, save it as a preset, reload it, and hear the same result.

---

## Slice 8: `.map` Importer

**Goal:** Import TrenchBroom-authored `.map` files.

- [ ] `MapImporter.h/cpp` — parse Quake `.map` format
- [ ] Texture name → material mapping table
- [ ] Entity type recognition: `worldizer_source`, `worldizer_mic` (with fallback defaults)
- [ ] On import, ray-trace at full quality and save as `.wzpreset`
- [ ] UI flow: "Import .map..." in edit mode, file picker, progress indicator during bake
- [ ] TrenchBroom configuration file in `Resources/TrenchBroom/` defining Worldizer textures and entities

**Acceptance:** A user can build a forest clearing in TrenchBroom, save the `.map`, import it into Worldizer, and hear it as a worldizing preset.

---

## Slice 9: Library Content

**Goal:** Real recorded assets and curated presets.

- [ ] 20+ shipped `.wzpreset` files across categories (Indoor, Outdoor, Vehicles/Devices, Cinematic, Experimental)
- [ ] 10 real speaker IR recordings
- [ ] 8 real mic IR recordings
- [ ] 10 real room tone bed recordings
- [ ] 20 materials with measured absorption + scattering data in `Resources/Materials/materials.json`
- [ ] Thumbnail images for each preset
- [ ] (Library recording is parallel work, can begin during earlier slices)

**Acceptance:** Plugin ships with content that demonstrably represents the breadth of worldizing use cases.

---

## Slice 10: Polish & Alpha Release

**Goal:** Shippable alpha.

- [ ] Universal Binary builds clean
- [ ] Code signing and notarization scripts work
- [ ] Soundminer compatibility verified (cold start, state save/restore, no audio glitches)
- [ ] pluginval strictness 10 passes
- [ ] README polished with screenshots and video links
- [ ] Onboarding documentation: a 5-minute "first use" video script
- [ ] GitHub release published with signed binaries

**Acceptance:** External alpha testers can install and use the plugin without ZQ's direct help. Five alpha testers give feedback. Issues are triaged and prioritized for v1.0.

---

## Post-MVP (v1.0 and beyond)

**v1.0 themes:**
- Multi-mic arrays (5.1, ORTF, spaced pair) and multichannel output
- Source/mic directivity controls
- Position automation with Doppler
- Source rotation in 2D view
- Murch / Burtt mode toggle with workflow defaults
- Boolean operations in editor (carving)
- "Randomize" button (Burtt mode)
- Larger curated library (50+ presets)

**v2.0 themes:**
- Multi-floor / mezzanines (3D editing)
- Voxel paint for outdoor environments
- Atmospheric effects (wind, temperature gradients)
- Custom mic array authoring
- Optional 3D preview pane

**Never ships:**
- Reverb-style tail parameters
- Genre presets ("Drum Room", "Vocal Plate")
- Spectral display
- DRM, paid features, telemetry
