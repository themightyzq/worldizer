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

Both modes share one engine: a ray tracer + image-source solver that bakes an impulse response from your scene, a curated library of recorded speakers, microphones, and room tones, and a lightweight 2D top-down editor.

## Status

> **Pre-alpha — under active development. Not yet ready for production use.**

The project is being built in slices (see [`TODO.md`](TODO.md)). The current scaffold builds and loads as a silent pass-through plugin; DSP and UI arrive in subsequent slices.

## Quick Start (build from source)

Worldizer uses CMake and JUCE 8.x (added as a git submodule).

```bash
# 1. Clone with submodules
git clone --recurse-submodules https://github.com/zqsfx/worldizer.git
cd worldizer

# If you already cloned without --recurse-submodules:
git submodule update --init --recursive

# 2. Configure and build (Release, Universal Binary on macOS)
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

The built artefacts land in `build/Worldizer_artefacts/Release/`. With `COPY_PLUGIN_AFTER_BUILD` enabled, the VST3 is also installed to your user VST3 folder.

Convenience scripts live in [`Scripts/`](Scripts/):

```bash
./Scripts/build_universal.sh            # clean Release build
./Scripts/verify_universal_binary.sh    # assert arm64 + x86_64
```

### Requirements

- macOS 10.15+ (Universal Binary: arm64 + x86_64)
- CMake 3.22+
- Xcode Command Line Tools

## Plugin Formats & Host Compatibility

- **Formats:** VST3, Standalone
- **Primary host:** Soundminer (the plugin is tuned for fast cold start and clean state save/restore — see [`Docs/architecture.md`](Docs/architecture.md))
- **Also targets:** Reaper, Pro Tools, Logic, and any VST3-capable DAW
- **Channels:** stereo in / stereo out (multichannel is post-MVP)

## License

Worldizer is licensed under the **GNU General Public License v3.0**. See [`LICENSE`](LICENSE). This matches JUCE's open-source license terms. There is no DRM, there are no paid features, and there is no telemetry.

## Contributing

Contributions are welcome — code, bug reports, feature ideas, and especially **recordings** (speaker IRs, mic IRs, room tones). See [`CONTRIBUTING.md`](CONTRIBUTING.md) for how to get involved, and [`Docs/contributing_recordings.md`](Docs/contributing_recordings.md) for the recording workflow.

## Acknowledgments

- **[JUCE](https://juce.com/)** — the C++ framework Worldizer is built on.
- **[TrenchBroom](https://trenchbroom.github.io/)** — the level editor used to author complex spaces, imported via the Quake `.map` format.
- **Walter Murch and Ben Burtt**, and the wider tradition of worldizing in film sound, whose practice this plugin is a love letter to.
