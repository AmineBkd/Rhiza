// Tests for asset keys, the cache's bookkeeping and glTF mesh parsing. No
// GPU or window, so they run anywhere, CI included; only the case-mismatch
// check touches the file system, in a temp folder of its own.

#include "core/AssetCache.h"
#include "core/AssetPath.h"
#include "core/Gltf.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>

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

bool near( float a, float b ) { return std::fabs( a - b ) < 1e-4f; }

std::string base64( const std::vector<uint8_t> &bytes )
{
    static const char *table = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    for( size_t i = 0; i < bytes.size(); i += 3 )
    {
        const uint32_t chunk = ( bytes[i] << 16 ) | ( i + 1 < bytes.size() ? bytes[i + 1] << 8 : 0 ) |
                               ( i + 2 < bytes.size() ? bytes[i + 2] : 0 );
        out += table[( chunk >> 18 ) & 63];
        out += table[( chunk >> 12 ) & 63];
        out += i + 1 < bytes.size() ? table[( chunk >> 6 ) & 63] : '=';
        out += i + 2 < bytes.size() ? table[chunk & 63] : '=';
    }
    return out;
}

void appendFloats( std::vector<uint8_t> &out, std::initializer_list<float> values )
{
    for( float v : values )
    {
        uint8_t raw[4];
        std::memcpy( raw, &v, 4 );
        out.insert( out.end(), raw, raw + 4 );
    }
}

// One triangle, (0,0,0) (1,0,0) (0,1,0): counter-clockwise seen from +Z.
struct Triangle
{
    std::string nodeTransform;        // e.g. "\"translation\":[5,0,0]"
    const float *normal = nullptr;    // same normal on all three vertices
    bool indexed = true;
    int mode = 4;                     // 4 = triangles, 0 = points

    std::vector<uint8_t> binary() const
    {
        std::vector<uint8_t> out;
        appendFloats( out, { 0, 0, 0, 1, 0, 0, 0, 1, 0 } );
        if( normal )
            for( int i = 0; i < 3; ++i )
                appendFloats( out, { normal[0], normal[1], normal[2] } );
        if( indexed )
            out.insert( out.end(), { 0, 0, 1, 0, 2, 0, 0, 0 } );  // uint16 0,1,2 + padding
        return out;
    }

    // `bufferUri` empty means the buffer lives in a GLB's binary chunk.
    std::string json( const std::string &bufferUri ) const
    {
        const size_t normalOffset = 36;
        const size_t indexOffset = normal ? 72 : 36;
        std::string attributes = "\"POSITION\":0";
        std::string views = "{\"buffer\":0,\"byteOffset\":0,\"byteLength\":36}";
        std::string accessors =
            "{\"bufferView\":0,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\","
            "\"min\":[0,0,0],\"max\":[1,1,0]}";
        int nextAccessor = 1;
        if( normal )
        {
            attributes += ",\"NORMAL\":1";
            views += ",{\"buffer\":0,\"byteOffset\":" + std::to_string( normalOffset ) + ",\"byteLength\":36}";
            accessors += ",{\"bufferView\":1,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\"}";
            nextAccessor = 2;
        }
        std::string primitive = "{\"attributes\":{" + attributes + "},\"mode\":" + std::to_string( mode );
        if( indexed )
        {
            const std::string view = std::to_string( nextAccessor );
            views += ",{\"buffer\":0,\"byteOffset\":" + std::to_string( indexOffset ) + ",\"byteLength\":6}";
            accessors += ",{\"bufferView\":" + view + ",\"componentType\":5123,\"count\":3,\"type\":\"SCALAR\"}";
            primitive += ",\"indices\":" + view;
        }
        primitive += "}";

        std::string buffer = "{\"byteLength\":" + std::to_string( binary().size() );
        if( !bufferUri.empty() )
            buffer += ",\"uri\":\"" + bufferUri + "\"";
        buffer += "}";

        std::string node = "{\"mesh\":0";
        if( !nodeTransform.empty() )
            node += "," + nodeTransform;
        node += "}";

        return "{\"asset\":{\"version\":\"2.0\"},\"scene\":0,\"scenes\":[{\"nodes\":[0]}],"
               "\"nodes\":[" + node + "],\"meshes\":[{\"primitives\":[" + primitive + "]}],"
               "\"buffers\":[" + buffer + "],\"bufferViews\":[" + views + "],"
               "\"accessors\":[" + accessors + "]}";
    }

    std::vector<uint8_t> embeddedGltf() const
    {
        const std::string text = json( "data:application/octet-stream;base64," + base64( binary() ) );
        return std::vector<uint8_t>( text.begin(), text.end() );
    }

