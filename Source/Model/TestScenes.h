#pragma once

#include "Scene.h"

namespace Worldizer::TestScenes
{
/** 6 m x 4 m x 3 m room: concrete floor, drywall walls, drywall ceiling.
    Source at centre, mic 2 m away. */
Scene smallConcreteRoom();

/** Long hallway: 20 m x 2 m x 2.5 m, drywall walls/ceiling, wood floor.
    Source at one end, mic 12 m down the hall. */
Scene hallway();

/** Outdoor "forest clearing": no ceiling (rays escape upward), gravel ground,
    a few tall thin boxes approximating trees. Source and mic 5 m apart. */
Scene forestClearing();

/** Large gymnasium: 30 m x 20 m x 8 m, concrete floor, drywall walls/ceiling.
    Source and mic 10 m apart. */
Scene gymnasium();

/** Minimal anechoic scene: a single absorbent ground plane only.
    Used to verify the direct sound is captured cleanly. */
Scene anechoic();

/** Builds a scene by name (matching the factory names above). On an unknown
    name, sets ok=false and returns smallConcreteRoom(). */
Scene byName (const juce::String& name, bool& ok);
} // namespace Worldizer::TestScenes
