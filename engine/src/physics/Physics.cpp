#include "Physics.h"

#include <box2d/box2d.h>

#include <algorithm>
#include <unordered_map>

namespace Rhiza
{

namespace
{

// Box2D's recommendation: more sub-steps trade time for stiffer stacking.
constexpr int kSubSteps = 4;

b2Vec2 toBox2d( Vec2 v )
{
    return { v.x, v.y };
}

Vec2 toRhiza( b2Vec2 v )
{
    return { v.x, v.y };
}

b2BodyType toBox2d( BodyType type )
{
    switch( type )
    {
    case BodyType::Static:
        return b2_staticBody;
    case BodyType::Kinematic:
        return b2_kinematicBody;
    case BodyType::Dynamic:
        return b2_dynamicBody;
    }
    return b2_dynamicBody;
}

// Each body carries its Rhiza handle as Box2D user data, so a shape in an
// event leads back to the body it belongs to.
void *packHandle( uint32_t handle )
{
    return reinterpret_cast<void *>( static_cast<uintptr_t>( handle ) );
}

uint32_t handleOf( b2ShapeId shape )
{
    return static_cast<uint32_t>(
        reinterpret_cast<uintptr_t>( b2Body_GetUserData( b2Shape_GetBody( shape ) ) ) );
}

std::string findShapeProblem( const ShapeDesc &desc )
{
    switch( desc.type )
    {
    case ShapeType::Circle:
    case ShapeType::Capsule:
        return desc.radius > 0.0f ? std::string() : "its radius is not above zero";
    case ShapeType::Box:
        return desc.halfExtents.x > 0.0f && desc.halfExtents.y > 0.0f ? std::string()
                                                                      : "a half extent is not above zero";
    case ShapeType::Polygon:
        if( desc.points.size() < 3 || desc.points.size() > B2_MAX_POLYGON_VERTICES )
            return "a polygon needs 3 to " + std::to_string( B2_MAX_POLYGON_VERTICES ) + " points";
        return desc.radius >= 0.0f ? std::string() : "its rounding radius is negative";
    }
    return "its type is unknown";
}

}  // namespace

struct Physics::Impl
{
    struct Body
    {
        b2BodyId id = b2_nullBodyId;
        uint64_t userData = 0;
    };

    b2WorldId world = b2_nullWorldId;
    float stepSeconds = 1.0f / 60.0f;
    float banked = 0.0f;
    uint32_t nextHandle = 1;
    std::unordered_map<uint32_t, Body> bodies;
    std::vector<ContactEvent> events;

    const Body *find( uint32_t handle ) const
    {
        auto it = bodies.find( handle );
        return it == bodies.end() ? nullptr : &it->second;
    }

    uint64_t userDataOf( uint32_t handle ) const
    {
        const Body *body = find( handle );
        return body ? body->userData : 0;
    }

