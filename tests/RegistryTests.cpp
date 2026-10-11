// Tests for Rhiza::Registry (the ECS core).
//
// Pure data structure - no window, no SDL, no GPU - so like Shapes/Input/
// Clock it can be checked in a plain console process. Worth testing
// properly because a sparse set gets two things wrong easily and silently:
// swap-and-pop corrupting an unrelated entity's component on removal, and a
// stale handle aliasing onto whatever now occupies its recycled slot. Both
// would show up as "the wrong entity's data" bugs far from this file.

#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/catch_test_macros.hpp>

#include <Rhiza/Registry.h>

#include <string>
#include <vector>

namespace
{

struct Position
{
    float x = 0.0f;
    float y = 0.0f;
};

struct Velocity
{
    float dx = 0.0f;
    float dy = 0.0f;
};

TEST_CASE( "Registry: create and destroy" )
{
    Rhiza::Registry registry;

    Rhiza::Entity a = registry.createEntity();
    Rhiza::Entity b = registry.createEntity();

    CHECK( a.isValid() );
    CHECK( b.isValid() );
    CHECK( a != b );
    CHECK( registry.isAlive( a ) );
    CHECK( registry.isAlive( b ) );

    registry.destroyEntity( a );
    CHECK( !registry.isAlive( a ) );
    CHECK( registry.isAlive( b ) );

    // Double-destroy must not crash or affect anything else.
    registry.destroyEntity( a );
    CHECK( registry.isAlive( b ) );
}

TEST_CASE( "Registry: stale handle after slot recycling" )
{
    Rhiza::Registry registry;

    Rhiza::Entity original = registry.createEntity();
    registry.addComponent( original, Position{ 1.0f, 2.0f } );
    registry.destroyEntity( original );

    // With only one entity ever created, the freed slot is the only one
    // available - createEntity() must reuse original.index, but with a
    // bumped generation.
    Rhiza::Entity recycled = registry.createEntity();
    CHECK( recycled.index == original.index );
    CHECK( recycled.generation != original.generation );
    CHECK( !registry.isAlive( original ) );
    CHECK( registry.isAlive( recycled ) );

    // The sharpest version of the bug class this exists to catch: the old
    // handle must not read the new occupant's component just because they
    // share an index.
    registry.addComponent( recycled, Position{ 9.0f, 9.0f } );
    CHECK( registry.getComponent<Position>( original ) == nullptr );
    CHECK( registry.getComponent<Position>( recycled ) != nullptr );
}

TEST_CASE( "Registry: add/get/has/remove component" )
{
    Rhiza::Registry registry;
    Rhiza::Entity e = registry.createEntity();

    CHECK( !registry.hasComponent<Position>( e ) );
    CHECK( registry.getComponent<Position>( e ) == nullptr );

    registry.addComponent( e, Position{ 3.0f, 4.0f } );
    CHECK( registry.hasComponent<Position>( e ) );

    Position *p = registry.getComponent<Position>( e );
    CHECK( p != nullptr );
    if( p )
        CHECK( p->x == 3.0f );
    CHECK( p->y == 4.0f );

    registry.removeComponent<Position>( e );
    CHECK( !registry.hasComponent<Position>( e ) );
    CHECK( registry.getComponent<Position>( e ) == nullptr );

    // Removing a component an entity never had must be a harmless no-op.
    registry.removeComponent<Velocity>( e );
}

TEST_CASE( "Registry: swap-and-pop leaves other entities intact" )
{
    Rhiza::Registry registry;

    Rhiza::Entity a = registry.createEntity();
    Rhiza::Entity b = registry.createEntity();
    Rhiza::Entity c = registry.createEntity();
    registry.addComponent( a, Position{ 1.0f, 1.0f } );
    registry.addComponent( b, Position{ 2.0f, 2.0f } );
    registry.addComponent( c, Position{ 3.0f, 3.0f } );

    // Removing the middle entry forces the pool's swap-and-pop to move c's
    // component into b's old slot - exactly the operation that corrupts
    // data if the sparse-array bookkeeping isn't patched correctly.
    registry.removeComponent<Position>( b );

    CHECK( !registry.hasComponent<Position>( b ) );
    Position *pa = registry.getComponent<Position>( a );
    Position *pc = registry.getComponent<Position>( c );
    REQUIRE( pa != nullptr );
    CHECK( pa->x == 1.0f );
    REQUIRE( pc != nullptr );
    CHECK( pc->x == 3.0f );
}

TEST_CASE( "Registry: destroyEntity clears every component pool" )
{
    Rhiza::Registry registry;

    Rhiza::Entity e = registry.createEntity();
    Rhiza::Entity other = registry.createEntity();
    registry.addComponent( e, Position{ 1.0f, 1.0f } );
    registry.addComponent( e, Velocity{ 0.5f, 0.5f } );
    registry.addComponent( other, Position{ 7.0f, 7.0f } );

    registry.destroyEntity( e );

    CHECK( !registry.hasComponent<Position>( e ) );
    CHECK( !registry.hasComponent<Velocity>( e ) );
    Position *po = registry.getComponent<Position>( other );
    REQUIRE( po != nullptr );
    CHECK( po->x == 7.0f );
}

TEST_CASE( "Registry: view iterates exactly the right entities" )
{
    Rhiza::Registry registry;

    Rhiza::Entity a = registry.createEntity();
    Rhiza::Entity b = registry.createEntity();
    Rhiza::Entity c = registry.createEntity();
    registry.addComponent( a, Position{ 1.0f, 0.0f } );
    registry.addComponent( c, Position{ 3.0f, 0.0f } );
    CHECK( !registry.hasComponent<Position>( b ) );

    int count = 0;
    float sumX = 0.0f;
    for( auto [entity, position] : registry.view<Position>() )
    {
        ++count;
        sumX += position.x;
        CHECK( ( entity == a || entity == c ) );
    }
    CHECK( count == 2 );
    CHECK( sumX == 4.0f );

    // Mutate through the view and confirm it stuck.
    for( auto [entity, position] : registry.view<Position>() )
        position.x += 10.0f;
    Position *pa = registry.getComponent<Position>( a );
    REQUIRE( pa != nullptr );
    CHECK( pa->x == 11.0f );
}

TEST_CASE( "Registry: view<A, B> yields only entities holding both" )
{
    Rhiza::Registry registry;

    const Rhiza::Entity both = registry.createEntity();
    registry.addComponent( both, Position{ 1.0f, 2.0f } );
    registry.addComponent( both, Velocity{ 3.0f, 4.0f } );

    const Rhiza::Entity positionOnly = registry.createEntity();
    registry.addComponent( positionOnly, Position{ 9.0f, 9.0f } );

    const Rhiza::Entity velocityOnly = registry.createEntity();
    registry.addComponent( velocityOnly, Velocity{ 9.0f, 9.0f } );

    int visited = 0;
    Rhiza::Entity seen;
    for( auto [entity, position, velocity] : registry.view<Position, Velocity>() )
    {
        ++visited;
        seen = entity;
        position.x += velocity.dx;
    }

    CHECK( visited == 1 );
    CHECK( seen == both );

    const Position *p = registry.getComponent<Position>( both );
    REQUIRE( p != nullptr );
    CHECK( p->x == 4.0f );

    const Position *untouched = registry.getComponent<Position>( positionOnly );
    REQUIRE( untouched != nullptr );
    CHECK( untouched->x == 9.0f );

    // The entity holding only Velocity sits earlier in Velocity's dense array
    // than `both` does, so iterating Velocity first exercises skipToMatch
    // having to advance past a non-match at index 0.
    int reversed = 0;
    for( auto [entity, velocity, position] : registry.view<Velocity, Position>() )
    {
        ++reversed;
        (void)entity;
        (void)velocity;
        (void)position;
    }
    CHECK( reversed == 1 );

    registry.destroyEntity( both );
    int afterDestroy = 0;
    for( auto [entity, position, velocity] : registry.view<Position, Velocity>() )
    {
        ++afterDestroy;
        (void)entity;
        (void)position;
        (void)velocity;
    }
    CHECK( afterDestroy == 0 );
}

TEST_CASE( "Registry: adding a component the entity already has replaces it" )
{
    Rhiza::Registry registry;

    const Rhiza::Entity e = registry.createEntity();
    registry.addComponent( e, Position{ 1.0f, 1.0f } );
    registry.addComponent( e, Position{ 2.0f, 2.0f } );

    const Position *p = registry.getComponent<Position>( e );
    REQUIRE( p != nullptr );
    CHECK( p->x == 2.0f );

    int visited = 0;
    for( auto [entity, position] : registry.view<Position>() )
    {
        ++visited;
        (void)entity;
        (void)position;
    }
    CHECK( visited == 1 );

    registry.removeComponent<Position>( e );
    int afterRemove = 0;
    for( auto [entity, position] : registry.view<Position>() )
    {
        ++afterRemove;
        (void)entity;
        (void)position;
    }
    CHECK( afterRemove == 0 );
}

TEST_CASE( "Registry: onRemove runs whenever a component leaves" )
{
    Rhiza::Registry registry;

    std::vector<float> removed;
    registry.onRemove<Position>( [&]( Rhiza::Entity, Position &p ) { removed.push_back( p.x ); } );

    const Rhiza::Entity a = registry.createEntity();
    const Rhiza::Entity b = registry.createEntity();
    const Rhiza::Entity c = registry.createEntity();
    registry.addComponent( a, Position{ 1.0f, 0.0f } );
    registry.addComponent( b, Position{ 2.0f, 0.0f } );
    registry.addComponent( c, Position{ 3.0f, 0.0f } );
    registry.addComponent( c, Velocity{ 9.0f, 9.0f } );

    registry.removeComponent<Position>( a );
    REQUIRE( removed.size() == 1 );
    CHECK( removed[0] == 1.0f );

    registry.addComponent( b, Position{ 20.0f, 0.0f } );
    REQUIRE( removed.size() == 2 );
    CHECK( removed[1] == 2.0f );

    registry.destroyEntity( c );
    REQUIRE( removed.size() == 3 );
    CHECK( removed[2] == 3.0f );

    registry.removeComponent<Position>( a );
    CHECK( removed.size() == 3 );

    const Position *survivor = registry.getComponent<Position>( b );
    REQUIRE( survivor != nullptr );
    CHECK( survivor->x == 20.0f );
}

// Hidden from ctest; run with: RhizaFrameworkTests [benchmark]
TEST_CASE( "Registry: view benchmark", "[.benchmark]" )
{
    Rhiza::Registry registry;
    for( int i = 0; i < 10000; ++i )
    {
        const Rhiza::Entity e = registry.createEntity();
        registry.addComponent( e, Position{} );
        if( i % 2 == 0 )
            registry.addComponent( e, Velocity{ 1.0f, 1.0f } );
    }

    BENCHMARK( "view<Position, Velocity>, 10k entities, half matching" )
    {
        float sum = 0.0f;
        for( auto [entity, position, velocity] : registry.view<Position, Velocity>() )
        {
            position.x += velocity.dx;
            sum += position.x;
        }
        return sum;
    };
}

}  // namespace
