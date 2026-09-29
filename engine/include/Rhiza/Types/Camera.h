#pragma once

#include <Rhiza/Types/Math.h>

namespace Rhiza
{

enum class Projection
{
    Perspective,
    Orthographic,
};

// Camera behaviour (follow, shake, zoom) lives above the engine and
// produces one of these per frame.
struct CameraDesc
{
    Projection projection = Projection::Perspective;
    Vec3 position{ 0.0f, 0.0f, 10.0f };

    // Identity looks down -Z with +Y up.
    Quat orientation;

    float fovYDegrees = 45.0f;

    // World units visible top to bottom; width follows the aspect ratio.
    float orthoHeight = 10.0f;

    float nearClip = 0.1f;
    float farClip = 1000.0f;
};

}  // namespace Rhiza