    std::vector<uint8_t> glb() const
    {
        std::string text = json( "" );
        while( text.size() % 4 )
            text += ' ';
        const std::vector<uint8_t> bin = binary();

        std::vector<uint8_t> out;
        auto u32 = [&out]( uint32_t v ) {
            for( int i = 0; i < 4; ++i )
                out.push_back( static_cast<uint8_t>( v >> ( 8 * i ) ) );
        };
        u32( 0x46546C67 );  // "glTF"
        u32( 2 );
        u32( static_cast<uint32_t>( 12 + 8 + text.size() + 8 + bin.size() ) );
        u32( static_cast<uint32_t>( text.size() ) );
        u32( 0x4E4F534A );  // "JSON"
        out.insert( out.end(), text.begin(), text.end() );
        u32( static_cast<uint32_t>( bin.size() ) );
        u32( 0x004E4942 );  // "BIN\0"
        out.insert( out.end(), bin.begin(), bin.end() );
        return out;
    }
};

const Rhiza::FileReader kNoFiles = []( const std::string &, std::vector<uint8_t> & ) { return false; };

bool parse( const std::vector<uint8_t> &bytes, Rhiza::MeshDesc &mesh, std::string &error,
            const Rhiza::FileReader &reader = kNoFiles, const std::string &path = "test.gltf" )
{
    return Rhiza::parseGltfMesh( bytes, path, reader, mesh, error );
}

void testNormalizeAssetPath()
{
    std::printf( "normalizeAssetPath:\n" );
    check( Rhiza::normalizeAssetPath( "ship.png" ) == "ship.png", "plain name unchanged" );
    check( Rhiza::normalizeAssetPath( "./ship.png" ) == "ship.png", "leading ./ removed" );
    check( Rhiza::normalizeAssetPath( "textures//ship.png" ) == "textures/ship.png", "doubled slash collapsed" );
    check( Rhiza::normalizeAssetPath( "textures\\ship.png" ) == "textures/ship.png", "backslash becomes slash" );
    check( Rhiza::normalizeAssetPath( "a/b/../ship.png" ) == "a/ship.png", ".. resolved" );
    check( Rhiza::normalizeAssetPath( "Ship.png" ) == "Ship.png", "case kept" );
}

void testJoinAssetPath()
{
    std::printf( "joinAssetPath:\n" );
    check( Rhiza::joinAssetPath( "", "a.png" ) == "a.png", "empty root leaves path relative" );
    check( Rhiza::joinAssetPath( "root", "a.png" ) == "root/a.png", "separator added" );
    check( Rhiza::joinAssetPath( "root/", "a.png" ) == "root/a.png", "no doubled separator" );
    check( Rhiza::joinAssetPath( "C:\\game\\", "a.png" ) == "C:\\game\\a.png", "Windows root kept" );
}

void testCacheReturnsTheSameHandle()
{
    std::printf( "cache: same key, same handle:\n" );
    Rhiza::AssetCache cache;
    check( cache.acquire( "rock.glb" ) == 0, "nothing cached yet" );

    cache.add( "rock.glb", 7 );
    check( cache.acquire( "rock.glb" ) == 7, "second load gets the first load's handle" );
    check( cache.acquire( "rock.glb" ) == 7, "and so does a third" );
    check( cache.loadCount( 7 ) == 3, "three loads counted" );

    check( cache.acquire( "ship.png" ) == 0, "a different key is not a hit" );
    cache.add( "ship.png", 8 );
    check( cache.acquire( "ship.png" ) == 8 && cache.loadCount( 7 ) == 3, "keys stay independent" );
}

void testCacheFreesOnTheLastRelease()
{
    std::printf( "cache: freed by the last release only:\n" );
    Rhiza::AssetCache cache;
    cache.add( "rock.glb", 7 );
    cache.acquire( "rock.glb" );
    cache.acquire( "rock.glb" );

    check( cache.releaseIfShared( 7 ), "first release: others still hold it" );
    check( cache.releaseIfShared( 7 ), "second release: one still holds it" );
    check( cache.loadCount( 7 ) == 1, "one load left" );
    check( !cache.releaseIfShared( 7 ), "last release tells the caller to free it" );
    check( cache.loadCount( 7 ) == 1, "and does not count below one by itself" );

    cache.forget( 7 );
    check( cache.acquire( "rock.glb" ) == 0, "forgotten: the next load must load again" );
    check( cache.loadCount( 7 ) == 0, "nothing counted for the old handle" );

    cache.add( "rock.glb", 12 );
    check( cache.acquire( "rock.glb" ) == 12, "reloaded under a new handle" );
}

