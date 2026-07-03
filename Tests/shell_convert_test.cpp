/**
    ShellConvertTest — verifies SectorGeometry::convertRoomShell, the bridge that
    makes shipped presets' walls editable:

      1. A closed brush room converts: sector at the interior bounds with the
         shell's heights and materials; the six shell brushes are removed;
         interior prop brushes survive as fixed obstacles.
      2. Open-air scenes (no ceiling) refuse to convert, unchanged.
      3. Conversion is idempotent (second call is a no-op).
      4. Acoustic sanity: the converted small concrete room's IR is close to the
         brush original (same bounds/materials; oriented walls vs slabs).

    Returns 0 on pass / 1 on fail (CTest).
*/
#include <JuceHeader.h>
#include "../Source/Model/TestScenes.h"
#include "../Source/Model/LibraryScenes.h"
#include "../Source/DSP/RayTracer.h"
#include "../Source/DSP/IRBuilder.h"

using namespace Worldizer;

namespace
{
int failures = 0;

void check (bool condition, const juce::String& what)
{
    if (condition)
        std::cout << "  PASS: " << what << "\n";
    else
    {
        std::cout << "  FAIL: " << what << "\n";
        ++failures;
    }
}

float rmsDbOf (const Scene& scene)
{
    RayTracer tracer;
    RayTracer::Settings rt;
    rt.numRays = 20000; rt.maxBounces = 32; rt.randomSeed = 42;
    const auto result = tracer.trace (scene, rt, 48000);

    IRBuilder builder;
    IRBuilder::Settings irs;
    irs.sampleRate = 48000;
    const auto ir = builder.build (result, irs);

    double sumSq = 0.0;
    for (int i = 0; i < ir.getNumSamples(); ++i)
        sumSq += (double) ir.getSample (0, i) * (double) ir.getSample (0, i);
    return juce::Decibels::gainToDecibels (
        (float) std::sqrt (sumSq / (double) juce::jmax (1, ir.getNumSamples())), -120.0f);
}
} // namespace

int main()
{
    std::cout << "=== Closed room converts to an editable sector ===\n";
    {
        auto scene = TestScenes::smallConcreteRoom();          // 6x4x3 m shell, no props
        const auto brushesBefore = scene.getNumBrushes();
        check (brushesBefore == 6, "small concrete room starts as the 6-brush shell");

        check (SectorGeometry::convertRoomShell (scene), "conversion succeeds");
        check (scene.getNumBrushes() == 0, "all 6 shell brushes removed");
        check (! scene.getSectorGeometry().isEmpty(), "sector created");

        const auto& sector = scene.getSectorGeometry().sectors[0];
        check (sector.vertices.size() == 4 && sector.lineDefs.size() == 4, "4 vertices / 4 walls");
        const auto aabb = sector.getAABB();
        check (std::abs (aabb.minX - (-3.0f)) < 1.0e-4f && std::abs (aabb.maxX - 3.0f) < 1.0e-4f
            && std::abs (aabb.minY - (-2.0f)) < 1.0e-4f && std::abs (aabb.maxY - 2.0f) < 1.0e-4f,
               "sector spans the interior bounds (6 x 4 m)");
        check (std::abs (sector.floorHeight - 0.0f) < 1.0e-4f
            && std::abs (sector.ceilingHeight - 3.0f) < 1.0e-4f, "floor 0 m / ceiling 3 m");
        check (sector.floorMaterial == "concrete", "floor material carried over (concrete)");
        check (sector.ceilingMaterial == "drywall", "ceiling material carried over (drywall)");
        bool wallsDrywall = true;
        for (const auto& ld : sector.lineDefs)
            wallsDrywall = wallsDrywall && ld.frontMaterial == "drywall";
        check (wallsDrywall, "wall materials carried over (drywall)");
        check (sector.isClosed() && sector.isCounterClockwise(), "sector closed, CCW winding");

        check (! SectorGeometry::convertRoomShell (scene), "second conversion is a no-op");
    }

    std::cout << "=== Interior props survive as fixed obstacles ===\n";
    {
        auto scene = LibraryScenes::bedroom();                 // shell + curtain wall + bed
        const auto before = scene.getNumBrushes();
        check (SectorGeometry::convertRoomShell (scene), "bedroom converts");
        check (scene.getNumBrushes() == before - 6,
               "only the shell removed (" + juce::String ((int) scene.getNumBrushes()) + " props remain)");
    }

    std::cout << "=== Open-air scenes refuse to convert ===\n";
    {
        for (auto make : { &TestScenes::forestClearing, &TestScenes::anechoic })
        {
            auto scene = make();
            const auto before = scene.getNumBrushes();
            check (! SectorGeometry::convertRoomShell (scene) && scene.getNumBrushes() == before,
                   "open scene unchanged (" + juce::String ((int) before) + " brushes)");
        }
        bool ok = false;
        auto courtyard = LibraryScenes::byName ("courtyard", ok);
        const auto before = courtyard.getNumBrushes();
        check (ok && ! SectorGeometry::convertRoomShell (courtyard) && courtyard.getNumBrushes() == before,
               "courtyard (no ceiling) unchanged");
    }

    std::cout << "=== Acoustic sanity: converted room ~= brush original ===\n";
    {
        auto original  = TestScenes::smallConcreteRoom();
        auto converted = TestScenes::smallConcreteRoom();
        SectorGeometry::convertRoomShell (converted);

        const float dbOriginal  = rmsDbOf (original);
        const float dbConverted = rmsDbOf (converted);
        const float diff = std::abs (dbOriginal - dbConverted);
        check (diff < 6.0f, "IR RMS within 6 dB of the brush original (diff "
                            + juce::String (diff, 2) + " dB)");
    }

    std::cout << (failures == 0 ? "\nALL PASS\n" : "\n" + juce::String (failures) + " FAILURES\n");
    return failures == 0 ? 0 : 1;
}
