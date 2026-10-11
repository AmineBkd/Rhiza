// Tests for the physics layer. Box2D needs no window or GPU, so bodies,
// fixed steps, contacts, sensors, filtering and raycasts run headless.

#include "physics/Physics.h"

#include <Rhiza/Systems/PhysicsSystem.h>

#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <string>

namespace
{

using Rhiza::BodyType;
using Rhiza::ContactEventType;
using Rhiza::ShapeDesc;
using Rhiza::Vec2;

constexpr float kStep = 1.0f / 60.0f;

struct World
{
    Rhiza::Physics physics;

    explicit World( Vec2 gravity = {} ) { physics.initialize( gravity, kStep ); }

    uint32_t body( BodyType type, Vec2 position, const ShapeDesc &shape, uint64_t userData = 0 )
    {
        Rhiza::BodyDesc desc;
        desc.type = type;
        desc.position = position;
        desc.userData = userData;
        const uint32_t handle = physics.createBody( desc );
        std::string error;
        const bool added = physics.addShape( handle, shape, error );
        INFO( error );
        REQUIRE( added );
        return handle;
    }

    void run( int steps )
    {
        for( int i = 0; i < steps; ++i )
            physics.step();
    }

    int count( ContactEventType type ) const
    {
        int n = 0;
        for( const Rhiza::ContactEvent &event : physics.events() )
            n += event.type == type ? 1 : 0;
        return n;
    }

