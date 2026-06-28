#pragma once

#include <JuceHeader.h>
#include <array>
#include <vector>
#include "RayTracer.h"
#include "../Shared/Constants.h"

namespace Worldizer
{
/**
    Builds a time-domain impulse response from ray-tracer output. The IR has one
    channel per mic in the trace result: 1 channel for a single mic, 2 channels for
    a stereo XY / spaced pair (channel 0 = mic 0, channel 1 = mic 1).

    Reconstruction (standard auralization method): for each octave band, the per-bin
    energy histogram is smoothed into an energy-vs-time envelope; independent white
    noise is band-pass filtered to that octave, normalised to unit RMS, then
    amplitude-modulated by sqrt(energy density). Summing the bands gives a dense,
    spectrally-shaped decay where HF dies faster than LF. Air absorption is folded
    into each band's energy.

    Slice 4.5 — the IR is a *timing- and level-normalised room response*:
      - The direct sound is an impulse near sample 0; its amplitude keeps the
        1/distance scaling RELATIVE to the reflections (the direct-to-reverberant
        cue, distance-dependent). Peak-normalisation afterwards removes absolute
        level (DistanceModel re-applies it), leaving only that ratio.
      - Reflection times are stored relative to the direct, so propagation delay is
        out of the IR (DistanceModel re-applies it as wet pre-delay). Air absorption
        still uses absolute path lengths (farther paths stay darker).
      - A statistical late tail is synthesised beyond the ray-traced echogram so it
        decays smoothly to silence rather than truncating at the trace limit.

    Slice 5 — multi-channel:
      - All channels share ONE time reference = the EARLIEST direct arrival across
        the mics. Each channel places its own direct at (its arrival - reference),
        so a coincident XY pair has no inter-channel time difference (the difference
        is the per-capsule reception gain) while a spaced pair gets a real Haas/ITD
        cue (the two directs land a few samples apart).
      - Each channel's band-noise carrier and late-tail noise use a DECORRELATED RNG
        seed, so the two channels' diffuse fields / tails are independent — genuine
        stereo width rather than a mono-summing copy.
      - Normalisation is SHARED: the global peak across all channels scales every
        channel by one factor, preserving inter-channel level differences.

    Distance cues (time-of-flight pre-delay, inverse-distance level) are applied on
    the wet path at runtime by DistanceModel, NOT baked into the IR.
*/
class IRBuilder
{
public:
    struct Settings
    {
        int   sampleRate = 48000;
        int   maxLengthSamples = kMaxIRLengthSeconds * 48000;
        bool  applyAirAbsorption = true;
        float temperatureCelsius = 20.0f;
        float relativeHumidity = 50.0f;
        bool  includeDirect = true;
        float directGainCompensation = 1.0f;
        float envelopeMs = 6.0f; // energy-envelope smoothing window (diffuse-field time resolution)

        // Multiplies the growth rate + cap of the time-growing envelope-smoothing
        // window. 1.0 is tuned for an omnidirectional mic's hit density. A DIRECTIONAL
        // mic (shotgun) weights most reflections down toward the rear-rejection floor,
        // so its histogram is effectively sparse — the same hit-starvation that pumps
        // the late tail. The render thread raises this for directional mics so the
        // sparser envelope is smoothed enough to stay monotonic. Left at 1.0 for omni,
        // so omni output is bit-identical to Slice 4.5.
        float envelopeSmoothingScale = 1.0f;

        // === Statistical late-tail synthesis (Slice 4.5) ===
        bool  synthesizeLateTail = true;  // false => IR ends at the ray-traced echogram (raw trace)
        float tailExtensionSeconds = 4.0f; // max synthesised extension beyond the trace cutoff
        float crossfadeSeconds = 0.2f;     // ray-traced -> synthesised crossfade window
        float finalFadeSeconds = 0.05f;    // last-N-ms linear fade guaranteeing clean silence
    };

    IRBuilder() = default;

    /** Builds a 1- or 2-channel IR from a (possibly multi-mic) ray-tracer result. */
    juce::AudioBuffer<float> build (const RayTracer::Result& traceResult,
                                    const Settings& settings) const;

private:
    struct ChannelBuild
    {
        std::vector<float> ir;          // relative-time IR (no trim/fade/normalise yet)
        bool               synthesized = false;
    };

    /** Builds one channel's raw IR (direct + reflections + optional synthesised
        tail) in output/relative time, with output sample 0 == absolute time
        refOffset/sr. Distinct noise/tail seeds decorrelate channels. */
    ChannelBuild buildChannel (const std::array<std::vector<float>, Material::kNumBands>& histogram,
                               const RayTracer::Result::DirectInfo& direct,
                               int refOffset, int numBins, int numRays, float micRadius,
                               const Settings& settings, int sr,
                               int noiseSeed, int tailSeed) const;
};
} // namespace Worldizer