void testCacheIgnoresUnknownHandles()
{
    std::printf( "cache: handles it never saw:\n" );
    Rhiza::AssetCache cache;
    check( !cache.releaseIfShared( 99 ), "uncached handle: caller frees it as usual" );
    cache.forget( 99 );
    cache.add( "a", 1 );
    cache.forget( 2 );
    check( cache.acquire( "a" ) == 1, "forgetting an unknown handle leaves others alone" );

    cache.clear();
    check( cache.acquire( "a" ) == 0, "clear drops everything" );
}

void testSameNameInDifferentFolders()
{
    std::printf( "cache: same file name in different folders:\n" );
    const std::string ships = Rhiza::normalizeAssetPath( "ships/hull.png" );
    const std::string stations = Rhiza::normalizeAssetPath( "stations/hull.png" );
    check( ships != stations, "keys keep the folder" );

    Rhiza::AssetCache cache;
    cache.add( ships, 1 );
    check( cache.acquire( stations ) == 0, "the other folder's hull.png is not a hit" );
    cache.add( stations, 2 );
    check( cache.acquire( ships ) == 1 && cache.acquire( stations ) == 2, "both cached side by side" );

    const Rhiza::TextureDesc linear;
    check( Rhiza::textureCacheKey( ships, linear ) != Rhiza::textureCacheKey( stations, linear ),
           "texture keys differ too" );
}

// Compares names rather than asking the file system, so it reports the same
// on case-insensitive Windows/macOS and case-sensitive Linux.
void testFindCaseMismatch()
{
    std::printf( "case mismatch against the names on disk:\n" );
    namespace fs = std::filesystem;
    const fs::path root = fs::temp_directory_path() / "rhiza_asset_tests";
    std::error_code error;
    fs::remove_all( root, error );
    fs::create_directories( root / "Textures", error );
    std::ofstream( root / "Textures" / "Ship.png" ).put( 'x' );
    const std::string rootText = root.string();

    check( Rhiza::findCaseMismatch( rootText, "Textures/Ship.png" ).empty(), "exact spelling: no warning" );
    check( Rhiza::findCaseMismatch( rootText, "textures/ship.png" ) == "Textures/Ship.png",
           "wrong case reports the spelling on disk" );
    check( Rhiza::findCaseMismatch( rootText, "Textures/ship.png" ) == "Textures/Ship.png",
           "a mismatch in the file name alone is caught" );
    check( Rhiza::findCaseMismatch( rootText, "Textures/missing.png" ).empty(), "missing file: no warning" );
    check( Rhiza::findCaseMismatch( ( root / "nowhere" ).string(), "Ship.png" ).empty(),
           "root that is not a folder: no warning" );

    fs::remove_all( root, error );
}

void testTextureCacheKey()
{
    std::printf( "texture keys: settings are part of identity:\n" );
    const Rhiza::TextureDesc linear;
    Rhiza::TextureDesc nearest;
    nearest.filter = Rhiza::TextureFilter::Nearest;
    Rhiza::TextureDesc repeating;
    repeating.wrap = Rhiza::TextureWrap::Repeat;

    check( Rhiza::textureCacheKey( "ship.png", linear ) == Rhiza::textureCacheKey( "ship.png", linear ),
           "same file, same settings: same key" );
    check( Rhiza::textureCacheKey( "ship.png", linear ) != Rhiza::textureCacheKey( "ship.png", nearest ),
           "different filter: different key" );
    check( Rhiza::textureCacheKey( "ship.png", linear ) != Rhiza::textureCacheKey( "ship.png", repeating ),
           "different wrap: different key" );
    check( Rhiza::textureCacheKey( Rhiza::normalizeAssetPath( "./textures//ship.png" ), linear ) ==
               Rhiza::textureCacheKey( Rhiza::normalizeAssetPath( "textures/ship.png" ), linear ),
           "two spellings of one path: same key" );
}

void testTriangle()
{
    std::printf( "embedded triangle:\n" );
    Rhiza::MeshDesc mesh;
    std::string error;
    check( parse( Triangle{}.embeddedGltf(), mesh, error ), "parses: " + error );
    check( mesh.vertices.size() == 3 && mesh.indices.size() == 3, "3 vertices, 3 indices" );
    if( mesh.vertices.size() != 3 )
        return;
    check( near( mesh.vertices[1].position.x, 1.0f ), "positions read" );
    check( near( mesh.vertices[0].normal.z, 1.0f ), "missing normals generated, facing +Z" );
    check( mesh.indices[0] == 0 && mesh.indices[1] == 1 && mesh.indices[2] == 2, "indices kept in order" );
}

void testNodeTranslation()
{
    std::printf( "node transform baked:\n" );
    Triangle triangle;
    triangle.nodeTransform = "\"translation\":[5,0,0]";
    Rhiza::MeshDesc mesh;
    std::string error;
    check( parse( triangle.embeddedGltf(), mesh, error ), "parses: " + error );
    check( !mesh.vertices.empty() && near( mesh.vertices[0].position.x, 5.0f ), "translation applied" );
}

