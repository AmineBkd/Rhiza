// Tests for Rhiza's math types. Pure data, so they run anywhere.

#include <catch2/catch_test_macros.hpp>

#include <Rhiza/Types/Math.h>

#include <cmath>

namespace
{

bool near( Rhiza::Vec3 a, Rhiza::Vec3 b )
{
    return std::fabs( a.x - b.x ) < 1e-5f && std::fabs( a.y - b.y ) < 1e-5f && std::fabs( a.z - b.z ) < 1e-5f;
}

// Compile-time, so this also proves the operators stay constexpr.
static_assert( Rhiza::Vec3{ 1, 2, 3 } + Rhiza::Vec3{ 4, 5, 6 } == Rhiza::Vec3{ 5, 7, 9 } );
static_assert( Rhiza::Vec2{ 1, 2 } * 2.0f == 2.0f * Rhiza::Vec2{ 1, 2 } );

TEST_CASE( "Math: Vec2 and Vec3 operators" )
{
    using Rhiza::Vec2;
    using Rhiza::Vec3;

    CHECK( Vec3{ 5, 7, 9 } - Vec3{ 4, 5, 6 } == Vec3{ 1, 2, 3 } );
    CHECK( -Vec3{ 1, -2, 3 } == Vec3{ -1, 2, -3 } );
    CHECK( Vec3{ 2, 4, 6 } / 2.0f == Vec3{ 1, 2, 3 } );

    Vec2 v{ 1, 1 };
    v += { 2, 3 };
    v *= 2.0f;
    CHECK( v == Vec2{ 6, 8 } );

    CHECK( dot( Vec3{ 1, 0, 0 }, Vec3{ 0, 1, 0 } ) == 0.0f );
    CHECK( cross( Vec3{ 1, 0, 0 }, Vec3{ 0, 1, 0 } ) == Vec3{ 0, 0, 1 } );
    CHECK( cross( Vec2{ 1, 0 }, Vec2{ 0, 1 } ) == 1.0f );
    CHECK( cross( Vec2{ 0, 1 }, Vec2{ 1, 0 } ) == -1.0f );

    CHECK( length( Vec2{ 3, 4 } ) == 5.0f );
    CHECK( near( normalized( Vec3{ 3, 4, 0 } ), { 0.6f, 0.8f, 0.0f } ) );
    CHECK( normalized( Vec3{} ) == Vec3{} );
}

TEST_CASE( "Math: Quat::rotate" )
{
    constexpr float kQuarterTurn = 1.5707963f;
    const Rhiza::Quat aboutZ = Rhiza::Quat::fromAxisAngle( { 0, 0, 1 }, kQuarterTurn );
    CHECK( near( aboutZ.rotate( { 1, 0, 0 } ), { 0, 1, 0 } ) );
    CHECK( near( aboutZ.rotate( { 0, 0, 1 } ), { 0, 0, 1 } ) );

    const Rhiza::Quat aboutY = Rhiza::Quat::fromAxisAngle( { 0, 1, 0 }, kQuarterTurn );
    CHECK( near( aboutY.rotate( { 0, 0, -1 } ), { -1, 0, 0 } ) );

    CHECK( near( Rhiza::Quat{}.rotate( { 1, 2, 3 } ), { 1, 2, 3 } ) );
}

}  // namespace
