// Tests for Rhiza::Shapes.
//
// Shapes is pure data - it allocates nothing on the GPU and needs no window -
// so unlike the rest of the engine it can be checked in a plain console
// process. That makes it worth testing properly: a wrong normal or a
// stray index here shows up as a subtly mis-lit or invisible surface much
// later, at a point where the renderer looks like the culprit.

#include <catch2/catch_test_macros.hpp>

#include <Rhiza/Shapes.h>

#include <cmath>
#include <string>

namespace
{

// Every index must land inside the vertex list. An out-of-range index is
// undefined behaviour on the GPU rather than a clean error, so it is the
// single most valuable thing to assert.
void checkIndicesInRange( const Rhiza::MeshDesc &mesh, const std::string &name )
{
    for( size_t i = 0; i < mesh.indices.size(); ++i )
    {
        if( mesh.indices[i] >= mesh.vertices.size() )
        {
            FAIL_CHECK( name + ": index " + std::to_string( i ) + " is " + std::to_string( mesh.indices[i] ) +
                        ", past " + std::to_string( mesh.vertices.size() ) + " vertices" );
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
            FAIL_CHECK( name + ": normal " + std::to_string( i ) + " has length " + std::to_string( length ) +
                        ", expected 1" );
            return;
        }
    }
}

void checkIsTriangleList( const Rhiza::MeshDesc &mesh, const std::string &name )
{
    INFO( name );
    CHECK( !mesh.vertices.empty() );
    CHECK( !mesh.indices.empty() );
    CHECK( mesh.indices.size() % 3 == 0 );
}

TEST_CASE( "Shapes: cube" )
{
    const Rhiza::MeshDesc cube = Rhiza::Shapes::cube( 2.0f );

    checkIsTriangleList( cube, "cube" );
    checkIndicesInRange( cube, "cube" );
    checkNormalsAreUnitLength( cube, "cube" );

    // 6 faces x 4 corners. Corners are not shared between faces because each
    // face needs its own normal - see Shapes.h.
    CHECK( cube.vertices.size() == 24 );
    CHECK( cube.indices.size() == 36 );

    // size = 2 should span -1..+1 on every axis.
    for( const Rhiza::Vertex &v : cube.vertices )
    {
        const bool inBounds = std::fabs( std::fabs( v.position.x ) - 1.0f ) < 1e-5f &&
                              std::fabs( std::fabs( v.position.y ) - 1.0f ) < 1e-5f &&
                              std::fabs( std::fabs( v.position.z ) - 1.0f ) < 1e-5f;
        if( !inBounds )
        {
            FAIL_CHECK( "cube: a corner of cube(2.0) is not at +/-1 on all axes" );
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
            if( std::fabs( v.normal.x - want.x ) < 1e-5f && std::fabs( v.normal.y - want.y ) < 1e-5f &&
                std::fabs( v.normal.z - want.z ) < 1e-5f )
            {
                ++matches;
            }
        }
        CAPTURE( want.x, want.y, want.z );
        CHECK( matches == 4 );
    }
}

TEST_CASE( "Shapes: plane" )
{
    const Rhiza::MeshDesc plane = Rhiza::Shapes::plane( 4.0f );

    checkIsTriangleList( plane, "plane" );
    checkIndicesInRange( plane, "plane" );
    checkNormalsAreUnitLength( plane, "plane" );

    CHECK( plane.vertices.size() == 4 );
    CHECK( plane.indices.size() == 6 );

    for( const Rhiza::Vertex &v : plane.vertices )
    {
        CHECK( std::fabs( v.position.y ) < 1e-5f );
        CHECK( std::fabs( v.normal.y - 1.0f ) < 1e-5f );
    }
}

TEST_CASE( "Shapes: quad" )
{
    const Rhiza::MeshDesc quad = Rhiza::Shapes::quad( 2.0f, 4.0f );

    checkIsTriangleList( quad, "quad" );
    checkIndicesInRange( quad, "quad" );
    checkNormalsAreUnitLength( quad, "quad" );

    CHECK( quad.vertices.size() == 4 );

    for( const Rhiza::Vertex &v : quad.vertices )
    {
        CHECK( std::fabs( v.position.z ) < 1e-5f );
        CHECK( std::fabs( v.normal.z - 1.0f ) < 1e-5f );
        CHECK( std::fabs( std::fabs( v.position.x ) - 1.0f ) < 1e-5f );
        CHECK( std::fabs( std::fabs( v.position.y ) - 2.0f ) < 1e-5f );

        // A texture must land upright: the top edge samples the image's top
        // row (v = 0), the left edge its left column (u = 0).
        CHECK( ( v.position.y > 0.0f ) == ( v.uv.y == 0.0f ) );
        CHECK( ( v.position.x < 0.0f ) == ( v.uv.x == 0.0f ) );
    }
}

TEST_CASE( "Shapes: scaling" )
{
    // A size of 0 is degenerate but must not produce indices that point
    // nowhere - the renderer would happily upload it.
    const Rhiza::MeshDesc degenerate = Rhiza::Shapes::cube( 0.0f );
    checkIndicesInRange( degenerate, "cube(0)" );
    CHECK( degenerate.vertices.size() == 24 );

    const Rhiza::MeshDesc big = Rhiza::Shapes::cube( 10.0f );
    CHECK( std::fabs( std::fabs( big.vertices[0].position.x ) - 5.0f ) < 1e-5f );
}

}  // namespace
