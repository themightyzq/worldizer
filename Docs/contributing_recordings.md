# Contributing Recordings

> **Placeholder.** This document describes the recording contribution workflow at a high level. It will be expanded with detailed, step-by-step guidance (and reference recordings) as the library system comes online in later slices.

Worldizer's character comes from real recordings. There are three kinds of audio assets you can contribute, plus complete presets that combine geometry with a baked IR.

## What to Record

### Speaker (reproducer) IRs — `Resources/Speakers/`

The impulse response of a loudspeaker or other reproducer: studio monitors, guitar cabs, hi-fi speakers, intercoms, megaphones, toys, telephone earpieces — anything a sound can be "played through." This captures the reproducer's frequency response and resonances.

### Microphone IRs — `Resources/Mics/`

The character of a microphone: dynamics, condensers, ribbons, contact mics, lavaliers, lo-fi capsules. This captures the mic's frequency and transient signature.

### Room tone beds — `Resources/RoomTones/`

Steady-state ambient recordings of real spaces: HVAC hum, refrigerator drone, distant traffic, forest air, the quiet of a stairwell. These are layered under the worldized signal to seat it in the space.

### Presets — `Resources/Presets/`

A complete `.wzpreset` bundle (geometry + baked IR + thumbnail + metadata). See the format in [`architecture.md`](architecture.md) §4.

## Recommended Technique

- **Speaker / mic IRs:** use the **sine sweep + deconvolution** method. Play an exponential sine sweep through the reproducer (for speaker IRs) or into the mic under controlled conditions, record, and deconvolve against the inverse sweep to recover the impulse response. Trim silence, normalize sensibly, and remove any DC offset.
- **Room tones:** capture a long, uninterrupted recording (30s+) of the space at rest, free of transient events, so it can be looped seamlessly.

## Required Format

- **Sample rate:** 48 kHz
- **Bit depth:** 24-bit (PCM WAV) for source recordings
- **Channels:** mono for IRs unless the capture is intentionally stereo; stereo for room tones
- **File type:** `.wav`
- Keep files trimmed and reasonably sized; document any processing you applied.

## Submission Process

1. Open a **Recording contribution** issue describing what you recorded and the gear/conditions.
2. Submit a pull request adding the WAV file(s) to the appropriate `Resources/` subfolder, each accompanied by a **metadata JSON sidecar** (name, type, equipment, conditions, author). The exact sidecar schema will be finalized with the library system.
3. Expect review for format, level, and labeling consistency.

## License Assignment

By contributing recordings you agree that they will be distributed under the project's **GPL-3.0** license as part of Worldizer. Only submit recordings you have the right to license this way. Confirm this in your contribution issue/PR.
