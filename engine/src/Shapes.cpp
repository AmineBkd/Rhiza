#include <Rhiza/Shapes.h>

namespace Rhiza::Shapes
{

namespace
{

// Appends one quad as two triangles, with all four corners sharing `normal`.
// Corners must be given counter-clockwise as seen from the outside, which is
// the winding Ogre-Next treats as front-facing.
void addQuad( MeshDesc &mesh, const Vec3 &a, const Vec3 &b, const Vec3 &c, const Vec3 &d,
              const Vec3 &normal )
{
    const uint16_t base = static_cast<uint16_t>( mesh.vertices.size() );

    mesh.vertices.push_back( { a, normal } );
    mesh.vertices.push_back( { b, normal } );
    mesh.vertices.push_back( { c, normal } );
    mesh.vertices.push_back( { d, normal } );

    mesh.indices.insert( mesh.indices.end(), {
        static_cast<uint16_t>( base + 0 ), static_cast<uint16_t>( base + 1 ),
        static_cast<uint16_t>( base + 2 ), static_cast<uint16_t>( base + 2 ),
        static_cast<uint16_t>( base + 3 ), static_cast<uint16_t>( base + 0 ),
    } );
}

}  // namespace

MeshDesc cube( float size )
{
    const float h = size * 0.5f;
    MeshDesc mesh;
    mesh.vertices.reserve( 24 );
    mesh.indices.reserve( 36 );

    // +Z front
    addQuad( mesh, { -h, -h, h }, { h, -h, h }, { h, h, h }, { -h, h, h }, { 0, 0, 1 } );
    // -Z back
    addQuad( mesh, { h, -h, -h }, { -h, -h, -h }, { -h, h, -h }, { h, h, -h }, { 0, 0, -1 } );
    // +X right
    addQuad( mesh, { h, -h, h }, { h, -h, -h }, { h, h, -h }, { h, h, h }, { 1, 0, 0 } );
    // -X left
    addQuad( mesh, { -h, -h, -h }, { -h, -h, h }, { -h, h, h }, { -h, h, -h }, { -1, 0, 0 } );
    // +Y top
    addQuad( mesh, { -h, h, h }, { h, h, h }, { h, h, -h }, { -h, h, -h }, { 0, 1, 0 } );
    // -Y bottom
    addQuad( mesh, { -h, -h, -h }, { h, -h, -h }, { h, -h, h }, { -h, -h, h }, { 0, -1, 0 } );

    return mesh;
}

MeshDesc plane( float size )
{
    const float h = size * 0.5f;
    MeshDesc mesh;
    mesh.vertices.reserve( 4 );
    mesh.indices.reserve( 6 );

    addQuad( mesh, { -h, 0, h }, { h, 0, h }, { h, 0, -h }, { -h, 0, -h }, { 0, 1, 0 } );

    return mesh;
}

}  // namespace Rhiza::Shapes
