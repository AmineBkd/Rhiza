#pragma once

namespace Rhiza
{

struct Vec2
{
    float x = 0.0f;
    float y = 0.0f;
};

struct Vec3
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

// Build with fromAxisAngle; the fields are not angles.
struct Quat
{
    float w = 1.0f;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    // `axis` must be unit length.
    static Quat fromAxisAngle( Vec3 axis, float radians );
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
