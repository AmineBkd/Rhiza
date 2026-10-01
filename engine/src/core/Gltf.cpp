#include "Gltf.h"

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <utility>

#define CGLTF_IMPLEMENTATION
#include <cgltf.h>

namespace Rhiza
{

namespace
{

constexpr size_t kMaxVertices = 65536;  // what MeshDesc's 16-bit indices can address

Vec3 add( Vec3 a, Vec3 b ) { return { a.x + b.x, a.y + b.y, a.z + b.z }; }
Vec3 subtract( Vec3 a, Vec3 b ) { return { a.x - b.x, a.y - b.y, a.z - b.z }; }
Vec3 scale( Vec3 v, float s ) { return { v.x * s, v.y * s, v.z * s }; }
float dot( Vec3 a, Vec3 b ) { return a.x * b.x + a.y * b.y + a.z * b.z; }

Vec3 cross( Vec3 a, Vec3 b )
{
    return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x };
}

Vec3 normalized( Vec3 v )
{
    const float length = std::sqrt( dot( v, v ) );
    return length > 0.0f ? scale( v, 1.0f / length ) : v;
}

// glTF matrices are column-major.
struct NodeTransform
{
    float matrix[16];

    // Normals need the inverse transpose of the upper 3x3, or non-uniform
    // scale tilts them. These cofactor columns are that matrix times the
    // determinant; normalising removes the scale, `mirrored` restores the sign.
    Vec3 normalColumns[3];
    bool mirrored;
};

NodeTransform makeNodeTransform( const float *matrix )
{
    NodeTransform transform;
    std::memcpy( transform.matrix, matrix, sizeof( transform.matrix ) );

    const Vec3 a{ matrix[0], matrix[1], matrix[2] };
    const Vec3 b{ matrix[4], matrix[5], matrix[6] };
    const Vec3 c{ matrix[8], matrix[9], matrix[10] };
    transform.normalColumns[0] = cross( b, c );
    transform.normalColumns[1] = cross( c, a );
    transform.normalColumns[2] = cross( a, b );
    transform.mirrored = dot( a, cross( b, c ) ) < 0.0f;
    return transform;
}

Vec3 transformPoint( const NodeTransform &t, Vec3 p )
{
    const float *m = t.matrix;
    return { m[0] * p.x + m[4] * p.y + m[8] * p.z + m[12],
             m[1] * p.x + m[5] * p.y + m[9] * p.z + m[13],
             m[2] * p.x + m[6] * p.y + m[10] * p.z + m[14] };
}

Vec3 transformNormal( const NodeTransform &t, Vec3 n )
{
    const Vec3 r = add( add( scale( t.normalColumns[0], n.x ), scale( t.normalColumns[1], n.y ) ),
                        scale( t.normalColumns[2], n.z ) );
    return normalized( t.mirrored ? scale( r, -1.0f ) : r );
}

// Area-weighted: the unnormalised cross product is twice the face's area.
void generateSmoothNormals( MeshDesc &mesh, size_t firstVertex, size_t firstIndex )
{
    for( size_t i = firstIndex; i + 2 < mesh.indices.size(); i += 3 )
    {
        Vertex &a = mesh.vertices[mesh.indices[i]];
        Vertex &b = mesh.vertices[mesh.indices[i + 1]];
        Vertex &c = mesh.vertices[mesh.indices[i + 2]];
        const Vec3 face = cross( subtract( b.position, a.position ), subtract( c.position, a.position ) );
        a.normal = add( a.normal, face );
        b.normal = add( b.normal, face );
        c.normal = add( c.normal, face );
    }
    for( size_t i = firstVertex; i < mesh.vertices.size(); ++i )
        mesh.vertices[i].normal = normalized( mesh.vertices[i].normal );
}

bool appendPrimitive( const cgltf_primitive &primitive, const NodeTransform &transform, MeshDesc &out,
                      std::string &error )
{
    if( primitive.type != cgltf_primitive_type_triangles )
        return true;

    if( primitive.has_draco_mesh_compression )
    {
        error = "it uses Draco compression, which is not supported";
        return false;
    }

    const cgltf_accessor *positions = cgltf_find_accessor( &primitive, cgltf_attribute_type_position, 0 );
    if( !positions )
    {
        error = "a primitive has no POSITION attribute";
        return false;
    }
    const cgltf_accessor *normals = cgltf_find_accessor( &primitive, cgltf_attribute_type_normal, 0 );
    const cgltf_accessor *uvs = cgltf_find_accessor( &primitive, cgltf_attribute_type_texcoord, 0 );

    const size_t firstVertex = out.vertices.size();
    const size_t vertexCount = positions->count;
    if( firstVertex + vertexCount > kMaxVertices )
    {
        error = "it has more than " + std::to_string( kMaxVertices ) +
                " vertices, which 16-bit indices cannot address";
        return false;
    }

    for( size_t i = 0; i < vertexCount; ++i )
    {
        float position[3] = {};
        float normal[3] = {};
        float uv[2] = {};
        cgltf_accessor_read_float( positions, i, position, 3 );
        if( normals )
            cgltf_accessor_read_float( normals, i, normal, 3 );
        if( uvs )
            cgltf_accessor_read_float( uvs, i, uv, 2 );

        Vertex vertex;
        vertex.position = transformPoint( transform, { position[0], position[1], position[2] } );
        if( normals )
            vertex.normal = transformNormal( transform, { normal[0], normal[1], normal[2] } );
        vertex.uv = { uv[0], uv[1] };
        out.vertices.push_back( vertex );
    }

    const size_t firstIndex = out.indices.size();
    const size_t indexCount = primitive.indices ? primitive.indices->count : vertexCount;
    if( indexCount % 3u != 0u )
    {
        error = "a triangle primitive has " + std::to_string( indexCount ) + " indices, not a multiple of 3";
        return false;
    }
    for( size_t i = 0; i < indexCount; ++i )
    {
        const size_t local = primitive.indices ? cgltf_accessor_read_index( primitive.indices, i ) : i;
        if( local >= vertexCount )
        {
            error = "an index points past the end of its primitive's vertices";
            return false;
        }
        out.indices.push_back( static_cast<uint16_t>( firstVertex + local ) );
    }

    // A mirroring transform turns counter-clockwise triangles clockwise,
    // which would swap which side counts as the front.
    if( transform.mirrored )
    {
        for( size_t i = firstIndex; i < out.indices.size(); i += 3 )
            std::swap( out.indices[i + 1], out.indices[i + 2] );
    }

    if( !normals )
        generateSmoothNormals( out, firstVertex, firstIndex );
    return true;
}

bool appendNode( const cgltf_node &node, MeshDesc &out, std::string &error )
{
    if( node.mesh )
    {
        float world[16];
        cgltf_node_transform_world( &node, world );
        const NodeTransform transform = makeNodeTransform( world );
        for( size_t i = 0; i < node.mesh->primitives_count; ++i )
        {
            if( !appendPrimitive( node.mesh->primitives[i], transform, out, error ) )
                return false;
        }
    }
    for( size_t i = 0; i < node.children_count; ++i )
    {
        if( !appendNode( *node.children[i], out, error ) )
            return false;
    }
    return true;
}

// Buffers in separate files go through the caller's reader rather than
// cgltf's fopen, so they load from wherever the .gltf did - an APK included.
cgltf_result readThroughFileReader( const cgltf_memory_options *, const cgltf_file_options *fileOptions,
                                    const char *path, cgltf_size *size, void **data )
{
    const FileReader &readFile = *static_cast<const FileReader *>( fileOptions->user_data );
    std::vector<uint8_t> bytes;
    if( !readFile( path, bytes ) )
        return cgltf_result_file_not_found;

    void *copy = std::malloc( bytes.empty() ? 1 : bytes.size() );
    if( !copy )
        return cgltf_result_out_of_memory;
    if( !bytes.empty() )
        std::memcpy( copy, bytes.data(), bytes.size() );

    *size = bytes.size();
    *data = copy;
    return cgltf_result_success;
}

void releaseFileReaderData( const cgltf_memory_options *, const cgltf_file_options *, void *data )
{
    std::free( data );
}

struct CgltfDataDeleter
{
    void operator()( cgltf_data *data ) const { cgltf_free( data ); }
};

}  // namespace

