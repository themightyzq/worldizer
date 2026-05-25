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
- Performance is far inside budget: 20k rays trace in ~0.05s, 200k in ~0.5s (no BVH needed yet).
- The principled reflection normalization (`2/micRadius · √(E/numRays)`) puts reflections on the same physical scale as the `1/d` direct sound.

**Harder / surprising**
- `juce::Vector3D` lives in `juce_opengl`, which would drag the GUI stack into the headless tool. Used a small `Worldizer::Vec3` instead (matches the architecture's "pure C++ core" goal). All engine types unified under the `Worldizer` namespace.
- **Specular early reflections over-spike.** With low-scattering surfaces (concrete/glass), many rays focus into one time bin, so the first floor bounce reads ~12 dB louder than the `1/path` law predicts — it even exceeds the direct in the small room. This is the known limitation of pure stochastic ray tracing for low-order specular reflections.
- The 10 cm mic + 20k rays under-samples large/open scenes (gym ~37 hits, forest 0). Hit count scales linearly with rays (200k → gym 395, forest 27), so it's sampling density, not a bug. IRs are noticeably cleaner at 100k–200k rays.

**Deferred (to Slice 1.5 / 2 / later)**
- `ImageSourceSolver` — handles orders 1–3 specular accurately and will fix the early-reflection over-spike (hybrid ISM + ray tracing per the architecture).
- Per-band noise-filtered IR reconstruction (Slice 1 uses the pragmatic broadband-energy-per-bin shortcut, which is correct in energy but not spectrally detailed).
- Statistical late-tail synthesis; stereo / multi-mic IRs; JSON scene loading; larger default mic radius or adaptive ray counts for big/open scenes.

---

## Slice 2: Convolution Runtime

**Goal:** Plug the ray-traced IR into the audio thread.

- [ ] `ConvolutionEngine.h/cpp` — wraps `juce::dsp::Convolution`, supports background IR loading with crossfade
- [ ] `PluginProcessor` integrates `ConvolutionEngine`
- [ ] Load a hardcoded baked IR at startup
- [ ] Wet/dry mix parameter
- [ ] Bypass works correctly
- [ ] No allocations in `processBlock`
- [ ] No clicks on IR swap

**Acceptance:** Plugin loaded in a DAW with the hardcoded IR audibly worldizes input audio. Bypass A/Bs cleanly. CPU usage is reasonable (< 5% on a modern Mac for a 2-second IR).

---

## Slice 3: Geometry Data Model + Serialization

**Goal:** Scene state can be loaded from and saved to disk.

- [ ] Complete `Scene.h/cpp` data model
- [ ] `WzPresetIO.h/cpp` — read/write `.wzpreset` bundles (geometry.json + rendered.wav + metadata.json + thumbnail.png)
- [ ] JSON schemas documented in `Docs/architecture.md`
- [ ] Hand-author 2-3 test `.wzpreset` bundles (small room, hallway, outdoor clearing)
- [ ] Plugin can load a `.wzpreset` from a hardcoded path and use its baked IR

**Acceptance:** Plugin loads a preset file from disk and audibly applies it. Modifying the preset's WAV file changes the sound on next load.

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
