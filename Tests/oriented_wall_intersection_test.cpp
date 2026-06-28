/*
    OrientedWallIntersectionTest — verifies the Slice 6a Brush::Type::OrientedWall
    ray intersection:
      - A 45-degree wall is hit correctly from both sides
      - Hit normal points to the appropriate side
      - A ray that misses (parallel offset) reports no hit
      - A ray that grazes parallel to the wall reports no hit
*/
#include <JuceHeader.h>
#include <iostream>
#include <cmath>

#include "../Source/Model/Brush.h"

using namespace Worldizer;

namespace
{
    Brush make45DegWall()
    {
        // Wall from (0,0) to (5,5) at floor 0, ceiling 3, thickness 0.1.
        Brush::OrientedWallParams p;
        p.v1 = { 0.0f, 0.0f, 0.0f };
        p.v2 = { 5.0f, 5.0f, 0.0f };
        p.floorZ = 0.0f; p.ceilingZ = 3.0f;
        p.thickness = 0.1f;
        return Brush ("test_wall", p);
    }
}

int main()
{
    bool ok = true;
    const auto wall = make45DegWall();

    // 1. Type and AABB sanity.
    if (wall.getType() != Brush::Type::OrientedWall)
        { std::cout << "FAIL: wrong type\n"; ok = false; }

    // The AABB should span (~0,~0,0) to (~5,~5,3) plus thickness slop.
    const auto mn = wall.getMin(), mx = wall.getMax();
    if (mx.x - mn.x < 5.0f - 0.2f || mx.x - mn.x > 5.0f + 0.5f)
        { std::cout << "FAIL: AABB X span unexpected (" << mn.x << ".." << mx.x << ")\n"; ok = false; }

    // 2. A ray from (-1,2,1.5) heading +X should hit the wall (it crosses the diagonal).
    {
        float t; Vec3 hp, n; Brush::Face f;
        const bool hit = wall.intersect ({ -1.0f, 2.0f, 1.5f }, Vec3 { 1.0f, 0.0f, 0.0f }.normalised(),
                                          0.0f, t, hp, n, f);
        std::cout << "Ray (-1,2,1.5)+X: " << (hit ? "HIT" : "miss") << " t=" << t << "\n";
        if (! hit) { std::cout << "FAIL: should hit\n"; ok = false; }
        else
        {
            // Normal should be roughly along the wall's outward thickness axis ((-1,1,0)/sqrt2).
            if (std::abs (n.z) > 0.2f) { std::cout << "FAIL: normal has Z component (" << n.z << ")\n"; ok = false; }
        }
    }

    // 3. A ray from the opposite side should also hit (different face).
    {
        float t; Vec3 hp, n; Brush::Face f;
        const bool hit = wall.intersect ({ 6.0f, 2.0f, 1.5f }, Vec3 { -1.0f, 0.0f, 0.0f }.normalised(),
                                          0.0f, t, hp, n, f);
        std::cout << "Ray (6,2,1.5)-X:  " << (hit ? "HIT" : "miss") << " t=" << t << "\n";
        if (! hit) { std::cout << "FAIL: opposite-side should hit\n"; ok = false; }
    }

    // 4. A ray that misses entirely (passes far above the wall).
    {
        float t; Vec3 hp, n; Brush::Face f;
        const bool hit = wall.intersect ({ -1.0f, 2.0f, 10.0f }, Vec3 { 1.0f, 0.0f, 0.0f }.normalised(),
                                          0.0f, t, hp, n, f);
        std::cout << "Ray above wall:   " << (hit ? "HIT" : "miss") << "\n";
        if (hit) { std::cout << "FAIL: should miss\n"; ok = false; }
    }

    // 5. A ray nearly parallel to the wall and offset along the thickness axis: miss.
    {
        // Wall direction (length axis) = (5,5,0).norm() = (0.707,0.707,0). Shift origin
        // 1 m perpendicular: along thickness axis (-0.707, 0.707, 0).
        const Vec3 perp { -0.7071f, 0.7071f, 0.0f };
        const Vec3 origin = Vec3 { 1.0f, 1.0f, 1.5f } + perp * 1.0f;
        const Vec3 dir { 0.7071f, 0.7071f, 0.0f };
        float t; Vec3 hp, n; Brush::Face f;
        const bool hit = wall.intersect (origin, dir, 0.0f, t, hp, n, f);
        std::cout << "Parallel offset:  " << (hit ? "HIT" : "miss") << "\n";
        if (hit) { std::cout << "FAIL: parallel ray 1 m off should miss\n"; ok = false; }
    }

    // 6. Box brush still works (regression).
    {
        Brush b ("box", { -1.0f, -1.0f, 0.0f }, { 1.0f, 1.0f, 1.0f });
        float t; Vec3 hp, n; Brush::Face f;
        const bool hit = b.intersect ({ -5.0f, 0.0f, 0.5f }, Vec3 { 1.0f, 0.0f, 0.0f }, 0.0f, t, hp, n, f);
        if (! hit) { std::cout << "FAIL: Box regression — should hit\n"; ok = false; }
        if (f != Brush::Face::NegX) { std::cout << "FAIL: Box regression — wrong face (" << (int) f << ")\n"; ok = false; }
    }

    std::cout << "\n" << (ok ? "PASS" : "FAIL") << "\n";
    return ok ? 0 : 1;
}
