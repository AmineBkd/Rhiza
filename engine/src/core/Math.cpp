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

// v' = v + 2w(q x v) + 2 q x (q x v), with q the vector part: the expanded
// form of q v q*, without building a matrix.
Vec3 Quat::rotate( Vec3 v ) const
{
    const Vec3 t{ 2.0f * ( y * v.z - z * v.y ), 2.0f * ( z * v.x - x * v.z ), 2.0f * ( x * v.y - y * v.x ) };
    return { v.x + w * t.x + ( y * t.z - z * t.y ),
             v.y + w * t.y + ( z * t.x - x * t.z ),
             v.z + w * t.z + ( x * t.y - y * t.x ) };
}

}  // namespace Rhiza
