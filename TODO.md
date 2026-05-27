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

- [x] `WorldizerLookAndFeel` with the committed color system (dark neutral + amber) and amber knob/button/scrollbar/texteditor drawing
- [x] `RoomView2D` — top-down geometry outlines + grid + scale bar, source (amber) / mic (cyan) icons, drag in the horizontal plane, `renderToImage` / `renderSceneThumbnail`
- [x] `PresetBrowser` — collapsible sidebar, search filter, scrollable list with thumbnails, Save-As placeholder, 1 Hz user-folder polling
- [x] Control row: Input Gain / Mix / Output Gain knobs + Click/Sweep/Noise audition + rendering indicator (the character/ambient controls are Slices 5–6, not this slice)
- [x] Source/mic drag → preview render (5k rays, 30 ms crossfade); release → full render (50k, 100 ms) via `RenderThread::Job` Scene-snapshot + Quality
- [x] Real geometry thumbnails baked into presets (BakePresets uses `renderSceneThumbnail`)
- [x] State save/restore includes source/mic positions + sidebar collapse (skips re-render when positions == defaults); 900×650 resizable window
- [x] pluginval --strictness-level 10 passes; Universal Binary

**Acceptance:** Plugin loads with a preset; user drags source/mic and hears the worldizing morph live; switching presets works. **Verified:** clean build, real thumbnails, pluginval strictness 10, UB, and a screenshot confirming the full UI renders as a coherent product (`Docs/images/screenshot.png`). **Needs user/DAW:** drag responsiveness by ear, user-folder polling round-trip, Soundminer fit.

### Slice 4 retrospective

**Visual language:** committed in `Worldizer::Colors` (dark neutral `#1a1a1a`/`#242424` + amber `#ffab00`; mic is the lone cyan accent). `WorldizerLookAndFeel` draws amber rotary arcs with an indicator pip, outline/filled buttons, and a thin scrollbar. The product reads coherently (see screenshot).

**RoomView2D:** a static `drawScene()` helper computes an auto-fit scene→view transform (10% padding, +Y up) and is shared by the live component and `renderSceneThumbnail()`, so thumbnails are pixel-identical to the live view. Drag projects the mouse back to scene XY (Z fixed), soft-clamps to bounds, and fires `onPositionsChanged` (preview) / `onPositionsFinalized` (full).

**Live drag rendering:** `RenderThread::Job` now carries a `Scene` snapshot + `Quality` (Preview 5k/12 vs Full 50k/32) instead of a scene name; the one-deep replacement queue collapses rapid drag updates to the latest. Drag edits are ephemeral (not written to the preset on disk); the header subtitle shows `*` when modified.

**Deviations:** (1) UI/processor position APIs use `Worldizer::Vec3` (not `juce::Vector3D`, consistent with the engine). (2) `RoomView2D::hitTest` renamed `pickTarget` (avoids hiding `Component::hitTest`). (3) sidebar-collapse state stored on the processor (atomic) so it persists even when the editor is closed. (4) `BakePresets` renders real 128×128 thumbnails (links `RoomView2D.cpp`). (5) `WorldizerLookAndFeel` exposes a global `using` alias for the global-namespace editor. Version → 0.0.3.

**To revisit:** the browser populates once + polls the user folder at 1 Hz (no live shipped-list changes). No user zoom/pan in the room view. Source "facing" arrow points at the mic (omni source has no real orientation yet). Edit button is a disabled placeholder (Slice 7).

---

## Slice 4.5: DSP Polish Pass (distance model + late-tail synthesis)

**Goal:** Make distance audibly dramatic and make tails decay smoothly to silence,
without breaking dry/wet alignment or any Slice 4 functionality. No UI changes.

- [x] `DistanceModel` (header-only) — dual accurate↔musical curves for time-of-flight pre-delay and inverse-distance attenuation; default `accuracy = 0.4`; documented rationale
- [x] `Trim::yes`/`Normalise::yes` revisited and KEPT intentionally; IR is now a distance-independent room response (direct at sample 0, reflections relative); rationale documented in `ConvolutionEngine` + `architecture.md` §8
- [x] Statistical per-band late-tail synthesis with crossfade from ray-traced → synthesised, gated by the data cliff; 50 ms end fade to silence
- [x] Wet path applies pre-delay (`Lagrange3rd` delay line) + attenuation, both smoothed (~50 ms); dry path matched to convolver latency only
- [x] `IRInspect` CLI (render/analyze: peak, RMS, first-nonzero, energy distribution, per-band decay, RT60, decay envelope; `--no-tail`, `--distance`, `--accuracy`)
- [x] `DistanceTest` confirms the curves are mathematically correct; `MicMoveTest` updated (IR changes with mic, direct at sample 0, distance model responds)
- [x] `pluginval --strictness-level 10` passes; Universal Binary; presets re-baked

