# Contributing to Worldizer

Thanks for your interest in Worldizer. This project is open-source (GPL-3.0) and welcomes contributions of code, documentation, bug reports, ideas, and recordings.

## Code of Conduct

Be kind, be constructive, assume good faith. Worldizer is built by and for sound designers — critique the work, not the person. Harassment of any kind isn't welcome here. If something feels off, open an issue or reach out to the maintainer.

## Reporting Bugs

Open an issue using the **Bug report** template. The more of this you can include, the faster it gets fixed:

- OS and version, DAW/host and version, Worldizer version
- Exact steps to reproduce
- What you expected vs. what actually happened
- An audio file, screenshot, or short screen recording if relevant

## Suggesting Features

Open an issue using the **Feature request** template. Describe the problem you're trying to solve first, then the solution you have in mind. Note that Worldizer has a deliberately narrow scope — see "What This Plugin Is NOT" in [`CLAUDE.md`](CLAUDE.md) and the "Never ships" list in [`TODO.md`](TODO.md) before proposing something.

## Contributing Recordings

Recordings are the heart of Worldizer's library: **speaker IRs**, **microphone IRs**, and **room tone beds**, plus complete **presets**. If you have access to interesting reproducers, microphones, or spaces, your recordings can ship with the plugin.

The full process — what to record, recommended technique, required format, and license assignment — is documented in [`Docs/contributing_recordings.md`](Docs/contributing_recordings.md). Use the **Recording contribution** issue template to propose a contribution before submitting large audio files.

## Submitting Code Changes

1. **Fork** the repository and create a branch off `main` (e.g. `slice-2-convolution` or `fix/preset-load-crash`).
2. Make your change. Keep it focused — one logical change per pull request.
3. Open a **pull request** using the PR template. Fill in what changed, why, and how you tested it.
4. Expect review feedback. Worldizer values [brutally honest, constructive review](CLAUDE.md); it's how the quality stays high.

## Development Setup

Follow the **Quick Start** in [`README.md`](README.md): clone with submodules, then `cmake -B build && cmake --build build`. The convenience scripts in [`Scripts/`](Scripts/) wrap the common build and verification steps.

## Code Style

Worldizer follows the conventions documented in the project's JUCE best-practices guides:

- **Build & DSP:** `JUCE_VST3_BEST_PRACTICES.md` — CMake configuration, plugin metadata, compile definitions, bus layouts, real-time safety (no allocations / locks / file I/O on the audio thread), code signing and notarization.
- **UI/UX:** `JUCE_VST3_UI_UX_BEST_PRACTICES.md` — layout, the semantic color system, knob sizing, tooltips, parameter formatting.

Match the surrounding code: brace style, naming, and comment density already in the file you're editing. When in doubt, simpler is correct, and the architecture in [`Docs/architecture.md`](Docs/architecture.md) is the source of truth — if a decision there seems wrong, flag it in an issue rather than quietly diverging.
