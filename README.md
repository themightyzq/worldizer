# Worldizer

Worldizer is a plugin that simulates worldizing: the technique, pioneered by Walter
Murch and Ben Burtt, of re-recording a sound through a loudspeaker into a real physical
space and capturing it with microphones, which film sound uses to make an effect or a
line of dialogue sit believably in a place. Instead of dialing in abstract reverb
parameters, you place a source and a microphone in a room drawn in a 2D top-down
editor, and a geometric acoustics engine computes how the sound actually travels.
Built with JUCE for macOS, Windows, and Linux.

Worldizer supports two ways of working. Murch mode places dialogue or effects
believably in a space, where distance is the main cue and room tone and source/mic
choice do the rest. Burtt mode treats the speaker and the space as creative
instruments, pushing extreme reproducers and impossible geometries into sounds that
never existed. Both share the same acoustics engine.

Mics can be omnidirectional or shotgun, and you can choose a single mic, a coincident
stereo XY pair, or a spaced pair, the last two producing a genuine stereo impulse
response. The geometry editor lets you draw your own space vertex by vertex, set floor
and ceiling heights and per-surface materials, and hear the space update live as you
edit. You can pick the reproducer the sound plays through (telephone handset, guitar
cab, megaphone, transistor radio) and the microphone that captures the room (ribbon,
stage dynamic, carbon button, contact mic), add speaker drive and mic self-noise, and
layer a looped ambient room-tone bed (HVAC, city rumble, fluorescent buzz) under the
worldized signal. 25 rooms ship as presets, from a closet and a tiled bathroom to a
cathedral and a canyon, alongside a 20-material acoustic library with published
absorption data.

## Screenshot

![Worldizer browse mode](Docs/images/screenshot.png)

Browse mode: the preset browser with top-down thumbnails, the room view with a
draggable source and mic, the gain/mix/audition row, the mic array row, and the
character row for speaker and mic picks plus the ambient bed level.

## Install

Version 0.0.8. There are no packaged or signed releases yet. Build from source (below);
the built plugin is unsigned, so on macOS the first launch needs right-click, Open.

## Use

1. Pick a room from the preset browser, or switch to Edit mode to draw your own space.
2. Drag the source and the microphone into position in the top-down view.
3. Choose a mic pattern (omni, shotgun, stereo XY, or spaced pair) and, if shotgun,
   rotate it toward or away from the source.
4. Pick a speaker character and a mic character, and dial in Drive and Noise.
5. Optionally add an ambient room-tone bed and set its level.
6. Adjust gain, mix, and audition to hear the worldized result against the dry signal.

## Build from source

Requirements: CMake 3.22+, a C++17 toolchain, and JUCE 8.x (added as a git submodule).

```bash
git clone --recurse-submodules https://github.com/themightyzq/worldizer.git
cd worldizer
# If you already cloned without --recurse-submodules:
git submodule update --init --recursive

cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release

# First-time setup only: bake the shipped assets and presets, then rebuild to embed
# them. A fresh checkout builds fine without this step, it just has no libraries yet.
./build/BakeAssets_artefacts/Release/BakeAssets
./build/BakePresets_artefacts/Release/BakePresets
cmake --build build --config Release
```

Built artefacts land in `build/Worldizer_artefacts/Release/`. The build also copies the
VST3 and AU bundles into your user plug-in folders.

The bake step generates the shipped presets, speaker/mic character impulse responses,
and ambient room-tone beds from in-repo definitions and embeds them into the plugin.
They are not committed to the repo, so a fresh checkout needs the bake step once;
rebuilds after that pick them up automatically.

User presets live under your OS user-data directory:
`~/Library/Application Support/ZQ SFX/Worldizer/Presets/` on macOS,
`%APPDATA%\ZQ SFX\Worldizer\Presets\` on Windows,
`~/.config/ZQ SFX/Worldizer/Presets/` on Linux. Drop a `.wzpreset` directory there and
restart the plugin.

Platform requirements:

- macOS 10.15+, Xcode Command Line Tools (builds a Universal Binary: arm64 + x86_64)
- Windows: Visual Studio 2022 (MSVC) build tools
- Linux: GCC/Clang and the JUCE dev packages (ALSA, X11, FreeType, Mesa/GL; see the
  Install Linux dependencies step in `.github/workflows/build.yml`)

Formats: VST3, AU, and Standalone on macOS; VST3 and Standalone on Windows and Linux.
Logic works via AU. Pro Tools is not supported and will not be: it requires AAX, which
is not built.

## Licence

GPL-3.0-or-later. See `LICENSE`. Built with JUCE.

ZQ SFX, https://www.zq-sfx.com, connect@zq-sfx.com.