    // A shape destroyed before the step leaves an invalid id behind, so
    // bodies the game destroyed report no End event.
    void record( ContactEventType type, b2ShapeId a, b2ShapeId b )
    {
        if( !b2Shape_IsValid( a ) || !b2Shape_IsValid( b ) )
            return;
        ContactEvent event;
        event.type = type;
        event.a.id = handleOf( a );
        event.b.id = handleOf( b );
        event.userDataA = userDataOf( event.a.id );
        event.userDataB = userDataOf( event.b.id );
        events.push_back( event );
    }
};

Physics::Physics() : mImpl( std::make_unique<Impl>() )
{
}

Physics::~Physics()
{
    shutdown();
}

bool Physics::initialize( Vec2 gravity, float stepSeconds )
{
    if( b2World_IsValid( mImpl->world ) || !( stepSeconds > 0.0f ) )
        return false;

    b2WorldDef def = b2DefaultWorldDef();
    def.gravity = toBox2d( gravity );
    mImpl->world = b2CreateWorld( &def );
    mImpl->stepSeconds = stepSeconds;
    return true;
}

void Physics::shutdown()
{
    if( b2World_IsValid( mImpl->world ) )
        b2DestroyWorld( mImpl->world );
    mImpl->world = b2_nullWorldId;
    mImpl->bodies.clear();
    mImpl->events.clear();
    mImpl->banked = 0.0f;
}

void Physics::setGravity( Vec2 gravity )
{
    if( b2World_IsValid( mImpl->world ) )
        b2World_SetGravity( mImpl->world, toBox2d( gravity ) );
}

uint32_t Physics::createBody( const BodyDesc &desc )
{
    Impl &p = *mImpl;
    if( !b2World_IsValid( p.world ) )
        return 0;

    const uint32_t handle = p.nextHandle++;
    b2BodyDef def = b2DefaultBodyDef();
    def.type = toBox2d( desc.type );
    def.position = toBox2d( desc.position );
    def.rotation = b2MakeRot( desc.angle );
    def.linearVelocity = toBox2d( desc.linearVelocity );
    def.angularVelocity = desc.angularVelocity;
    def.linearDamping = desc.linearDamping;
    def.angularDamping = desc.angularDamping;
    def.gravityScale = desc.gravityScale;
    def.fixedRotation = desc.fixedRotation;
    def.isBullet = desc.bullet;
    def.userData = packHandle( handle );

    p.bodies[handle] = { b2CreateBody( p.world, &def ), desc.userData };
    return handle;
}

void Physics::destroyBody( uint32_t body )
{
    auto it = mImpl->bodies.find( body );
    if( it == mImpl->bodies.end() )
        return;
    if( b2Body_IsValid( it->second.id ) )
        b2DestroyBody( it->second.id );
    mImpl->bodies.erase( it );
}

bool Physics::addShape( uint32_t body, const ShapeDesc &desc, std::string &error )
{
    const Impl::Body *found = mImpl->find( body );
    if( !found )
    {
        error = "the body does not exist";
        return false;
    }
    error = findShapeProblem( desc );
    if( !error.empty() )
        return false;

    // Box2D reports neither kind of event unless asked, and a shape that
    // collides silently is never what a game wants.
    b2ShapeDef def = b2DefaultShapeDef();
    def.density = desc.density;
    def.material.friction = desc.friction;
    def.material.restitution = desc.restitution;
    def.isSensor = desc.isSensor;
    def.enableContactEvents = true;
    def.enableSensorEvents = true;
    def.filter.categoryBits = desc.categoryBits;
    def.filter.maskBits = desc.maskBits;

    switch( desc.type )
    {
    case ShapeType::Circle:
    {
        const b2Circle circle{ toBox2d( desc.center ), desc.radius };
        b2CreateCircleShape( found->id, &def, &circle );
        break;
    }
    case ShapeType::Box:
    {
        const b2Polygon box = b2MakeOffsetBox( desc.halfExtents.x, desc.halfExtents.y, toBox2d( desc.center ),
                                               b2MakeRot( desc.angle ) );
        b2CreatePolygonShape( found->id, &def, &box );
        break;
    }
    case ShapeType::Capsule:
    {
        const b2Capsule capsule{ toBox2d( desc.pointA ), toBox2d( desc.pointB ), desc.radius };
        b2CreateCapsuleShape( found->id, &def, &capsule );
        break;
    }
    case ShapeType::Polygon:
    {
        b2Vec2 points[B2_MAX_POLYGON_VERTICES];
        for( size_t i = 0; i < desc.points.size(); ++i )
            points[i] = toBox2d( desc.points[i] );
        const b2Hull hull = b2ComputeHull( points, static_cast<int>( desc.points.size() ) );
        if( hull.count == 0 )
        {
            error = "its points are collinear or too close together to form a hull";
            return false;
        }
        const b2Polygon polygon = b2MakePolygon( &hull, desc.radius );
        b2CreatePolygonShape( found->id, &def, &polygon );
        break;
    }
    }
    return true;
}

BodyPose Physics::pose( uint32_t body ) const
{
    const Impl::Body *found = mImpl->find( body );
    if( !found )
        return {};
    return { toRhiza( b2Body_GetPosition( found->id ) ), b2Rot_GetAngle( b2Body_GetRotation( found->id ) ) };
}

void Physics::setPose( uint32_t body, const BodyPose &pose )
{
    if( const Impl::Body *found = mImpl->find( body ) )
        b2Body_SetTransform( found->id, toBox2d( pose.position ), b2MakeRot( pose.angle ) );
}

Vec2 Physics::linearVelocity( uint32_t body ) const
{
    const Impl::Body *found = mImpl->find( body );
    return found ? toRhiza( b2Body_GetLinearVelocity( found->id ) ) : Vec2{};
}

void Physics::setLinearVelocity( uint32_t body, Vec2 velocity )
{
    if( const Impl::Body *found = mImpl->find( body ) )
        b2Body_SetLinearVelocity( found->id, toBox2d( velocity ) );
}

float Physics::angularVelocity( uint32_t body ) const
{
    const Impl::Body *found = mImpl->find( body );
    return found ? b2Body_GetAngularVelocity( found->id ) : 0.0f;
}

void Physics::setAngularVelocity( uint32_t body, float velocity )
{
    if( const Impl::Body *found = mImpl->find( body ) )
        b2Body_SetAngularVelocity( found->id, velocity );
}

void Physics::applyForce( uint32_t body, Vec2 force )
{
    if( const Impl::Body *found = mImpl->find( body ) )
        b2Body_ApplyForceToCenter( found->id, toBox2d( force ), true );
}

void Physics::applyImpulse( uint32_t body, Vec2 impulse )
{
    if( const Impl::Body *found = mImpl->find( body ) )
        b2Body_ApplyLinearImpulseToCenter( found->id, toBox2d( impulse ), true );
}

void Physics::applyTorque( uint32_t body, float torque )
{
    if( const Impl::Body *found = mImpl->find( body ) )
        b2Body_ApplyTorque( found->id, torque, true );
}

void Physics::bank( float seconds )
{
    Impl &p = *mImpl;
    p.banked = std::min( p.banked + std::max( seconds, 0.0f ), p.stepSeconds * kMaxBankedSteps );
}

bool Physics::stepDue() const
{
    return b2World_IsValid( mImpl->world ) && mImpl->banked >= mImpl->stepSeconds;
}

void Physics::step()
{
    Impl &p = *mImpl;
    if( !b2World_IsValid( p.world ) )
        return;

    p.banked = std::max( p.banked - p.stepSeconds, 0.0f );
    b2World_Step( p.world, p.stepSeconds, kSubSteps );

    const b2ContactEvents contacts = b2World_GetContactEvents( p.world );
    for( int i = 0; i < contacts.beginCount; ++i )
        p.record( ContactEventType::Begin, contacts.beginEvents[i].shapeIdA,
                  contacts.beginEvents[i].shapeIdB );
    for( int i = 0; i < contacts.endCount; ++i )
        p.record( ContactEventType::End, contacts.endEvents[i].shapeIdA, contacts.endEvents[i].shapeIdB );

    const b2SensorEvents sensors = b2World_GetSensorEvents( p.world );
    for( int i = 0; i < sensors.beginCount; ++i )
        p.record( ContactEventType::SensorBegin, sensors.beginEvents[i].sensorShapeId,
                  sensors.beginEvents[i].visitorShapeId );
    for( int i = 0; i < sensors.endCount; ++i )
        p.record( ContactEventType::SensorEnd, sensors.endEvents[i].sensorShapeId,
                  sensors.endEvents[i].visitorShapeId );
}

const std::vector<ContactEvent> &Physics::events() const
{
    return mImpl->events;
}

void Physics::clearEvents()
{
    mImpl->events.clear();
}

RaycastHit Physics::raycast( Vec2 from, Vec2 to, uint64_t maskBits ) const
{
    const Impl &p = *mImpl;
    const Vec2 translation = to - from;
    if( !b2World_IsValid( p.world ) || lengthSquared( translation ) == 0.0f )
        return {};

    // The ray belongs to every category, so only `maskBits` decides what it
    // can hit.
    b2QueryFilter filter = b2DefaultQueryFilter();
    filter.categoryBits = ~uint64_t( 0 );
    filter.maskBits = maskBits;
    const b2RayResult result =
        b2World_CastRayClosest( p.world, toBox2d( from ), toBox2d( translation ), filter );
    if( !result.hit )
        return {};

    RaycastHit hit;
    hit.hit = true;
    hit.body.id = handleOf( result.shapeId );
    hit.userData = p.userDataOf( hit.body.id );
    hit.point = toRhiza( result.point );
    hit.normal = toRhiza( result.normal );
    hit.fraction = result.fraction;
    return hit;
}

}  // namespace Rhiza
