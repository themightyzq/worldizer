#pragma once

#include <JuceHeader.h>
#include <array>
#include "../Shared/Vec3.h"
#include "MicNode.h"

namespace Worldizer
{
/**
    A configuration of one or two microphones in the scene.

    MVP configurations:
      - Single     : one MicNode.
      - StereoXY   : two coincident MicNodes (same position), splayed by an angle
                     about a common "array facing" direction.
      - SpacedPair : two independently-positioned MicNodes.

    Channel convention (for the stereo IR built downstream): channel 0 = mic 0,
    channel 1 = mic 1. For StereoXY, mic 0 is the LEFT capsule (orientation splayed
    counter-clockwise from the array facing in the top-down view), mic 1 the right.
    For SpacedPair, mic 0 is "L" and mic 1 "R" by index (the new second mic spawns to
    the +X side, so it starts on the right). Index-stable L/R avoids channel swaps
    mid-drag — a deviation from the prompt's "lower-X = channel 0", which the prompt
    sanctions ("or some consistent rule").

    Future: ORTF, M/S, multi-mic (>2).
*/
class MicArray
{
public:
    enum class Configuration
    {
        Single,
        StereoXY,
        SpacedPair
        // Future: ORTF, MidSide, Multi (>2)
    };

    MicArray() = default;
    explicit MicArray (Configuration config);

    Configuration getConfiguration() const noexcept     { return configuration; }
    /** Switches configuration, re-initialising the mics for the new layout
        (see the .cpp for the per-transition rules). */
    void setConfiguration (Configuration c);

    int getNumMics() const noexcept;  // 1 for Single, 2 for StereoXY / SpacedPair

    // === Individual mics (index must be < getNumMics()) ===
    MicNode&       getMic (int index)       { jassert (index >= 0 && index < getNumMics()); return mics[(size_t) juce::jlimit (0, 1, index)]; }
    const MicNode& getMic (int index) const { jassert (index >= 0 && index < getNumMics()); return mics[(size_t) juce::jlimit (0, 1, index)]; }

    /** The "primary" mic (mic 0). For distance purposes prefer getCenterPosition(). */
    MicNode&       getPrimary()       { return mics[0]; }
    const MicNode& getPrimary() const { return mics[0]; }

    // === StereoXY-specific ===

    /** Angle between the two XY capsules in degrees (default 90, range 30..180).
        Ignored for other configurations. */
    float getXYAngleDegrees() const noexcept    { return xyAngleDegrees; }
    void setXYAngleDegrees (float deg)          { xyAngleDegrees = juce::jlimit (30.0f, 180.0f, deg); recomputeXYOrientations(); }

    /** The XY array's shared capsule position. */
    Vec3 getXYPosition() const noexcept         { return mics[0].getPosition(); }
    void setXYPosition (Vec3 pos);

    /** The XY array's facing direction (the bisector of the two capsule axes). */
    Vec3 getXYOrientation() const noexcept      { return xyFacing; }
    void setXYOrientation (Vec3 dir);

    // === Centroid / distance reference ===

    /** Position used for the wet-path distance model.
        Single / StereoXY: the mic (array) position. SpacedPair: the midpoint. */
    Vec3 getCenterPosition() const noexcept;

    // === Pattern broadcast ===

    /** Set all mics in the array to the same pattern (MVP arrays are uniform). */
    void setAllPatterns (MicPattern pattern);
    /** The array pattern (taken from mic 0; arrays are uniform in MVP). */
    MicPattern getPattern() const noexcept      { return mics[0].getPattern(); }

private:
    Configuration configuration = Configuration::Single;
    std::array<MicNode, 2> mics;     // always allocated; only first getNumMics() used
    float xyAngleDegrees = 90.0f;
    Vec3  xyFacing { -1.0f, 0.0f, 0.0f };  // XY array bisector (toward a source at origin)

    void recomputeXYOrientations();
    static Vec3 horizontalUnit (Vec3 v) noexcept;  // project to XY plane + normalise
};
} // namespace Worldizer
