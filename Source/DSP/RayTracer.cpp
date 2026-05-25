#include "RayTracer.h"
#include <cmath>

namespace Worldizer
{
namespace
{
    constexpr float kRayEpsilon   = 1.0e-4f;  // offset after a hit to avoid self-intersection
    constexpr float kEnergyFloor  = 1.0e-6f;  // terminate a ray below this energy

    /** Cosine-weighted hemisphere sample about a unit normal n (for diffuse bounces). */
    Vec3 sampleCosineHemisphere (Vec3 n, juce::Random& random) noexcept
    {
        const float u1 = random.nextFloat();
        const float u2 = random.nextFloat();
        const float r  = std::sqrt (u1);
        const float theta = juce::MathConstants<float>::twoPi * u2;

        const float x = r * std::cos (theta);
        const float y = r * std::sin (theta);
        const float z = std::sqrt (juce::jmax (0.0f, 1.0f - u1));

        // Orthonormal basis around n.
        const Vec3 w = n;
        const Vec3 a = (std::abs (w.x) > 0.9f) ? Vec3 { 0.0f, 1.0f, 0.0f }
                                               : Vec3 { 1.0f, 0.0f, 0.0f };
        const Vec3 u = cross (a, w).normalised();
        const Vec3 v = cross (w, u);

        return (u * x + v * y + w * z).normalised();
    }
}

RayTracer::Result RayTracer::trace (const Scene& scene, const Settings& settings, int sampleRate) const
{
    Result result;
    result.sampleRate = sampleRate;
    result.numBins    = (int) std::ceil (settings.maxTraceTimeSeconds * (float) sampleRate);
    result.numRays    = settings.numRays;
    result.micRadius  = scene.getMic().getRadius();

    for (auto& band : result.histogram)
        band.assign ((size_t) result.numBins, 0.0f);

    // === Direct sound (deterministic) ===
    const Vec3 sourcePos = scene.getSource().getPosition();
    const Vec3 micPos    = scene.getMic().getPosition();
    const Vec3 toMic     = micPos - sourcePos;
    const float directDist = toMic.length();

    result.directDistance    = directDist;
    result.directArrivalTime = directDist / settings.speedOfSound;

    if (directDist > 1.0e-6f)
    {
        const Vec3 dir = toMic / directDist;
        int bi; float t; Vec3 hp, n; Brush::Face face;
        const bool occluded = scene.intersect (sourcePos, dir, kRayEpsilon, bi, t, hp, n, face)
                                && t < directDist - scene.getMic().getRadius();
        result.directVisible = ! occluded;
    }
    else
    {
        result.directVisible = true;
    }

    // === Stochastic reflections ===
    juce::Random random ((juce::int64) settings.randomSeed);

    for (int i = 0; i < settings.numRays; ++i)
    {
        const Vec3 dir = scene.getSource().sampleEmissionDirection (random);
        traceRay (scene, sourcePos, dir, settings, result, random);
    }

    return result;
}

void RayTracer::traceRay (const Scene& scene,
                          Vec3 origin,
                          Vec3 direction,
                          const Settings& settings,
                          Result& result,
                          juce::Random& random) const
{
    const float c       = settings.speedOfSound;
    const float maxDist = settings.maxTraceTimeSeconds * c;
    const int   sr      = result.sampleRate;
    const MicNode& mic  = scene.getMic();

    std::array<float, Material::kNumBands> energy;
    energy.fill (1.0f);

    Vec3 o = origin;
    Vec3 d = direction.normalised();
    float accumDist = 0.0f;

    for (int bounce = 0; bounce <= settings.maxBounces; ++bounce)
    {
        int bi; float tSurf; Vec3 hp, n; Brush::Face face;
        const bool hitSurface = scene.intersect (o, d, kRayEpsilon, bi, tSurf, hp, n, face);

        const float remaining = maxDist - accumDist;
        if (remaining <= 0.0f)
            break;

        const float segMax = hitSurface ? juce::jmin (tSurf, remaining) : remaining;

        // Deposit a mic hit on this segment — but only from the first reflection onward
        // (bounce 0 is the unreflected, direct path, handled separately in trace()).
        if (bounce >= 1)
        {
            float micT;
            if (mic.intersectRay (o, d, segMax, micT))
            {
                const float arrivalTime = (accumDist + micT) / c;
                const int   bin = (int) std::lround (arrivalTime * (float) sr);

                if (bin >= 0 && bin < result.numBins)
                {
                    for (int b = 0; b < Material::kNumBands; ++b)
                        result.histogram[(size_t) b][(size_t) bin] += energy[(size_t) b];

                    ++result.hitCount;
                }
            }
            // The ray continues past the mic (mics receive, they do not absorb).
        }

        if (! hitSurface)
            break; // ray escaped the scene

        accumDist += tSurf;
        if (accumDist > maxDist)
            break;

        // Absorption at the surface.
        const Material& material = scene.getBrushes()[(size_t) bi].getFaceMaterial (face);
        const auto reflection = material.getReflection();

        float maxEnergy = 0.0f;
        for (int b = 0; b < Material::kNumBands; ++b)
        {
            energy[(size_t) b] *= reflection[(size_t) b];
            maxEnergy = juce::jmax (maxEnergy, energy[(size_t) b]);
        }

        if (maxEnergy < kEnergyFloor)
            break; // fully absorbed (e.g. openAir sentinel, or many lossy bounces)

        // Bounce: diffuse (cosine-weighted) with probability = scattering, else specular.
        Vec3 newDir = (random.nextFloat() < material.getScattering())
                          ? sampleCosineHemisphere (n, random)
                          : reflect (d, n).normalised();

        if (newDir.lengthSquared() < 1.0e-12f)
            break;

        o = hp + n * kRayEpsilon;
        d = newDir;
    }
}
} // namespace Worldizer
