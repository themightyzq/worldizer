#pragma once

#include <JuceHeader.h>
#include <vector>
#include <utility>
#include "../Shared/Vec3.h"
#include "Brush.h"
#include "SourceNode.h"
#include "MicNode.h"

namespace Worldizer
{
/**
    Top-level container for a worldizing scene: brushes plus a source and a mic.
    Slice 1 uses hardcoded scenes (see TestScenes); JSON (de)serialisation and
    multi-mic arrays arrive in later slices.
*/
class Scene
{
public:
    Scene() = default;

    // === Brushes ===
    void addBrush (Brush brush);
    const std::vector<Brush>& getBrushes() const noexcept   { return brushes; }
    size_t getNumBrushes() const noexcept                   { return brushes.size(); }

    // === Source & mic ===
    SourceNode& getSource() noexcept                        { return source; }
    const SourceNode& getSource() const noexcept            { return source; }

    MicNode& getMic() noexcept                              { return mic; }
    const MicNode& getMic() const noexcept                  { return mic; }

    // === Bounds ===
    /** Returns the axis-aligned bounding box (min, max) of all brushes. */
    std::pair<Vec3, Vec3> getBounds() const noexcept;

    // === Ray testing against all brushes ===
    /** Finds the closest brush hit for a ray. Returns true if any hit. Outputs the
        hit brush index, distance, hit point, outward normal, and face. */
    bool intersect (Vec3 origin,
                    Vec3 direction,
                    float tMin,
                    int& brushIndex,
                    float& t,
                    Vec3& hitPoint,
                    Vec3& normal,
                    Brush::Face& face) const noexcept;

private:
    std::vector<Brush> brushes;
    SourceNode source;
    MicNode mic;
};
} // namespace Worldizer
