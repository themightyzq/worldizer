#pragma once

#include <cmath>
#include <algorithm>

namespace Worldizer
{
/**
    Models the perceptual effect of source-to-mic distance.

    The room IR built by IRBuilder is *distance-independent*: its direct sound sits
    at sample 0 at unit amplitude and it carries no propagation delay or 1/r level
    change (see IRBuilder, Slice 4.5). All distance cues are applied afterwards, on
    the WET path only, by this model. It translates a geometric distance into a
    (pre-delay, attenuation) pair:

      - **Pre-delay** — the time of flight (distance / speed of sound). The brain
        reads onset delay as distance; for many sources it is a stronger cue than
        level. Applied as a variable delay line on the wet signal.
      - **Attenuation** — the inverse-distance pressure law (1/r). Applied as a
        smoothed gain on the wet signal.

    Both support a continuous "accuracy <-> musical" blend so SFX work can favour
    impact over metrological correctness:

      - accuracy = 1.0 (full accuracy): true physics — d/c delay, 1/r attenuation.
      - accuracy = 0.0 (full musical):  exaggerated curves tuned for SFX punch
                                        (0.6x delay, steeper 1/r^1.5 level drop).
      - in between: linear interpolation between the two curves.

    A future "Distance Character" UI knob (post-MVP) will expose `accuracy`; for now
    the default (0.4 — mostly musical, a touch of physical grounding) bakes in.

    The dry path is never delayed or attenuated: dry = "the source as emitted, no
    propagation." Worldizing comes from summing the un-propagated dry with the
    propagated (delayed, attenuated, room-convolved) wet.
*/
class DistanceModel
{
public:
    struct Settings
    {
        float speedOfSound     = 343.0f; // m/s at ~20 C
        float accuracy         = 0.4f;   // 0..1, see class doc (default favours musical impact)
        float referenceDistance = 1.0f;  // distance at which attenuation = 1.0 (0 dB)
        float minDistance       = 0.3f;  // floor to prevent gain blow-up at zero distance
        float maxDelaySeconds   = 1.0f;  // cap delay here (~343 m); beyond is silly
        float maxGain           = 2.0f;  // safety ceiling (+6 dB) so very-close placements
                                         // can't drive the wet path into hard clipping.
                                         // Only bites below referenceDistance; at the
                                         // tested >= 1 m range the curves are <= 0 dB.
    };

    DistanceModel() = default;
    explicit DistanceModel (const Settings& s) noexcept : settings (s) {}

    void            setSettings (const Settings& s) noexcept { settings = s; }
    const Settings& getSettings() const noexcept             { return settings; }

    /** Returns the wet pre-delay in samples for a distance (metres) and sample rate.

        Accurate: preDelay = distance / speedOfSound.
        Musical:  preDelay = (distance / speedOfSound) * 0.6  (less "slap").
        Result is lerp(musical, accurate, accuracy), clamped to maxDelaySeconds. */
    float computePreDelaySamples (float distanceMeters, double sampleRate) const noexcept
    {
        const float d = std::max (0.0f, distanceMeters);
        const float physicalSeconds = d / settings.speedOfSound;
        const float musical  = physicalSeconds * 0.6f;
        const float accurate = physicalSeconds;
        float seconds = lerp (musical, accurate, settings.accuracy);
        seconds = std::min (seconds, settings.maxDelaySeconds);
        return seconds * (float) sampleRate;
    }

    /** Returns the linear wet gain (0..maxGain) for a distance (metres).

        At referenceDistance the gain is 1.0 (0 dB). Accurate halves per doubling
        (-6 dB, the 1/r law); musical drops ~9 dB per doubling (1/r^1.5, a
        perceptually-tuned power law). Result is lerp(musical, accurate, accuracy),
        clamped to [0, maxGain]. */
    float computeAttenuationGain (float distanceMeters) const noexcept
    {
        const float r = std::max (distanceMeters, settings.minDistance);
        const float ratio = settings.referenceDistance / r;
        const float accurate = ratio;
        const float musical  = std::pow (ratio, 1.5f);
        float g = lerp (musical, accurate, settings.accuracy);
        return std::min (std::max (0.0f, g), settings.maxGain);
    }

private:
    static float lerp (float a, float b, float t) noexcept { return a + (b - a) * t; }

    Settings settings;
};
} // namespace Worldizer