    const Rhiza::ContactEvent *first( ContactEventType type ) const
    {
        for( const Rhiza::ContactEvent &event : physics.events() )
            if( event.type == type )
                return &event;
        return nullptr;
    }
};

ShapeDesc bouncy( ShapeDesc shape )
{
    shape.restitution = 1.0f;
    return shape;
}

TEST_CASE( "Physics: a dynamic body falls onto a static one and rests there" )
{
    World world( { 0.0f, -10.0f } );
    const uint32_t ball = world.body( BodyType::Dynamic, { 0.0f, 10.0f }, ShapeDesc::circle( 0.5f ) );
    const uint32_t floor = world.body( BodyType::Static, {}, ShapeDesc::box( { 5.0f, 0.5f } ) );

    world.run( 30 );
    CHECK( world.physics.pose( ball ).position.y < 10.0f );
    CHECK( world.physics.linearVelocity( ball ).y < 0.0f );

    world.run( 240 );
    CHECK( world.physics.pose( ball ).position.y == Catch::Approx( 1.0f ).margin( 0.05f ) );
    CHECK( world.physics.pose( floor ).position == Vec2{} );
}

TEST_CASE( "Physics: time is spent in fixed steps, and never backward" )
{
    World world;
    auto stepsDue = [&world] {
        int steps = 0;
        while( world.physics.stepDue() )
        {
            world.physics.step();
            ++steps;
        }
        return steps;
    };

    world.physics.bank( kStep * 3.5f );
    CHECK( stepsDue() == 3 );

    // The half step left over is spent once the next frame tops it up.
    world.physics.bank( kStep * 0.5f );
    CHECK( stepsDue() == 1 );

    world.physics.bank( 0.0f );
    CHECK( stepsDue() == 0 );

    // Reversed time must not leave a debt that stalls physics afterwards.
    world.physics.bank( -1.0f );
    CHECK( stepsDue() == 0 );
    world.physics.bank( kStep * 1.5f );
    CHECK( stepsDue() == 1 );

    world.physics.bank( 10.0f );
    CHECK( stepsDue() == Rhiza::Physics::kMaxBankedSteps );
}

TEST_CASE( "Physics: touching shapes report Begin, and End once they part" )
{
    World world;
    const uint32_t a =
        world.body( BodyType::Dynamic, { -2.0f, 0.0f }, bouncy( ShapeDesc::circle( 0.5f ) ), 11 );
    const uint32_t b =
        world.body( BodyType::Dynamic, { 2.0f, 0.0f }, bouncy( ShapeDesc::circle( 0.5f ) ), 22 );
    world.physics.setLinearVelocity( a, { 5.0f, 0.0f } );
    world.physics.setLinearVelocity( b, { -5.0f, 0.0f } );

    world.run( 60 );
    const Rhiza::ContactEvent *begin = world.first( ContactEventType::Begin );
    REQUIRE( begin != nullptr );
    const bool aFirst = begin->a.id == a;
    CHECK( ( aFirst ? begin->b.id : begin->a.id ) == b );
    CHECK( ( aFirst ? begin->userDataA : begin->userDataB ) == 11 );
    CHECK( ( aFirst ? begin->userDataB : begin->userDataA ) == 22 );
    CHECK( world.count( ContactEventType::End ) >= 1 );
    CHECK( world.physics.linearVelocity( a ).x < 0.0f );
}

TEST_CASE( "Physics: a sensor reports overlaps without colliding" )
{
    World world;
    ShapeDesc zone = ShapeDesc::box( { 1.0f, 1.0f } );
    zone.isSensor = true;
    const uint32_t sensor = world.body( BodyType::Static, {}, zone );
    const uint32_t visitor = world.body( BodyType::Dynamic, { -3.0f, 0.0f }, ShapeDesc::circle( 0.25f ) );
    world.physics.setLinearVelocity( visitor, { 5.0f, 0.0f } );

    world.run( 120 );
    const Rhiza::ContactEvent *entered = world.first( ContactEventType::SensorBegin );
    REQUIRE( entered != nullptr );
    CHECK( entered->a.id == sensor );
    CHECK( entered->b.id == visitor );
    CHECK( world.count( ContactEventType::SensorEnd ) == 1 );
    CHECK( world.count( ContactEventType::Begin ) == 0 );
    CHECK( world.physics.pose( visitor ).position.x > 2.0f );
}

TEST_CASE( "Physics: categories and masks decide what collides" )
{
    World world;
    ShapeDesc ghost = ShapeDesc::circle( 0.5f );
    ghost.categoryBits = 2;
    ShapeDesc solid = ShapeDesc::circle( 0.5f );
    solid.maskBits = ~uint64_t( 2 );
    const uint32_t a = world.body( BodyType::Dynamic, { -2.0f, 0.0f }, ghost );
    const uint32_t b = world.body( BodyType::Dynamic, { 2.0f, 0.0f }, solid );
    world.physics.setLinearVelocity( a, { 5.0f, 0.0f } );
    world.physics.setLinearVelocity( b, { -5.0f, 0.0f } );

    world.run( 60 );
    CHECK( world.count( ContactEventType::Begin ) == 0 );
    CHECK( world.physics.pose( a ).position.x > 2.0f );
}

TEST_CASE( "Physics: a raycast hits the nearest shape its mask allows" )
{
    World world;
    ShapeDesc nearBox = ShapeDesc::box( { 0.5f, 0.5f } );
    nearBox.categoryBits = 2;
    const uint32_t nearest = world.body( BodyType::Static, { 3.0f, 0.0f }, nearBox, 7 );
    const uint32_t farther = world.body( BodyType::Static, { 6.0f, 0.0f }, ShapeDesc::box( { 0.5f, 0.5f } ) );

    const Rhiza::RaycastHit hit = world.physics.raycast( {}, { 10.0f, 0.0f }, ~uint64_t( 0 ) );
    REQUIRE( hit.hit );
    CHECK( hit.body.id == nearest );
    CHECK( hit.userData == 7 );
    CHECK( hit.point.x == Catch::Approx( 2.5f ).margin( 0.01f ) );
    CHECK( hit.normal.x == Catch::Approx( -1.0f ).margin( 0.01f ) );
    CHECK( hit.fraction == Catch::Approx( 0.25f ).margin( 0.01f ) );

    const Rhiza::RaycastHit masked = world.physics.raycast( {}, { 10.0f, 0.0f }, ~uint64_t( 2 ) );
    REQUIRE( masked.hit );
    CHECK( masked.body.id == farther );

    CHECK( !world.physics.raycast( { 0.0f, 5.0f }, { 10.0f, 5.0f }, ~uint64_t( 0 ) ).hit );
}

TEST_CASE( "Physics: forces, impulses and teleports" )
{
    World world;
    const uint32_t body = world.body( BodyType::Dynamic, {}, ShapeDesc::circle( 0.5f ) );

    world.physics.applyImpulse( body, { 2.0f, 0.0f } );
    const float afterImpulse = world.physics.linearVelocity( body ).x;
    CHECK( afterImpulse > 0.0f );

    world.physics.applyForce( body, { 10.0f, 0.0f } );
    world.physics.applyTorque( body, 1.0f );
    world.run( 1 );
    CHECK( world.physics.linearVelocity( body ).x > afterImpulse );
    CHECK( world.physics.angularVelocity( body ) > 0.0f );

    world.physics.setPose( body, { { 4.0f, 5.0f }, 1.0f } );
    CHECK( world.physics.pose( body ).position == Vec2{ 4.0f, 5.0f } );
    CHECK( world.physics.pose( body ).angle == Catch::Approx( 1.0f ).margin( 0.001f ) );
}

TEST_CASE( "Physics: unusable shapes and unknown bodies are rejected" )
{
    World world;
    Rhiza::BodyDesc desc;
    const uint32_t body = world.physics.createBody( desc );
    std::string error;

    CHECK( !world.physics.addShape( body, ShapeDesc::circle( 0.0f ), error ) );
    CHECK( !error.empty() );
    CHECK( !world.physics.addShape( body, ShapeDesc::box( { 1.0f, 0.0f } ), error ) );
    CHECK( !world.physics.addShape( body, ShapeDesc::polygon( { { 0, 0 }, { 1, 0 } } ), error ) );
    CHECK( !world.physics.addShape( body, ShapeDesc::polygon( { { 0, 0 }, { 1, 0 }, { 2, 0 } } ), error ) );
    CHECK( !world.physics.addShape( 999, ShapeDesc::circle( 1.0f ), error ) );
    CHECK( world.physics.addShape( body, ShapeDesc::polygon( { { 0, 0 }, { 1, 0 }, { 0, 1 } } ), error ) );
}

TEST_CASE( "Physics: a destroyed body is forgotten" )
{
    World world;
    const uint32_t body = world.body( BodyType::Dynamic, { 3.0f, 0.0f }, ShapeDesc::circle( 0.5f ) );
    world.physics.destroyBody( body );
    world.physics.destroyBody( body );

    std::string error;
    CHECK( world.physics.pose( body ).position == Vec2{} );
    CHECK( !world.physics.addShape( body, ShapeDesc::circle( 0.5f ), error ) );
    CHECK( !world.physics.raycast( {}, { 10.0f, 0.0f }, ~uint64_t( 0 ) ).hit );
}

TEST_CASE( "Physics: entities round-trip through body user data" )
{
    const Rhiza::Entity entity{ 5, 7 };
    CHECK( Rhiza::entityFromUserData( Rhiza::toUserData( entity ) ) == entity );
    CHECK( Rhiza::toUserData( Rhiza::Entity{ 0, 0 } ) != 0 );
    CHECK( Rhiza::toUserData( Rhiza::Entity{} ) == 0 );
    CHECK( !Rhiza::entityFromUserData( 0 ).isValid() );
}

// Hidden from ctest; run with: RhizaPhysicsTests [benchmark]
TEST_CASE( "Physics: step benchmark", "[.benchmark]" )
{
    // Bouncing without loss in a closed arena, so no body ever comes to rest:
    // Box2D puts resting bodies to sleep, and sleeping bodies cost nothing.
    World world;
    const ShapeDesc wallX = ShapeDesc::box( { 0.5f, 21.0f } );
    const ShapeDesc wallY = ShapeDesc::box( { 21.0f, 0.5f } );
    world.body( BodyType::Static, { -20.5f, 0.0f }, wallX );
    world.body( BodyType::Static, { 20.5f, 0.0f }, wallX );
    world.body( BodyType::Static, { 0.0f, -20.5f }, wallY );
    world.body( BodyType::Static, { 0.0f, 20.5f }, wallY );
    for( int i = 0; i < 500; ++i )
    {
        const Vec2 position{ static_cast<float>( i % 25 ) * 1.5f - 18.0f,
                             static_cast<float>( i / 25 ) * 1.5f - 14.0f };
        const uint32_t body = world.body( BodyType::Dynamic, position, bouncy( ShapeDesc::circle( 0.5f ) ) );
        world.physics.setLinearVelocity(
            body, { static_cast<float>( i % 7 ) - 3.0f, static_cast<float>( i % 5 ) - 2.0f } );
    }
    world.run( 60 );

    BENCHMARK( "one step, 500 circles bouncing in an arena" )
    {
        world.physics.step();
        world.physics.clearEvents();
    };
}

}  // namespace
