#pragma once

#include <Rhiza/Types/Handles.h>
#include <Rhiza/Types/Math.h>

#include <cstdint>
#include <utility>
#include <vector>

namespace Rhiza
{

// Physics is 2D, in the XY plane: positions are Vec2, angles are radians
// about +Z, and one world unit is one metre. Box2D is tuned for moving
// objects between 0.1 and 10 units across.

enum class BodyType
{
    Static,     // never moves: walls, terrain
    Kinematic,  // moves only at the velocity it is given; pushes, is never pushed
    Dynamic,    // moved by forces, gravity and collisions
};

struct BodyDesc
{
    BodyType type = BodyType::Dynamic;
    Vec2 position;
    float angle = 0.0f;
    Vec2 linearVelocity;
    float angularVelocity = 0.0f;

    // Drag: the fraction of velocity lost per second, roughly.
    float linearDamping = 0.0f;
    float angularDamping = 0.0f;
    float gravityScale = 1.0f;
    bool fixedRotation = false;

    // Continuous collision against other dynamic bodies, so a fast
    // projectile cannot pass through a thin ship between two steps.
    bool bullet = false;

    // Handed back in contact events and raycast hits - typically the entity
    // that owns the body (see toUserData in Systems/PhysicsSystem.h).
    uint64_t userData = 0;
};

enum class ShapeType
{
    Circle,
    Box,
    Capsule,
    Polygon,
};

// Build with the static functions; which fields apply depends on the type.
struct ShapeDesc
{
    ShapeType type = ShapeType::Circle;
    Vec2 center;                     // Circle, Box: offset from the body's origin
    Vec2 halfExtents{ 0.5f, 0.5f };  // Box
    float angle = 0.0f;              // Box: rotation relative to the body
    Vec2 pointA;                     // Capsule: centre of one end
    Vec2 pointB;                     // Capsule: centre of the other end
    float radius = 0.5f;             // Circle, Capsule; Polygon: corner rounding
    std::vector<Vec2> points;        // Polygon: convex hull of 3 to 8 points

    float density = 1.0f;
    float friction = 0.6f;
    float restitution = 0.0f;  // bounciness: 0 stops dead, 1 bounces back fully

    // Reports overlaps as SensorBegin/SensorEnd events instead of colliding.
    bool isSensor = false;

    // Two shapes collide only if each one's category is in the other's mask.
    uint64_t categoryBits = 1;
    uint64_t maskBits = ~uint64_t( 0 );

    static ShapeDesc circle( float radius, Vec2 center = {} )
    {
        ShapeDesc shape;
        shape.type = ShapeType::Circle;
        shape.radius = radius;
        shape.center = center;
        return shape;
    }

    static ShapeDesc box( Vec2 halfExtents, Vec2 center = {}, float angle = 0.0f )
    {
        ShapeDesc shape;
        shape.type = ShapeType::Box;
        shape.halfExtents = halfExtents;
        shape.center = center;
        shape.angle = angle;
        return shape;
    }

    static ShapeDesc capsule( Vec2 pointA, Vec2 pointB, float radius )
    {
        ShapeDesc shape;
        shape.type = ShapeType::Capsule;
        shape.pointA = pointA;
        shape.pointB = pointB;
        shape.radius = radius;
        return shape;
    }

    static ShapeDesc polygon( std::vector<Vec2> points, float radius = 0.0f )
    {
        ShapeDesc shape;
        shape.type = ShapeType::Polygon;
        shape.points = std::move( points );
        shape.radius = radius;
        return shape;
    }
};

struct BodyPose
{
    Vec2 position;
    float angle = 0.0f;
};

enum class ContactEventType
{
    Begin,        // two solid shapes started touching
    End,          // and stopped
    SensorBegin,  // a shape entered a sensor; `a` is the sensor's body
    SensorEnd,
};

// userData is copied in because a body may be destroyed before the game
// reads the event.
struct ContactEvent
{
    ContactEventType type = ContactEventType::Begin;
    BodyHandle a;
    BodyHandle b;
    uint64_t userDataA = 0;
    uint64_t userDataB = 0;
};

struct RaycastHit
{
    bool hit = false;
    BodyHandle body;
    uint64_t userData = 0;
    Vec2 point;
    Vec2 normal;
    float fraction = 0.0f;  // where along the ray: 0 at `from`, 1 at `to`
};

}  // namespace Rhiza
