// Tests for Rhiza::Registry (the ECS core).
//
// Pure data structure - no window, no SDL, no GPU - so like Shapes/Input/
// Clock it can be checked in a plain console process. Worth testing
// properly because a sparse set gets two things wrong easily and silently:
// swap-and-pop corrupting an unrelated entity's component on removal, and a
// stale handle aliasing onto whatever now occupies its recycled slot. Both
// would show up as "the wrong entity's data" bugs far from this file.
//
// Deliberately dependency-free rather than pulling in a test framework;
// there is not yet enough here to justify one.

#include <Rhiza/Registry.h>

#include <cstdio>
#include <string>

namespace
{

int gFailures = 0;

void check( bool condition, const std::string &what )
{
    if( !condition )
    {
        std::printf( "  FAIL: %s\n", what.c_str() );
        ++gFailures;
    }
}

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

void testCreateAndDestroy()
{
    std::printf( "create and destroy:\n" );
    Rhiza::Registry registry;

    Rhiza::Entity a = registry.createEntity();
    Rhiza::Entity b = registry.createEntity();

    check( a.isValid(), "a is valid" );
    check( b.isValid(), "b is valid" );
    check( a != b, "two createEntity() calls return different entities" );
    check( registry.isAlive( a ), "a is alive after creation" );
    check( registry.isAlive( b ), "b is alive after creation" );

    registry.destroyEntity( a );
    check( !registry.isAlive( a ), "a is not alive after destroyEntity" );
    check( registry.isAlive( b ), "destroying a does not affect b" );

    // Double-destroy must not crash or affect anything else.
    registry.destroyEntity( a );
    check( registry.isAlive( b ), "double-destroying a still leaves b alive" );
}

void testStaleHandleAfterRecycle()
{
    std::printf( "stale handle after slot recycling:\n" );
    Rhiza::Registry registry;

    Rhiza::Entity original = registry.createEntity();
    registry.addComponent( original, Position{ 1.0f, 2.0f } );
    registry.destroyEntity( original );

    // With only one entity ever created, the freed slot is the only one
    // available - createEntity() must reuse original.index, but with a
    // bumped generation.
    Rhiza::Entity recycled = registry.createEntity();
    check( recycled.index == original.index, "the freed index is reused" );
    check( recycled.generation != original.generation, "the reused slot's generation is bumped" );
    check( !registry.isAlive( original ), "the stale (pre-destroy) handle reads as not alive" );
    check( registry.isAlive( recycled ), "the new handle to the same index reads as alive" );

    // The sharpest version of the bug class this exists to catch: the old
    // handle must not read the new occupant's component just because they
    // share an index.
    registry.addComponent( recycled, Position{ 9.0f, 9.0f } );
    check( registry.getComponent<Position>( original ) == nullptr,
           "a stale handle cannot read the recycled slot's new component" );
    check( registry.getComponent<Position>( recycled ) != nullptr,
           "the live handle can read its own component" );
}

void testComponents()
{
    std::printf( "add/get/has/remove component:\n" );
    Rhiza::Registry registry;
    Rhiza::Entity e = registry.createEntity();

    check( !registry.hasComponent<Position>( e ), "no Position before it's added" );
    check( registry.getComponent<Position>( e ) == nullptr, "getComponent returns null before it's added" );

    registry.addComponent( e, Position{ 3.0f, 4.0f } );
    check( registry.hasComponent<Position>( e ), "hasComponent true after addComponent" );

    Position *p = registry.getComponent<Position>( e );
    check( p != nullptr, "getComponent non-null after addComponent" );
    if( p )
        check( p->x == 3.0f && p->y == 4.0f, "getComponent returns the value that was added" );

    registry.removeComponent<Position>( e );
    check( !registry.hasComponent<Position>( e ), "hasComponent false after removeComponent" );
    check( registry.getComponent<Position>( e ) == nullptr, "getComponent null after removeComponent" );

    // Removing a component an entity never had must be a harmless no-op.
    registry.removeComponent<Velocity>( e );
}

void testSwapAndPopDoesNotCorruptOtherEntities()
{
    std::printf( "swap-and-pop leaves other entities intact:\n" );
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

    check( !registry.hasComponent<Position>( b ), "b's component is gone" );
    Position *pa = registry.getComponent<Position>( a );
    Position *pc = registry.getComponent<Position>( c );
    check( pa != nullptr && pa->x == 1.0f, "a's component is untouched by removing b" );
    check( pc != nullptr && pc->x == 3.0f, "c's component survives the swap-and-pop with its own value" );
}

void testDestroyEntityClearsEveryComponentType()
{
    std::printf( "destroyEntity clears every component pool:\n" );
    Rhiza::Registry registry;

    Rhiza::Entity e = registry.createEntity();
    Rhiza::Entity other = registry.createEntity();
    registry.addComponent( e, Position{ 1.0f, 1.0f } );
    registry.addComponent( e, Velocity{ 0.5f, 0.5f } );
    registry.addComponent( other, Position{ 7.0f, 7.0f } );

    registry.destroyEntity( e );

    check( !registry.hasComponent<Position>( e ), "Position gone after destroyEntity" );
    check( !registry.hasComponent<Velocity>( e ), "Velocity gone after destroyEntity" );
    Position *po = registry.getComponent<Position>( other );
    check( po != nullptr && po->x == 7.0f, "an unrelated entity's component survives" );
}

void testView()
{
    std::printf( "view iterates exactly the right entities:\n" );
    Rhiza::Registry registry;

    Rhiza::Entity a = registry.createEntity();
    Rhiza::Entity b = registry.createEntity();
    Rhiza::Entity c = registry.createEntity();
    registry.addComponent( a, Position{ 1.0f, 0.0f } );
    registry.addComponent( c, Position{ 3.0f, 0.0f } );
    check( !registry.hasComponent<Position>( b ), "b deliberately has no Position" );

    int count = 0;
    float sumX = 0.0f;
    for( auto [entity, position] : registry.view<Position>() )
    {
        ++count;
        sumX += position.x;
        check( entity == a || entity == c, "view only yields entities that actually have Position" );
    }
    check( count == 2, "view visits exactly the two entities with Position" );
    check( sumX == 4.0f, "view exposes mutable references to the real component values" );

    // Mutate through the view and confirm it stuck.
    for( auto [entity, position] : registry.view<Position>() )
        position.x += 10.0f;
    Position *pa = registry.getComponent<Position>( a );
    check( pa != nullptr && pa->x == 11.0f, "mutating through a view is visible afterward" );
}

void testPairView()
{
    std::printf( "view<A, B> yields only entities holding both:\n" );
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

    check( visited == 1, "pair view yields only entities holding both" );
    check( seen == both, "pair view yields the entity holding both" );

    const Position *p = registry.getComponent<Position>( both );
    check( p != nullptr && p->x == 4.0f, "mutating through a pair view is visible afterward" );

    const Position *untouched = registry.getComponent<Position>( positionOnly );
    check( untouched != nullptr && untouched->x == 9.0f, "pair view skips half-matching entities" );

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
    check( reversed == 1, "pair view is symmetric in which type comes first" );

    registry.destroyEntity( both );
    int afterDestroy = 0;
    for( auto [entity, position, velocity] : registry.view<Position, Velocity>() )
    {
        ++afterDestroy;
        (void)entity;
        (void)position;
        (void)velocity;
    }
    check( afterDestroy == 0, "pair view is empty once the only match is destroyed" );
}

}  // namespace

int main()
{
    testCreateAndDestroy();
    testStaleHandleAfterRecycle();
    testComponents();
    testSwapAndPopDoesNotCorruptOtherEntities();
    testDestroyEntityClearsEveryComponentType();
    testView();
    testPairView();

    if( gFailures == 0 )
    {
        std::printf( "\nAll registry tests passed.\n" );
        return 0;
    }

    std::printf( "\n%d check(s) failed.\n", gFailures );
    return 1;
}
