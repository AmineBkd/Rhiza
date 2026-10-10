#pragma once

#include <cmath>

namespace Rhiza
{

struct Vec2
{
    float x = 0.0f;
    float y = 0.0f;

    bool operator==( const Vec2 & ) const = default;
};

struct Vec3
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    bool operator==( const Vec3 & ) const = default;
};

constexpr Vec2 operator+( Vec2 a, Vec2 b ) { return { a.x + b.x, a.y + b.y }; }
constexpr Vec2 operator-( Vec2 a, Vec2 b ) { return { a.x - b.x, a.y - b.y }; }
constexpr Vec2 operator-( Vec2 v ) { return { -v.x, -v.y }; }
constexpr Vec2 operator*( Vec2 v, float s ) { return { v.x * s, v.y * s }; }
constexpr Vec2 operator*( float s, Vec2 v ) { return v * s; }
constexpr Vec2 operator/( Vec2 v, float s ) { return { v.x / s, v.y / s }; }
constexpr Vec2 &operator+=( Vec2 &a, Vec2 b ) { return a = a + b; }
constexpr Vec2 &operator-=( Vec2 &a, Vec2 b ) { return a = a - b; }
constexpr Vec2 &operator*=( Vec2 &v, float s ) { return v = v * s; }
constexpr Vec2 &operator/=( Vec2 &v, float s ) { return v = v / s; }

constexpr Vec3 operator+( Vec3 a, Vec3 b ) { return { a.x + b.x, a.y + b.y, a.z + b.z }; }
constexpr Vec3 operator-( Vec3 a, Vec3 b ) { return { a.x - b.x, a.y - b.y, a.z - b.z }; }
constexpr Vec3 operator-( Vec3 v ) { return { -v.x, -v.y, -v.z }; }
constexpr Vec3 operator*( Vec3 v, float s ) { return { v.x * s, v.y * s, v.z * s }; }
constexpr Vec3 operator*( float s, Vec3 v ) { return v * s; }
constexpr Vec3 operator/( Vec3 v, float s ) { return { v.x / s, v.y / s, v.z / s }; }
constexpr Vec3 &operator+=( Vec3 &a, Vec3 b ) { return a = a + b; }
constexpr Vec3 &operator-=( Vec3 &a, Vec3 b ) { return a = a - b; }
constexpr Vec3 &operator*=( Vec3 &v, float s ) { return v = v * s; }
constexpr Vec3 &operator/=( Vec3 &v, float s ) { return v = v / s; }

constexpr float dot( Vec2 a, Vec2 b ) { return a.x * b.x + a.y * b.y; }
constexpr float dot( Vec3 a, Vec3 b ) { return a.x * b.x + a.y * b.y + a.z * b.z; }

// 2D: the z of the 3D cross product - positive when b is anticlockwise of a.
constexpr float cross( Vec2 a, Vec2 b ) { return a.x * b.y - a.y * b.x; }
constexpr Vec3 cross( Vec3 a, Vec3 b )
{
    return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x };
}

constexpr float lengthSquared( Vec2 v ) { return dot( v, v ); }
constexpr float lengthSquared( Vec3 v ) { return dot( v, v ); }
inline float length( Vec2 v ) { return std::sqrt( lengthSquared( v ) ); }
inline float length( Vec3 v ) { return std::sqrt( lengthSquared( v ) ); }

// A zero vector stays zero rather than becoming NaN.
inline Vec2 normalized( Vec2 v )
{
    const float l = length( v );
    return l > 0.0f ? v / l : v;
}
inline Vec3 normalized( Vec3 v )
{
    const float l = length( v );
    return l > 0.0f ? v / l : v;
}

// Build with fromAxisAngle; the fields are not angles.
struct Quat
{
    float w = 1.0f;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    // `axis` must be unit length.
    static Quat fromAxisAngle( Vec3 axis, float radians );

    Vec3 rotate( Vec3 v ) const;
};

struct Color
{
    float r = 1.0f;
    float g = 1.0f;
    float b = 1.0f;
    float a = 1.0f;
};

struct Transform
{
    Vec3 position;
    Quat rotation;
    Vec3 scale{ 1.0f, 1.0f, 1.0f };
};

}  // namespace Rhiza
