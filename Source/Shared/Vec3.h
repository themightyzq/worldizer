#pragma once

#include <cmath>
#include <algorithm>

namespace Worldizer
{
/**
    Minimal 3D vector for the acoustics engine.

    The ray tracer's core math is plain C++ with no JUCE dependency (see
    Docs/architecture.md §2 — "pure C++ ... can run headless"). juce::Vector3D
    would pull in the juce_opengl / GUI module stack, which the headless
    RenderTestScene tool deliberately does not link, so we use this small struct
    plus a handful of free functions instead. This is the "thin wrapper" the
    Slice 1 spec sanctions, not a math library.

    Coordinates are in metres. +Z is up by convention (sources default to z=1.5).
*/
struct Vec3
{
    float x = 0.0f, y = 0.0f, z = 0.0f;

    constexpr Vec3() = default;
    constexpr Vec3 (float xx, float yy, float zz) noexcept : x (xx), y (yy), z (zz) {}

    constexpr Vec3 operator+ (Vec3 o) const noexcept { return { x + o.x, y + o.y, z + o.z }; }
    constexpr Vec3 operator- (Vec3 o) const noexcept { return { x - o.x, y - o.y, z - o.z }; }
    constexpr Vec3 operator- ()        const noexcept { return { -x, -y, -z }; }
    constexpr Vec3 operator* (float s) const noexcept { return { x * s, y * s, z * s }; }
    constexpr Vec3 operator/ (float s) const noexcept { return { x / s, y / s, z / s }; }

    Vec3& operator+= (Vec3 o) noexcept { x += o.x; y += o.y; z += o.z; return *this; }

    float operator[] (int i) const noexcept { return (i == 0) ? x : (i == 1) ? y : z; }

    float lengthSquared() const noexcept { return x * x + y * y + z * z; }
    float length()        const noexcept { return std::sqrt (lengthSquared()); }

    Vec3 normalised() const noexcept
    {
        const float len = length();
        return len > 1.0e-12f ? Vec3 { x / len, y / len, z / len } : Vec3 { 0.0f, 0.0f, 0.0f };
    }
};

inline float dot (Vec3 a, Vec3 b) noexcept { return a.x * b.x + a.y * b.y + a.z * b.z; }

inline Vec3 cross (Vec3 a, Vec3 b) noexcept
{
    return { a.y * b.z - a.z * b.y,
             a.z * b.x - a.x * b.z,
             a.x * b.y - a.y * b.x };
}

/** Mirror an incoming direction d about a unit surface normal n. */
inline Vec3 reflect (Vec3 d, Vec3 n) noexcept { return d - n * (2.0f * dot (d, n)); }

inline Vec3 componentMin (Vec3 a, Vec3 b) noexcept
{
    return { std::min (a.x, b.x), std::min (a.y, b.y), std::min (a.z, b.z) };
}

inline Vec3 componentMax (Vec3 a, Vec3 b) noexcept
{
    return { std::max (a.x, b.x), std::max (a.y, b.y), std::max (a.z, b.z) };
}
} // namespace Worldizer
