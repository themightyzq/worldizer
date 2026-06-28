/*
    SectorCompilerTest — verifies SectorGeometry::compileToBrushes() produces the
    expected brush layout (Slice 6a):
      - Rectangular sector (4 vertices) -> 1 floor + 1 ceiling + 4 walls = 6 brushes
      - L-shaped sector  (6 vertices) -> 1 floor + 1 ceiling + 6 walls = 8 brushes
      - Materials propagate from sector/linedef to the brush face materials
      - A rectangular compiled sector traced acoustically resembles a hand-built
        brush-based equivalent (within reverberation envelope tolerance)
*/
#include <JuceHeader.h>
#include <iostream>
#include <cmath>

#include "../Source/Model/SectorGeometry.h"
#include "../Source/Model/Scene.h"
#include "../Source/Model/Material.h"
#include "../Source/Model/TestScenes.h"
#include "../Source/DSP/RayTracer.h"
#include "../Source/DSP/IRBuilder.h"

using namespace Worldizer;

namespace
{
    Sector makeRect (float w, float d, float floorH = 0.0f, float ceilH = 3.0f)
    {
        Sector s;
        s.vertices = { { 0.0f, 0.0f }, { w, 0.0f }, { w, d }, { 0.0f, d } };
        for (int i = 0; i < 4; ++i) s.lineDefs.emplace_back (i, (i + 1) % 4, "drywall");
        s.floorHeight   = floorH;
        s.ceilingHeight = ceilH;
        s.floorMaterial   = "wood_floor";
        s.ceilingMaterial = "drywall";
        return s;
    }

    Sector makeL()
    {
        // L-shape: outer rectangle with a notch cut out of one corner.
        //   (0,0) -> (6,0) -> (6,3) -> (3,3) -> (3,6) -> (0,6) -> close
        Sector s;
        s.vertices = { { 0.0f, 0.0f }, { 6.0f, 0.0f }, { 6.0f, 3.0f },
                       { 3.0f, 3.0f }, { 3.0f, 6.0f }, { 0.0f, 6.0f } };
        for (int i = 0; i < 6; ++i) s.lineDefs.emplace_back (i, (i + 1) % 6, "drywall");
        if (! s.isCounterClockwise()) s.reverseWinding();
        s.floorHeight = 0.0f; s.ceilingHeight = 3.0f;
        return s;
    }
}

int main()
{
    bool ok = true;

    // --- 1. Rectangular sector compiles to 6 brushes ---
    {
        SectorGeometry sg;
        sg.sectors.push_back (makeRect (6.0f, 4.0f));
        const auto brushes = sg.compileToBrushes();
        std::cout << "Rectangular sector -> " << brushes.size() << " brushes\n";
        if (brushes.size() != 6) { std::cout << "FAIL: expected 6 brushes\n"; ok = false; }

        // 2 of those should be boxes (floor + ceiling); 4 should be OrientedWalls.
        int boxes = 0, walls = 0;
        for (const auto& b : brushes)
            (b.getType() == Brush::Type::OrientedWall ? walls : boxes) += 1;
        if (boxes != 2 || walls != 4)
        {
            std::cout << "FAIL: expected 2 boxes + 4 oriented walls, got " << boxes << " + " << walls << "\n";
            ok = false;
        }
    }

    // --- 2. L-shaped sector compiles to 8 brushes ---
    {
        SectorGeometry sg;
        sg.sectors.push_back (makeL());
        const auto brushes = sg.compileToBrushes();
        std::cout << "L-shaped sector  -> " << brushes.size() << " brushes\n";
        if (brushes.size() != 8) { std::cout << "FAIL: expected 8 brushes\n"; ok = false; }
    }

    // --- 3. Floor/ceiling Z positions and materials propagate ---
    {
        SectorGeometry sg;
        sg.sectors.push_back (makeRect (5.0f, 4.0f, 0.0f, 3.0f));
        const auto brushes = sg.compileToBrushes();

        const auto* floor   = &brushes[0];
        const auto* ceiling = &brushes[1];
        const bool floorAtZero    = std::abs (floor->getMax().z   - 0.0f) < 1.0e-3f;
        const bool ceilingAtThree = std::abs (ceiling->getMin().z - 3.0f) < 1.0e-3f;
        if (! floorAtZero)    { std::cout << "FAIL: floor top Z != 0\n"; ok = false; }
        if (! ceilingAtThree) { std::cout << "FAIL: ceiling bottom Z != 3\n"; ok = false; }

        // Floor material name round-trips into Material::getName().
        if (floor->getFaceMaterial (Brush::Face::PosZ).getName() != "wood_floor")
        {
            std::cout << "FAIL: floor material not propagated\n"; ok = false;
        }
    }

    // --- 4. End-to-end: rectangular sector + Test scene equivalent produce similar IRs ---
    // smallConcreteRoom is a 6 m x 4 m x 3 m brush room with concrete floor + drywall.
    // We model the same room as a compiled sector (concrete floor + drywall walls/ceiling)
    // and compare gross IR shape (length, RMS). They won't be byte-identical (wall thickness
    // and floor approximation differ), but they should sit within a reasonable envelope.
    {
        Scene brushScene = TestScenes::smallConcreteRoom();

        Scene sectorScene;
        Sector s;
        s.vertices = { { -3.0f, -2.0f }, { 3.0f, -2.0f }, { 3.0f, 2.0f }, { -3.0f, 2.0f } };
        for (int i = 0; i < 4; ++i) s.lineDefs.emplace_back (i, (i + 1) % 4, "drywall");
        if (! s.isCounterClockwise()) s.reverseWinding();
        s.floorMaterial   = "concrete";
        s.ceilingMaterial = "drywall";
        s.floorHeight = 0.0f; s.ceilingHeight = 3.0f;
        sectorScene.getSectorGeometry().sectors.push_back (s);
        sectorScene.getSource().setPosition ({ 0.0f, 0.0f, 1.5f });
        sectorScene.getMicArray().getMic (0).setPosition ({ 2.0f, 0.0f, 1.5f });

        RayTracer tracer;
        RayTracer::Settings rt; rt.numRays = 30000; rt.randomSeed = 12345;
        const auto brushResult  = tracer.trace (brushScene,  rt, 48000);
        const auto sectorResult = tracer.trace (sectorScene, rt, 48000);

        IRBuilder ib; IRBuilder::Settings irs; irs.sampleRate = 48000;
        const auto brushIR  = ib.build (brushResult,  irs);
        const auto sectorIR = ib.build (sectorResult, irs);

        auto rmsDb = [] (const juce::AudioBuffer<float>& b)
        {
            const auto* d = b.getReadPointer (0);
            double ss = 0.0;
            for (int i = 0; i < b.getNumSamples(); ++i) ss += (double) d[i] * d[i];
            return 20.0 * std::log10 (std::sqrt (ss / juce::jmax (1, b.getNumSamples())) + 1e-20);
        };

        const double brushRms  = rmsDb (brushIR);
        const double sectorRms = rmsDb (sectorIR);
        std::cout << "Brush IR rms:  " << juce::String (brushRms,  1).toRawUTF8() << " dB\n";
        std::cout << "Sector IR rms: " << juce::String (sectorRms, 1).toRawUTF8() << " dB\n";
        const bool similar = std::abs (brushRms - sectorRms) < 6.0; // within 6 dB envelope
        if (! similar) { std::cout << "FAIL: brush vs sector room RMS differ by > 6 dB\n"; ok = false; }
    }

    std::cout << "\n" << (ok ? "PASS" : "FAIL") << "\n";
    return ok ? 0 : 1;
}
