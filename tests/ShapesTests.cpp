// Tests for Rhiza::Shapes.
//
// Shapes is pure data - it allocates nothing on the GPU and needs no window -
// so unlike the rest of the engine it can be checked in a plain console
// process. That makes it worth testing properly: a wrong normal or a
// stray index here shows up as a subtly mis-lit or invisible surface much
// later, at a point where the renderer looks like the culprit.
//
// Deliberately dependency-free rather than pulling in a test framework;
// there is not yet enough here to justify one.

#include <Rhiza/Shapes.h>

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

// Every index must land inside the vertex list. An out-of-range index is
// undefined behaviour on the GPU rather than a clean error, so it is the
// single most valuable thing to assert.
void checkIndicesInRange( const Rhiza::MeshDesc &mesh, const std::string &name )
{
    for( size_t i = 0; i < mesh.indices.size(); ++i )
    {
        if( mesh.indices[i] >= mesh.vertices.size() )
        {
            check( false, name + ": index " + std::to_string( i ) + " is " +
                              std::to_string( mesh.indices[i] ) + ", past " +
                              std::to_string( mesh.vertices.size() ) + " vertices" );
            return;
        }
    }
}

// A normal that is not unit length makes lighting too bright or too dim; a
// zero normal makes the surface black under every light.
void checkNormalsAreUnitLength( const Rhiza::MeshDesc &mesh, const std::string &name )
{
    for( size_t i = 0; i < mesh.vertices.size(); ++i )
    {
        const Rhiza::Vec3 &n = mesh.vertices[i].normal;
        const float length = std::sqrt( n.x * n.x + n.y * n.y + n.z * n.z );
        if( std::fabs( length - 1.0f ) > 1e-5f )
        {
            check( false, name + ": normal " + std::to_string( i ) + " has length " +
                              std::to_string( length ) + ", expected 1" );
            return;
        }
    }
}

void checkIsTriangleList( const Rhiza::MeshDesc &mesh, const std::string &name )
{
    check( !mesh.vertices.empty(), name + ": has vertices" );
    check( !mesh.indices.empty(), name + ": has indices" );
    check( mesh.indices.size() % 3 == 0, name + ": index count is a multiple of 3" );
}

void testCube()
{
    std::printf( "cube:\n" );
    const Rhiza::MeshDesc cube = Rhiza::Shapes::cube( 2.0f );

    checkIsTriangleList( cube, "cube" );
    checkIndicesInRange( cube, "cube" );
    checkNormalsAreUnitLength( cube, "cube" );

    // 6 faces x 4 corners. Corners are not shared between faces because each
    // face needs its own normal - see Shapes.h.
    check( cube.vertices.size() == 24, "cube: has 24 vertices, not 8" );
    check( cube.indices.size() == 36, "cube: has 36 indices (6 faces x 2 triangles)" );

    // size = 2 should span -1..+1 on every axis.
    for( const Rhiza::Vertex &v : cube.vertices )
    {
        const bool inBounds = std::fabs( std::fabs( v.position.x ) - 1.0f ) < 1e-5f &&
                              std::fabs( std::fabs( v.position.y ) - 1.0f ) < 1e-5f &&
                              std::fabs( std::fabs( v.position.z ) - 1.0f ) < 1e-5f;
        if( !inBounds )
        {
            check( false, "cube: a corner of cube(2.0) is not at +/-1 on all axes" );
            break;
        }
    }

    // All six axis directions should appear as face normals, exactly 4
    // vertices each. This catches a duplicated or flipped face.
    const Rhiza::Vec3 expected[6] = { { 1, 0, 0 },  { -1, 0, 0 }, { 0, 1, 0 },
                                      { 0, -1, 0 }, { 0, 0, 1 },  { 0, 0, -1 } };
    for( const Rhiza::Vec3 &want : expected )
    {
        int matches = 0;
        for( const Rhiza::Vertex &v : cube.vertices )
        {
            if( std::fabs( v.normal.x - want.x ) < 1e-5f &&
                std::fabs( v.normal.y - want.y ) < 1e-5f &&
                std::fabs( v.normal.z - want.z ) < 1e-5f )
            {
                ++matches;
            }
        }
        check( matches == 4, "cube: face normal (" + std::to_string( (int)want.x ) + "," +
                                 std::to_string( (int)want.y ) + "," +
                                 std::to_string( (int)want.z ) + ") is used by exactly 4 vertices" );
    }
}

void testPlane()
{
    std::printf( "plane:\n" );
    const Rhiza::MeshDesc plane = Rhiza::Shapes::plane( 4.0f );

    checkIsTriangleList( plane, "plane" );
    checkIndicesInRange( plane, "plane" );
    checkNormalsAreUnitLength( plane, "plane" );

    check( plane.vertices.size() == 4, "plane: has 4 vertices" );
    check( plane.indices.size() == 6, "plane: has 6 indices (2 triangles)" );

    for( const Rhiza::Vertex &v : plane.vertices )
    {
        check( std::fabs( v.position.y ) < 1e-5f, "plane: lies flat in the XZ plane" );
        check( std::fabs( v.normal.y - 1.0f ) < 1e-5f, "plane: faces up" );
    }
}

void testScaling()
{
    std::printf( "scaling:\n" );

    // A size of 0 is degenerate but must not produce indices that point
    // nowhere - the renderer would happily upload it.
    const Rhiza::MeshDesc degenerate = Rhiza::Shapes::cube( 0.0f );
    checkIndicesInRange( degenerate, "cube(0)" );
    check( degenerate.vertices.size() == 24, "cube(0): still well-formed" );

    const Rhiza::MeshDesc big = Rhiza::Shapes::cube( 10.0f );
    check( std::fabs( std::fabs( big.vertices[0].position.x ) - 5.0f ) < 1e-5f,
           "cube(10): spans +/-5" );
}

}  // namespace

int main()
{
    testCube();
    testPlane();
    testScaling();

    if( gFailures == 0 )
    {
        std::printf( "\nAll shape tests passed.\n" );
        return 0;
    }

    std::printf( "\n%d check(s) failed.\n", gFailures );
    return 1;
}