bool parseGltfMesh( const std::vector<uint8_t> &bytes, const std::string &path,
                    const FileReader &readFile, MeshDesc &out, std::string &error )
{
    cgltf_options options = {};
    options.file.read = &readThroughFileReader;
    options.file.release = &releaseFileReaderData;
    options.file.user_data = const_cast<FileReader *>( &readFile );

    cgltf_data *parsed = nullptr;
    if( cgltf_parse( &options, bytes.data(), bytes.size(), &parsed ) != cgltf_result_success )
    {
        error = "it is not a valid glTF or GLB file";
        return false;
    }
    std::unique_ptr<cgltf_data, CgltfDataDeleter> data( parsed );

    if( cgltf_load_buffers( &options, data.get(), path.c_str() ) != cgltf_result_success )
    {
        error = "its buffers could not be loaded";
        return false;
    }
    if( cgltf_validate( data.get() ) != cgltf_result_success )
    {
        error = "it failed glTF validation";
        return false;
    }

    MeshDesc mesh;
    const cgltf_scene *scene = data->scene ? data->scene : ( data->scenes_count > 0 ? &data->scenes[0] : nullptr );
    if( scene )
    {
        for( size_t i = 0; i < scene->nodes_count; ++i )
        {
            if( !appendNode( *scene->nodes[i], mesh, error ) )
                return false;
        }
    }
    else
    {
        // No scene means no transforms either: take the meshes as authored.
        const float identity[16] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
        const NodeTransform transform = makeNodeTransform( identity );
        for( size_t m = 0; m < data->meshes_count; ++m )
        {
            for( size_t p = 0; p < data->meshes[m].primitives_count; ++p )
            {
                if( !appendPrimitive( data->meshes[m].primitives[p], transform, mesh, error ) )
                    return false;
            }
        }
    }

    if( mesh.vertices.empty() )
    {
        error = "it contains no triangle geometry";
        return false;
    }

    out = std::move( mesh );
    return true;
}

}  // namespace Rhiza