**Acceptance:** Dragging the mic from near to far produces an unambiguous distance
change; tails fade smoothly to silence. **Verified programmatically:** DistanceTest
matches the expected curves to the sample; IRInspect confirms direct@0, −1 dBFS peak,
smooth monotonic decay → silence on all five scenes (no cliff); the gym 2 m→20 m
offline renders show a 30 dB / 31 ms (musical) or 20 dB / 52 ms (accurate) change;
pluginval strictness 10; clean build. **Confirmed by ZQ in the DAW (by ear):** the
drag-distance feel (§9.6), tail naturalness (§9.7), no pumping, Soundminer drop-in
(§9.12) and multi-instance (§9.11) — all confirmed by ear (see retrospective).

### Slice 4.5 retrospective

**Distance model (wet-path only).** The IR is a timing- and level-normalised room
response: the direct sits at sample 0 with its 1/distance amplitude kept RELATIVE to
the reflections (that ratio is the direct-to-reverberant cue, which is distance-
dependent), reflections are placed relative to the direct, and the whole IR is
peak-normalised. `DistanceModel` then applies the other two cues on the **wet path
only** — time-of-flight pre-delay and inverse-distance attenuation — leaving the dry
path as the un-propagated source. Dual curves blend by `accuracy`: musical (0) = 0.6×
delay + 1/r^1.5 level; accurate (1) = d/c + 1/r. Default **`accuracy = 0.4`** (biased
musical for SFX punch; confirmed by ear). `referenceDistance` is set to each preset's
default source/mic spacing on load, so a preset plays at its design level and dragging
changes level *relative* to that. A `maxGain` (+6 dB) ceiling bounds very-close
placements. 2 m→20 m gives the gym a 30 dB / 31 ms change (musical) or 20 dB / 52 ms
(accurate); both unmistakable, musical leaning on level, accurate on delay.

**Statistical late-tail synthesis.** The ray-traced echogram is finite and, in large
open scenes, **hit-starved**: the 10 cm mic catches so few rays that the histogram
cliffs to digital silence (the gym at ~1.4 s) long before the room stops ringing.
`IRBuilder` detects that cliff, fits the decay slope over the clean region before it,
and continues the decay past it with per-band band-limited noise under an exponential
envelope, crossfaded in *inside* the clean region so the handoff is seamless. Each
band's slope is clamped to be no slower than the broadband aggregate (so one slow LF
band can't bloat the tail) while faster HF bands keep darkening it. It is a
*statistical* reconstruction — the rate and spectral tilt are real, the fine texture
is synthesised — not a literally-traced tail.

**Tuned by ear (what the meters missed).** Four issues surfaced only on ZQ's DAW
listening, all corrected: (a) **reverb too faint / "tails short," esp. gym** — an early
version set the direct to flat unit amplitude, which inflated it ~20 dB over the reverb
in large rooms and broke the direct-to-reverberant cue; restoring the 1/distance
relative scaling lifted the gym reverb ~17 dB (IR RMS −44.6 → −31.2 dBFS). (b) **too
quiet / falloff too aggressive** — attenuation referenced a fixed 1 m, but no preset's
mic is at 1 m (gym default 10 m → −24.6 dB by default); re-referencing to the preset's
design distance fixed it. (c) **tail pumps** — the envelope-smoothing window was far too
narrow for sparse hits, so gaps between ray hits bounced the envelope ±4–6 dB and
modulated the noise; widening the growth to 0.09·t with a ~400 ms cap dropped it to the
±2 dB inherent to filtered noise. (d) **tails cut off** — a long-standing bug: the final
fade was applied to the zero-pad past the signal with an inverted ramp, so the tail
stepped off the −75 dB trim floor with no fade; now the buffer ends at the last audible
sample with a raised-cosine fade to exactly zero, and the crossfade was moved fully
inside the clean decay region to remove a level plateau.

**Output safety.** A stateless transparent soft-clip ceiling ends the chain (bit-exact
below ~−1.5 dBFS, asymptoting to ±1.0). Convolution can sum to peaks above the input
and dense input builds reverberant energy, so this stops the output clipping the mixer
— without a compressor/limiter, whose time-varying gain would pump (the artifact we are
avoiding) and add latency. Steady-state buildup is already bounded by the convolver's
`Normalise::yes`.

