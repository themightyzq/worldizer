#include "MicArray.h"
#include <cmath>

namespace Worldizer
{
namespace
{
    /** Rotate a vector about the +Z axis by `angleRad` (top-down: +angle = CCW). */
    Vec3 rotateZ (Vec3 v, float angleRad) noexcept
    {
        const float c = std::cos (angleRad), s = std::sin (angleRad);
        return { v.x * c - v.y * s, v.x * s + v.y * c, v.z };
    }
}

MicArray::MicArray (Configuration config)
{
    setConfiguration (config);
}

Vec3 MicArray::horizontalUnit (Vec3 v) noexcept
{
    Vec3 h { v.x, v.y, 0.0f };
    if (h.lengthSquared() < 1.0e-12f)
        return { -1.0f, 0.0f, 0.0f };  // pure-vertical input: fall back to facing -X
    return h.normalised();
}

int MicArray::getNumMics() const noexcept
{
    return configuration == Configuration::Single ? 1 : 2;
}

void MicArray::setConfiguration (Configuration c)
{
    const auto old = configuration;
    configuration = c;

    switch (c)
    {
        case Configuration::Single:
            // Mic 0 stays as-is; mic 1 is simply unused. No "are you sure" needed.
            break;

        case Configuration::StereoXY:
        {
            // Mic 1 takes mic 0's position/pattern/radius; facing comes from mic 0's
            // orientation, then both capsules are splayed about it (coincident).
            xyFacing = horizontalUnit (mics[0].getOrientation());
            mics[1].setPosition (mics[0].getPosition());
            mics[1].setPattern  (mics[0].getPattern());
            mics[1].setRadius   (mics[0].getRadius());
            recomputeXYOrientations();
            break;
        }

        case Configuration::SpacedPair:
        {
            if (old == Configuration::StereoXY)
            {
                // Both capsules start coincident at the XY array position; the user
                // drags them apart. Un-splay their orientations to the array facing.
                mics[0].setOrientation (xyFacing);
                mics[1].setOrientation (xyFacing);
            }
            else // from Single
            {
                // Spawn the second mic 0.5 m to the +X side — close enough to feel like
                // an array, far enough to give immediate stereo width.
                mics[1].setPosition  (mics[0].getPosition() + Vec3 { 0.5f, 0.0f, 0.0f });
                mics[1].setOrientation (mics[0].getOrientation());
                mics[1].setPattern   (mics[0].getPattern());
                mics[1].setRadius    (mics[0].getRadius());
            }
            break;
        }
    }
}

void MicArray::recomputeXYOrientations()
{
    // Coincident position; orientations splayed +/- half the angle about the facing.
    // Mic 0 = left capsule (CCW), mic 1 = right capsule (CW) — see header convention.
    const float half = juce::degreesToRadians (xyAngleDegrees * 0.5f);
    mics[1].setPosition (mics[0].getPosition());
    mics[0].setOrientation (rotateZ (xyFacing,  half));
    mics[1].setOrientation (rotateZ (xyFacing, -half));
}

void MicArray::setXYPosition (Vec3 pos)
{
    mics[0].setPosition (pos);
    mics[1].setPosition (pos);
}

void MicArray::setXYOrientation (Vec3 dir)
{
    xyFacing = horizontalUnit (dir);
    recomputeXYOrientations();
}

Vec3 MicArray::getCenterPosition() const noexcept
{
    if (configuration == Configuration::SpacedPair)
        return (mics[0].getPosition() + mics[1].getPosition()) * 0.5f;
    return mics[0].getPosition();  // Single / StereoXY (coincident)
}

void MicArray::setAllPatterns (MicPattern pattern)
{
    mics[0].setPattern (pattern);
    mics[1].setPattern (pattern);
}
} // namespace Worldizer
