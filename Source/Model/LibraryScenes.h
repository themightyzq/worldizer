#pragma once

#include "Scene.h"

namespace Worldizer::LibraryScenes
{
/**
    The shipped preset library scenes (Slice 9) — 20 spaces across the five
    categories (Indoor / Outdoor / Vehicles & Devices / Cinematic / Experimental),
    on top of the five Test scenes in TestScenes.h.

    All are brush-built (axis-aligned boxes) with materials from MaterialResolver.
    Geometry is authored for worldizing believability, not architectural accuracy:
    what matters is the reflection pattern, decay, and spectral fingerprint of
    each space.

    byName() builds a scene by its factory name; unknown names set ok=false.
*/

// --- Indoor ---
Scene stairwell();       // tall concrete shaft: stacked slap + long vertical decay
Scene parkingGarage();   // vast flat slab, low ceiling, columns: boomy smear
Scene tiledBathroom();   // tiny tile box: bright splashy ring
Scene bedroom();         // carpet/curtain domestic room: short soft decay
Scene cathedral();       // huge stone nave: seconds of holy wash
Scene warehouse();       // big metal/concrete hall with crate stacks
Scene closet();          // clothes-lined booth: near-dead, intimate
Scene kitchen();         // tile + counters: clattery small room

// --- Outdoor ---
Scene backAlley();       // two tall facades, narrow gap: urban slap
Scene courtyard();       // enclosed walls, open sky: contained but airy
Scene canyon();          // two huge rock faces far apart: distinct echoes
Scene streetTunnel();    // concrete tube open both ends: directional boom

// --- Vehicles & Devices ---
Scene carInterior();     // tiny padded/glass capsule: boxy closeness
Scene cargoVan();        // empty metal box on wheels: ringy clatter

// --- Cinematic ---
Scene bunker();          // low concrete crawl: oppressive close slap
Scene cave();            // irregular rock chamber: diffuse rumble
Scene grainSilo();       // tall narrow cylinder-ish: singing vertical ring

// --- Experimental ---
Scene infiniteCorridor();// absurdly long glass hall: endless flutter
Scene resonantTank();    // all-metal cube: brutal metallic ring
Scene deadRoom();        // curtain everywhere: drier than anechoic feels wrong

/** Builds a scene by factory name; unknown names set ok=false and return bedroom(). */
Scene byName (const juce::String& name, bool& ok);
} // namespace Worldizer::LibraryScenes
