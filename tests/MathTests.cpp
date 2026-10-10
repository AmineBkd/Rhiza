// Tests for Rhiza's math types. Pure data, so they run anywhere.

#include <Rhiza/Types/Math.h>

#include <cmath>
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

bool near( Rhiza::Vec3 a, Rhiza::Vec3 b )
{
    return std::fabs( a.x - b.x ) < 1e-5f && std::fabs( a.y - b.y ) < 1e-5f && std::fabs( a.z - b.z ) < 1e-5f;
}

// Compile-time, so this also proves the operators stay constexpr.
static_assert( Rhiza::Vec3{ 1, 2, 3 } + Rhiza::Vec3{ 4, 5, 6 } == Rhiza::Vec3{ 5, 7, 9 } );
static_assert( Rhiza::Vec2{ 1, 2 } * 2.0f == 2.0f * Rhiza::Vec2{ 1, 2 } );

void testVectors()
{
    using Rhiza::Vec2;
    using Rhiza::Vec3;
    std::printf( "Vec2/Vec3:\n" );

    check( Vec3{ 5, 7, 9 } - Vec3{ 4, 5, 6 } == Vec3{ 1, 2, 3 }, "subtracts per component" );
    check( -Vec3{ 1, -2, 3 } == Vec3{ -1, 2, -3 }, "negates per component" );
    check( Vec3{ 2, 4, 6 } / 2.0f == Vec3{ 1, 2, 3 }, "divides by a scalar" );

    Vec2 v{ 1, 1 };
    v += { 2, 3 };
    v *= 2.0f;
    check( v == Vec2{ 6, 8 }, "compound operators modify in place" );

    check( dot( Vec3{ 1, 0, 0 }, Vec3{ 0, 1, 0 } ) == 0.0f, "perpendicular vectors have a zero dot product" );
    check( cross( Vec3{ 1, 0, 0 }, Vec3{ 0, 1, 0 } ) == Vec3{ 0, 0, 1 }, "X cross Y is Z" );
    check( cross( Vec2{ 1, 0 }, Vec2{ 0, 1 } ) == 1.0f, "2D cross is positive when b is anticlockwise of a" );
    check( cross( Vec2{ 0, 1 }, Vec2{ 1, 0 } ) == -1.0f, "and negative when clockwise" );

    check( length( Vec2{ 3, 4 } ) == 5.0f, "length of a 3-4-5 triangle" );
    check( near( normalized( Vec3{ 3, 4, 0 } ), { 0.6f, 0.8f, 0.0f } ), "normalized has unit length" );
    check( normalized( Vec3{} ) == Vec3{}, "normalizing zero gives zero, not NaN" );
}

void testRotate()
{
    std::printf( "Quat::rotate:\n" );
    constexpr float kQuarterTurn = 1.5707963f;
    const Rhiza::Quat aboutZ = Rhiza::Quat::fromAxisAngle( { 0, 0, 1 }, kQuarterTurn );
    check( near( aboutZ.rotate( { 1, 0, 0 } ), { 0, 1, 0 } ), "a quarter turn about +Z takes +X to +Y" );
    check( near( aboutZ.rotate( { 0, 0, 1 } ), { 0, 0, 1 } ), "and leaves the axis alone" );

    const Rhiza::Quat aboutY = Rhiza::Quat::fromAxisAngle( { 0, 1, 0 }, kQuarterTurn );
    check( near( aboutY.rotate( { 0, 0, -1 } ), { -1, 0, 0 } ), "turning left about +Y swings forward (-Z) to -X" );

    check( near( Rhiza::Quat{}.rotate( { 1, 2, 3 } ), { 1, 2, 3 } ), "identity changes nothing" );
}

}  // namespace

int main()
{
    testVectors();
    testRotate();

    if( gFailures == 0 )
    {
        std::printf( "\nAll math tests passed.\n" );
        return 0;
    }

    std::printf( "\n%d check(s) failed.\n", gFailures );
    return 1;
}
