#pragma once

#include <Rhiza/Types/Math.h>

namespace Rhiza
{

enum class LightType
{
    // Infinitely far away, so only its direction matters. The sun.
    Directional,
    // Radiates from a point, fading with distance.
    Point,
};

struct LightDesc
{
    LightType type = LightType::Directional;

    // Directional only: the direction light travels, not where it comes
    // from. The default points down-and-away, like afternoon sun.
    Vec3 direction{ -1.0f, -1.0f, -1.0f };

    // Point only.
    Vec3 position{ 0.0f, 0.0f, 0.0f };

    Color color;

    // 1.0 is "normal" exposure for a scene with no HDR tonemapping, which is
    // what Rhiza currently renders.
    float power = 1.0f;
};

}  // namespace Rhiza
