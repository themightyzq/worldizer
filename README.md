# Worldizer

**Think spatially, not parametrically. Re-record your sounds through a speaker into a space that only exists in the plugin.**

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](LICENSE)
![Build](https://img.shields.io/badge/build-pre--alpha-orange)
![Format](https://img.shields.io/badge/format-VST3%20%7C%20Standalone-lightgrey)

---

## What is Worldizer?

Worldizer is a free, open-source VST3 plugin for SFX sound designers. It simulates **worldizing** — the technique pioneered by Walter Murch and Ben Burtt of re-recording a sound through a loudspeaker into a real physical space, then capturing it with microphones. Instead of dialing in abstract reverb parameters, you place a source and a microphone inside a space and let a geometric acoustics engine compute how the sound actually travels.

Worldizer is built for two complementary ways of working:

- **Murch mode (realism).** Place dialogue or effects believably in a space — a stairwell, an alley, a parking garage. Distance is the headline cue; room tone, signal-path character, and source/mic choices do the rest.
- **Burtt mode (transformation).** Treat the speaker and the space as creative instruments. Push extreme reproducers and impossible geometries to make sounds that never existed.

Both modes share one engine: a Monte Carlo ray tracer (with statistical late-tail synthesis) that bakes an impulse response from your scene, a 2D top-down editor for authoring spaces, and — on the roadmap — a curated library of recorded speakers, microphones, and room tones.

**Directional acoustics (v0.0.5):** mics can be omnidirectional or shotgun (rotate the
shotgun toward or away from the source for a real, physical tonal change), and you can
choose a single mic, a coincident **stereo XY** pair (adjustable splay angle), or a
**spaced pair** — the last two produce a genuine stereo impulse response with the
inter-channel level and time differences of the real recording techniques.

**Doom-style geometry editor (v0.0.6):** switch to **Edit** mode and draw your own
acoustic space sector-by-sector — click vertices in the top-down view, click the first
vertex to close, then set floor / ceiling heights and per-surface materials in the
right-side inspector. Hear the space update live as you drag vertices or change
materials, and **Save As** to add it to your user library.

**Source & mic character + ambient beds (v0.0.7):** pick the *reproducer* the sound
plays through (telephone handset, guitar cab, megaphone, transistor radio...) and the
*microphone* that captures the room (ribbon, stage dynamic, carbon button, contact
mic...) — hover a character in the picker to audition it instantly, click to keep it.
A **Drive** knob adds light speaker nonlinearity and a **Noise** knob adds mic
self-noise. Each preset can carry a looped **ambient room-tone bed** (HVAC, city
rumble, fluorescent buzz...) mixed under the worldized signal at its own level. The
shipped characters and beds are synthesized placeholders with real recordings to
follow — the ids and plumbing are final.

**Preset library (v0.0.7):** 25 shipped spaces across Test / Indoor / Outdoor /
Vehicles & Devices / Cinematic / Experimental — from a closet and a tiled bathroom to
a cathedral, a canyon, and a deliberately unreal 64 m glass corridor — plus a
20-material acoustic library (published absorption data) available in the editor.

## Screenshot

![Worldizer browse mode](Docs/images/screenshot.png)

*Browse mode: the preset browser (left) with real top-down thumbnails, the room view
with draggable source (amber) and mic (cyan), the gain/mix/audition row, the mic
array row (config / pattern), and the character row — speaker picker + Drive, mic
picker + Noise, and the ambient bed level.*

## Status

> **Pre-alpha — under active development. Not yet ready for production use.**

The project is being built in slices. The acoustics engine, convolution runtime,
browse-mode UI, directional/stereo mics, the Doom-style geometry editor, the source/mic
character chain, ambient beds, the 25-preset library, and the 20-material acoustic
library are all in place. What remains before alpha: replacing the synthesized
placeholder characters/beds with real recordings, by-ear tuning, and signed/notarized
release builds.

## Quick Start (build from source)

Worldizer uses CMake and JUCE 8.x (added as a git submodule).

```bash
# 1. Clone with submodules
git clone --recurse-submodules https://github.com/themightyzq/worldizer.git
cd worldizer

# If you already cloned without --recurse-submodules:
git submodule update --init --recursive

# 2. Configure and build (Release, Universal Binary on macOS)
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release

# 3. First-time setup only: bake the shipped assets (characters, room tones,
#    materials.json) and presets, then rebuild to embed them. (A fresh checkout
#    builds fine without this — it just has no libraries yet.)
./build/BakeAssets_artefacts/Release/BakeAssets
./build/BakePresets_artefacts/Release/BakePresets
cmake --build build --config Release
```

The built artefacts land in `build/Worldizer_artefacts/Release/`. With `COPY_PLUGIN_AFTER_BUILD` enabled, the VST3 is also installed to your user VST3 folder.

**About the bake step:** the shipped presets (`.wzpreset` bundles), speaker/mic
character IRs, and ambient room-tone beds are all generated from in-repo definitions
by the `BakeAssets` and `BakePresets` tools and embedded into the plugin. They are
not committed to the repo, so on a fresh checkout you run both once and rebuild.
After that, rebuilds pick them up automatically. User presets live under your OS user-data directory —
`~/Library/Application Support/ZQSFX/Worldizer/Presets/` on macOS, `%APPDATA%\ZQSFX\Worldizer\Presets\`
on Windows, `~/.config/ZQSFX/Worldizer/Presets/` on Linux (drop a `.wzpreset` directory
there and restart the plugin).

Convenience scripts live in [`Scripts/`](Scripts/):

```bash
./Scripts/build_universal.sh            # clean Release build
./Scripts/verify_universal_binary.sh    # assert arm64 + x86_64
```

### Requirements

Worldizer targets **macOS, Windows, and Linux** as equal-priority platforms (CI builds
and tests all three):

- CMake 3.22+ and a C++17 toolchain
- **macOS:** 10.15+, Xcode Command Line Tools (builds a Universal Binary: arm64 + x86_64)
- **Windows:** Visual Studio 2022 (MSVC) build tools
- **Linux:** GCC/Clang + the JUCE dev packages (ALSA, X11, FreeType, Mesa/GL — see the
  `Install Linux dependencies` step in [`.github/workflows/build.yml`](.github/workflows/build.yml))

## Plugin Formats & Host Compatibility

- **Formats:** VST3, Standalone
- **Primary host:** Soundminer (the plugin is tuned for fast cold start and clean state save/restore)
- **Also targets:** Reaper, Pro Tools, Logic, and any VST3-capable DAW
- **Channels:** stereo in / stereo out (multichannel is post-MVP)

## License

Worldizer is licensed under the **GNU General Public License v3.0**. See [`LICENSE`](LICENSE). This matches JUCE's open-source license terms. There is no DRM, there are no paid features, and there is no telemetry.

## Contributing

Contributions are welcome — code, bug reports, feature ideas, and especially **recordings** (speaker IRs, mic IRs, room tones). Open an issue or pull request on GitHub to get involved.

## Acknowledgments

- **[JUCE](https://juce.com/)** — the C++ framework Worldizer is built on.
- **Walter Murch and Ben Burtt**, and the wider tradition of worldizing in film sound, whose practice this plugin is a love letter to.
