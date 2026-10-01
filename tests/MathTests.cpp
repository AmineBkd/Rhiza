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
    testRotate();

    if( gFailures == 0 )
    {
        std::printf( "\nAll math tests passed.\n" );
        return 0;
    }

    std::printf( "\n%d check(s) failed.\n", gFailures );
    return 1;
}
