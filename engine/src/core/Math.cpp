#include <Rhiza/Types/Math.h>

#include <cmath>

namespace Rhiza
{

Quat Quat::fromAxisAngle( Vec3 axis, float radians )
{
    const float half = radians * 0.5f;
    const float s = std::sin( half );
    return Quat{ std::cos( half ), axis.x * s, axis.y * s, axis.z * s };
}

}  // namespace Rhiza