void testMirroring()
{
    std::printf( "mirroring keeps the front face:\n" );
    Triangle triangle;
    triangle.nodeTransform = "\"scale\":[-1,1,1]";
    Rhiza::MeshDesc mesh;
    std::string error;
    check( parse( triangle.embeddedGltf(), mesh, error ), "parses: " + error );
    check( mesh.indices.size() == 3 && mesh.indices[1] == 2 && mesh.indices[2] == 1, "winding flipped" );
    check( !mesh.vertices.empty() && near( mesh.vertices[0].normal.z, 1.0f ), "still faces +Z" );
}

void testNormalTransform()
{
    std::printf( "authored normals follow the transform:\n" );
    const float plusX[3] = { 1, 0, 0 };
    Triangle rotated;
    rotated.normal = plusX;
    rotated.nodeTransform = "\"rotation\":[0,0,0.7071068,0.7071068]";  // 90 degrees about +Z
    Rhiza::MeshDesc mesh;
    std::string error;
    check( parse( rotated.embeddedGltf(), mesh, error ), "rotated parses: " + error );
    check( !mesh.vertices.empty() && near( mesh.vertices[0].normal.y, 1.0f ), "+X normal rotated to +Y" );

    // Stretching along X tilts a diagonal normal toward Y, not toward X.
    const float diagonal[3] = { 0.7071068f, 0.7071068f, 0 };
    Triangle stretched;
    stretched.normal = diagonal;
    stretched.nodeTransform = "\"scale\":[2,1,1]";
    check( parse( stretched.embeddedGltf(), mesh, error ), "stretched parses: " + error );
    if( mesh.vertices.empty() )
        return;
    const Rhiza::Vec3 n = mesh.vertices[0].normal;
    check( near( n.x, 0.4472136f ) && near( n.y, 0.8944272f ), "inverse-transpose applied" );
}

void testExternalBuffer()
{
    std::printf( "external buffer goes through the reader:\n" );
    Triangle triangle;
    const std::string text = triangle.json( "tri.bin" );
    std::string requested;
    const Rhiza::FileReader reader = [&]( const std::string &path, std::vector<uint8_t> &out ) {
        requested = path;
        out = triangle.binary();
        return true;
    };
    Rhiza::MeshDesc mesh;
    std::string error;
    check( parse( std::vector<uint8_t>( text.begin(), text.end() ), mesh, error, reader, "models/tri.gltf" ),
           "parses: " + error );
    check( requested == "models/tri.bin", "buffer looked up beside the .gltf, got '" + requested + "'" );
    check( mesh.vertices.size() == 3, "geometry loaded" );
}

void testGlb()
{
    std::printf( "GLB container:\n" );
    Triangle triangle;
    triangle.indexed = false;
    Rhiza::MeshDesc mesh;
    std::string error;
    check( parse( triangle.glb(), mesh, error ), "parses: " + error );
    check( mesh.vertices.size() == 3, "3 vertices" );
    check( mesh.indices.size() == 3 && mesh.indices[2] == 2, "unindexed primitive gets sequential indices" );
}

void testRejections()
{
    std::printf( "bad input is rejected, not crashed on:\n" );
    Rhiza::MeshDesc mesh;
    std::string error;

    const std::string garbage = "this is not gltf";
    check( !parse( std::vector<uint8_t>( garbage.begin(), garbage.end() ), mesh, error ), "garbage rejected" );
    check( !parse( {}, mesh, error ), "empty input rejected" );

    Triangle points;
    points.mode = 0;
    check( !parse( points.embeddedGltf(), mesh, error ), "points-only file rejected" );
    check( error.find( "no triangle" ) != std::string::npos, "with a reason: " + error );

    Triangle external;
    const std::string text = external.json( "missing.bin" );
    check( !parse( std::vector<uint8_t>( text.begin(), text.end() ), mesh, error ), "missing buffer rejected" );
}

}  // namespace

int main()
{
    testNormalizeAssetPath();
    testJoinAssetPath();
    testCacheReturnsTheSameHandle();
    testCacheFreesOnTheLastRelease();
    testCacheIgnoresUnknownHandles();
    testSameNameInDifferentFolders();
    testFindCaseMismatch();
    testTextureCacheKey();
    testTriangle();
    testNodeTranslation();
    testMirroring();
    testNormalTransform();
    testExternalBuffer();
    testGlb();
    testRejections();

    if( gFailures == 0 )
    {
        std::printf( "\nAll asset tests passed.\n" );
        return 0;
    }

    std::printf( "\n%d check(s) failed.\n", gFailures );
    return 1;
}
