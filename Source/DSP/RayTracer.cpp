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
    const auto& source   = scene.getSource();
    const auto& micArray = scene.getMicArray();
    const int   numMics  = micArray.getNumMics();

    Result result;
    result.sampleRate = sampleRate;
    result.numBins    = (int) std::ceil (settings.maxTraceTimeSeconds * (float) sampleRate);
    result.numRays    = settings.numRays;
    result.micRadius  = micArray.getPrimary().getRadius();

    result.histogramsPerMic.resize ((size_t) numMics);
    result.directPerMic.assign ((size_t) numMics, Result::DirectInfo {});
    result.hitCountPerMic.assign ((size_t) numMics, 0);
    result.micPositions.resize ((size_t) numMics);
    for (int m = 0; m < numMics; ++m)
    {
        result.micPositions[(size_t) m] = micArray.getMic (m).getPosition();
        result.anyMicDirectional = result.anyMicDirectional || micArray.getMic (m).isDirectional();
    }
    for (auto& mh : result.histogramsPerMic)
        for (auto& band : mh)
            band.assign ((size_t) result.numBins, 0.0f);

    // Snapshot the brush list ONCE (manual + compiled sectors). All bounce intersections
    // iterate this local vector — re-compiling per ray would be needlessly expensive.
    const auto brushList = scene.getAllBrushesForTracing();

    // === Direct sound, per mic (deterministic) ===
    const Vec3 sourcePos = source.getPosition();
    for (int m = 0; m < numMics; ++m)
    {
        const auto& mic    = micArray.getMic (m);
        const Vec3  micPos = mic.getPosition();
        const Vec3  toMic  = micPos - sourcePos;
        const float dist   = toMic.length();

        auto& di = result.directPerMic[(size_t) m];
        di.distance    = dist;
        di.arrivalTime = dist / settings.speedOfSound;

        if (dist > 1.0e-6f)
        {
            const Vec3 dir = toMic / dist;
            int bi; float t; Vec3 hp, n; Brush::Face face;
            const bool occluded = Scene::intersectBrushList (brushList, sourcePos, dir, kRayEpsilon, bi, t, hp, n, face)
                                    && t < dist - mic.getRadius();
            di.visible = ! occluded;

            // Incoming direction at the mic = from mic toward the source. Source
            // emission direction = from source toward the mic. Both 1.0 for omni.
            const Vec3  incoming = (sourcePos - micPos) / dist;
            const float micGain  = mic.getReceptionGain (incoming);
            const float srcGain  = source.getEmissionGain (toMic / dist);
            di.receptionGain     = micGain * srcGain;
        }
        else
        {
            di.visible = true;
            di.receptionGain = 1.0f;
        }
    }

    // === Stochastic reflections ===
    juce::Random random ((juce::int64) settings.randomSeed);

    for (int i = 0; i < settings.numRays; ++i)
    {
        const Vec3  dir      = source.sampleEmissionDirection (random);
        const float emitGain = source.getEmissionGain (dir);
        traceRay (brushList, micArray, sourcePos, dir, emitGain, settings, result, random);
    }

    return result;
}

void RayTracer::traceRay (const std::vector<Brush>& brushList,
                          const MicArray& micArray,
                          Vec3 origin,
                          Vec3 direction,
                          float emissionGain,
                          const Settings& settings,
                          Result& result,
                          juce::Random& random) const
{
    const float c        = settings.speedOfSound;
    const float maxDist  = settings.maxTraceTimeSeconds * c;
    const int   sr       = result.sampleRate;
    const int   numMics  = micArray.getNumMics();

    // Initial ray energy weighted by the source's emission pattern (energy domain =
    // amplitude^2). Omni => emissionGain == 1.0 => energy 1.0 (Slice 4.5 behaviour).
    std::array<float, Material::kNumBands> energy;
    energy.fill (emissionGain * emissionGain);

    Vec3 o = origin;
    Vec3 d = direction.normalised();
    float accumDist = 0.0f;

    for (int bounce = 0; bounce <= settings.maxBounces; ++bounce)
    {
        int bi; float tSurf; Vec3 hp, n; Brush::Face face;
        const bool hitSurface = Scene::intersectBrushList (brushList, o, d, kRayEpsilon, bi, tSurf, hp, n, face);

        const float remaining = maxDist - accumDist;
        if (remaining <= 0.0f)
            break;

        const float segMax = hitSurface ? juce::jmin (tSurf, remaining) : remaining;

        // Deposit a mic hit on this segment for each mic in the array — but only from
        // the first reflection onward (bounce 0 is the direct path, handled in trace()).
        if (bounce >= 1)
        {
            // Incoming direction at the mic = from the mic looking back along the ray
            // = negation of the ray's travel direction (d is already unit).
            const Vec3 incoming = -d;

            for (int m = 0; m < numMics; ++m)
            {
                const auto& mic = micArray.getMic (m);
                float micT;
                if (mic.intersectRay (o, d, segMax, micT))
                {
                    const float arrivalTime = (accumDist + micT) / c;
                    const int   bin = (int) std::lround (arrivalTime * (float) sr);

                    if (bin >= 0 && bin < result.numBins)
                    {
                        // Reception pattern weights energy (amplitude^2). Omni => 1.0.
                        const float recGain   = mic.getReceptionGain (incoming);
                        const float recGainSq = recGain * recGain;

                        auto& hist = result.histogramsPerMic[(size_t) m];
                        for (int b = 0; b < Material::kNumBands; ++b)
                            hist[(size_t) b][(size_t) bin] += energy[(size_t) b] * recGainSq;

                        ++result.hitCountPerMic[(size_t) m];
                    }
                }
            }
            // The ray continues past the mic(s) (mics receive, they do not absorb).
        }

        if (! hitSurface)
            break; // ray escaped the scene

        accumDist += tSurf;
        if (accumDist > maxDist)
            break;

        // Absorption at the surface.
        const Material& material = brushList[(size_t) bi].getFaceMaterial (face);
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