**Audition.** A single-transient **Click** was added (the right probe for hearing one
clean tail decay); the original four-transient burst is kept as **Clicks**. These and
all the tail fixes are deliberately general IR-quality / neutral-ceiling changes — *not*
tuned to our dry sources or presets — so the plugin sounds predictable with any input.

**Final scene measurements (IRInspect, 100k rays):** anechoic 0.20 s (dry); small
concrete 1.01 s / RT60 0.67 s; hallway 1.21 s / 1.11 s; forest 0.20 s; gymnasium
4.61 s / RT60 4.80 s. All decay smoothly and monotonically with a clean fade to
silence. The gym is longer than the old "~2.6 s" characterisation, which was read off
the *truncated* trace; the synthesised tail exposes the full LF-dominated decay
(~4.8 s), realistic for an untreated concrete sports hall.

**Deviations from the prompt.** (1) The direct keeps its 1/distance scaling *relative*
to the reflections — the prompt's flat-unit direct broke the direct-to-reverberant cue.
(2) Attenuation is referenced to each preset's default spacing, not a fixed 1 m.
(3) The tail amplitude uses `decibelsToGain(startDb + slope·t)` (= √energy, continuous
with the ray-traced envelope), not the prompt's `dB/2` (a unit error that would jump
the level). (4) The crossfade is cliff-gated at 1.0× RT60 inside the clean region, not
the prompt's 1.5×. (5) Per-band slope clamped ≥ broadband; the trailing-silence trim
measures the floor relative to the *reverb* peak (the direct spike dwarfs the tail).
(6) Added a soft-clip safety ceiling and a `maxGain` clamp (not in the spec).
(7) Attenuation is applied via a pre-filled ramp buffer, not the prompt's `skip(-n)`
rewind (avoids negative skip on a multiplicative smoother). (8) `DistanceModel` is
header-only; `IRInspect` is a standalone tool; IR length is adaptive (trim to the decay
floor, capped at 6 s). Version → 0.0.4.

**Performance.** Full gym render (50k rays + tail): trace 0.13 s + IR build 0.03 s =
0.17 s — tail synthesis adds only a few ms. Preview (drag) renders skip synthesis.
Runtime cost is a delay line + a smoothed multiply (negligible); pluginval strictness
10 passes across sample rates / block sizes.

**To revisit.** The gym RT60 (~4.8 s) and `accuracy = 0.4` are by-ear defaults, easy
to nudge. Air absorption is not yet distance-dependent on the direct path. The ray
tracer over-traces to 4 s while big-room data cliffs early (an orthogonal engine
tweak). The IR is still mono — stereo / multi-mic is the next high-value item, since it
carries the distance/delay cues into a stereo image (tracked in the cross-cutting
backlog).

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

## Cross-cutting backlog (flagged during Slice 4 testing)

These are real gaps confirmed by ear/testing, to be addressed in future slices — not bugs in the current scope:

- ✅ **RESOLVED in Slice 4.5 — Distance isn't dramatic enough.** The IR is now a distance-independent room response (direct at sample 0); `DistanceModel` applies time-of-flight pre-delay + inverse-distance attenuation on the wet path (dual accurate↔musical curves). `Trim::yes`/`Normalise::yes` kept (latency ~0, level consistent) — the distance cues are no longer baked into / discarded by the IR. 2 m→20 m now gives 20–30 dB + 31–52 ms of change.
- ✅ **RESOLVED in Slice 4.5 — Tail has a hard limit.** Statistical per-band late-tail synthesis continues the measured decay past the ray-traced echogram (which cliffs early in hit-starved scenes) and a 50 ms end fade guarantees clean silence. All five test scenes now decay smoothly to silence.
- **No panning / stereo image.** The IR is mono, applied identically to L/R, so source/mic geometry produces no stereo. Needs a **stereo / multi-mic IR** (two capsules → inter-channel time/level differences). Currently a v1.0 theme; high-value, consider pulling forward (it also carries the distance/delay cues).
- **Out-of-bounds source/mic is crude.** Drag is soft-clamped to ~1 m past the scene bounds; beyond a wall the direct just occludes. Want a more elegant solution: either disallow placing source/mic outside the main area entirely, or render the through-wall case accurately (transmission/occlusion).

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
