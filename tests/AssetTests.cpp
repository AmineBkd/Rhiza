// Tests for asset keys, the cache's bookkeeping and glTF mesh parsing. No
// GPU or window, so they run anywhere, CI included; only the case-mismatch
// check touches the file system, in a temp folder of its own.

#include "core/AssetCache.h"
#include "core/AssetPath.h"
#include "core/Gltf.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>

namespace
{

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
    const bool parsed = Rhiza::parseGltfMesh( bytes, path, reader, mesh, error );
    if( !parsed )
        UNSCOPED_INFO( "parse error: " << error );
    return parsed;
}

TEST_CASE( "Assets: normalizeAssetPath" )
{
    CHECK( Rhiza::normalizeAssetPath( "ship.png" ) == "ship.png" );
    CHECK( Rhiza::normalizeAssetPath( "./ship.png" ) == "ship.png" );
    CHECK( Rhiza::normalizeAssetPath( "textures//ship.png" ) == "textures/ship.png" );
    CHECK( Rhiza::normalizeAssetPath( "textures\\ship.png" ) == "textures/ship.png" );
    CHECK( Rhiza::normalizeAssetPath( "a/b/../ship.png" ) == "a/ship.png" );
    CHECK( Rhiza::normalizeAssetPath( "Ship.png" ) == "Ship.png" );
}

TEST_CASE( "Assets: joinAssetPath" )
{
    CHECK( Rhiza::joinAssetPath( "", "a.png" ) == "a.png" );
    CHECK( Rhiza::joinAssetPath( "root", "a.png" ) == "root/a.png" );
    CHECK( Rhiza::joinAssetPath( "root/", "a.png" ) == "root/a.png" );
    CHECK( Rhiza::joinAssetPath( "C:\\game\\", "a.png" ) == "C:\\game\\a.png" );
}

TEST_CASE( "Assets: cache: same key, same handle" )
{
    Rhiza::AssetCache cache;
    CHECK( cache.acquire( "rock.glb" ) == 0 );

    cache.add( "rock.glb", 7 );
    CHECK( cache.acquire( "rock.glb" ) == 7 );
    CHECK( cache.acquire( "rock.glb" ) == 7 );
    CHECK( cache.loadCount( 7 ) == 3 );

    CHECK( cache.acquire( "ship.png" ) == 0 );
    cache.add( "ship.png", 8 );
    CHECK( cache.acquire( "ship.png" ) == 8 );
    CHECK( cache.loadCount( 7 ) == 3 );
}

TEST_CASE( "Assets: cache: freed by the last release only" )
{
    Rhiza::AssetCache cache;
    cache.add( "rock.glb", 7 );
    cache.acquire( "rock.glb" );
    cache.acquire( "rock.glb" );

    CHECK( cache.releaseIfShared( 7 ) );
    CHECK( cache.releaseIfShared( 7 ) );
    CHECK( cache.loadCount( 7 ) == 1 );
    CHECK( !cache.releaseIfShared( 7 ) );
    CHECK( cache.loadCount( 7 ) == 1 );

    cache.forget( 7 );
    CHECK( cache.acquire( "rock.glb" ) == 0 );
    CHECK( cache.loadCount( 7 ) == 0 );

    cache.add( "rock.glb", 12 );
    CHECK( cache.acquire( "rock.glb" ) == 12 );
}

TEST_CASE( "Assets: cache: handles it never saw" )
{
    Rhiza::AssetCache cache;
    CHECK( !cache.releaseIfShared( 99 ) );
    cache.forget( 99 );
    cache.add( "a", 1 );
    cache.forget( 2 );
    CHECK( cache.acquire( "a" ) == 1 );

    cache.clear();
    CHECK( cache.acquire( "a" ) == 0 );
}

TEST_CASE( "Assets: cache: same file name in different folders" )
{
    const std::string ships = Rhiza::normalizeAssetPath( "ships/hull.png" );
    const std::string stations = Rhiza::normalizeAssetPath( "stations/hull.png" );
    CHECK( ships != stations );

    Rhiza::AssetCache cache;
    cache.add( ships, 1 );
    CHECK( cache.acquire( stations ) == 0 );
    cache.add( stations, 2 );
    CHECK( cache.acquire( ships ) == 1 );
    CHECK( cache.acquire( stations ) == 2 );

    const Rhiza::TextureDesc linear;
    CHECK( Rhiza::textureCacheKey( ships, linear ) != Rhiza::textureCacheKey( stations, linear ) );
}

// Compares names rather than asking the file system, so it reports the same
// on case-insensitive Windows/macOS and case-sensitive Linux.
TEST_CASE( "Assets: case mismatch against the names on disk" )
{
    namespace fs = std::filesystem;
    const fs::path root = fs::temp_directory_path() / "rhiza_asset_tests";
    std::error_code error;
    fs::remove_all( root, error );
    fs::create_directories( root / "Textures", error );
    std::ofstream( root / "Textures" / "Ship.png" ).put( 'x' );
    const std::string rootText = root.string();

    CHECK( Rhiza::findCaseMismatch( rootText, "Textures/Ship.png" ).empty() );
    CHECK( Rhiza::findCaseMismatch( rootText, "textures/ship.png" ) == "Textures/Ship.png" );
    CHECK( Rhiza::findCaseMismatch( rootText, "Textures/ship.png" ) == "Textures/Ship.png" );
    CHECK( Rhiza::findCaseMismatch( rootText, "Textures/missing.png" ).empty() );
    CHECK( Rhiza::findCaseMismatch( ( root / "nowhere" ).string(), "Ship.png" ).empty() );

    fs::remove_all( root, error );
}

TEST_CASE( "Assets: texture keys: settings are part of identity" )
{
    const Rhiza::TextureDesc linear;
    Rhiza::TextureDesc nearest;
    nearest.filter = Rhiza::TextureFilter::Nearest;
    Rhiza::TextureDesc repeating;
    repeating.wrap = Rhiza::TextureWrap::Repeat;

    CHECK( Rhiza::textureCacheKey( "ship.png", linear ) == Rhiza::textureCacheKey( "ship.png", linear ) );
    CHECK( Rhiza::textureCacheKey( "ship.png", linear ) != Rhiza::textureCacheKey( "ship.png", nearest ) );
    CHECK( Rhiza::textureCacheKey( "ship.png", linear ) != Rhiza::textureCacheKey( "ship.png", repeating ) );
    CHECK( Rhiza::textureCacheKey( Rhiza::normalizeAssetPath( "./textures//ship.png" ), linear ) ==
               Rhiza::textureCacheKey( Rhiza::normalizeAssetPath( "textures/ship.png" ), linear ) );
}

TEST_CASE( "Assets: embedded triangle" )
{
    Rhiza::MeshDesc mesh;
    std::string error;
    CHECK( parse( Triangle{}.embeddedGltf(), mesh, error ) );
    CHECK( mesh.vertices.size() == 3 );
    CHECK( mesh.indices.size() == 3 );
    if( mesh.vertices.size() != 3 )
        return;
    CHECK( near( mesh.vertices[1].position.x, 1.0f ) );
    CHECK( near( mesh.vertices[0].normal.z, 1.0f ) );
    CHECK( mesh.indices[0] == 0 );
    CHECK( mesh.indices[1] == 1 );
    CHECK( mesh.indices[2] == 2 );
}

TEST_CASE( "Assets: node transform baked" )
{
    Triangle triangle;
    triangle.nodeTransform = "\"translation\":[5,0,0]";
    Rhiza::MeshDesc mesh;
    std::string error;
    CHECK( parse( triangle.embeddedGltf(), mesh, error ) );
    REQUIRE( !mesh.vertices.empty() );
    CHECK( near( mesh.vertices[0].position.x, 5.0f ) );
}

TEST_CASE( "Assets: mirroring keeps the front face" )
{
    Triangle triangle;
    triangle.nodeTransform = "\"scale\":[-1,1,1]";
    Rhiza::MeshDesc mesh;
    std::string error;
    CHECK( parse( triangle.embeddedGltf(), mesh, error ) );
    REQUIRE( mesh.indices.size() == 3 );
    CHECK( mesh.indices[1] == 2 );
    CHECK( mesh.indices[2] == 1 );
    REQUIRE( !mesh.vertices.empty() );
    CHECK( near( mesh.vertices[0].normal.z, 1.0f ) );
}

TEST_CASE( "Assets: authored normals follow the transform" )
{
    const float plusX[3] = { 1, 0, 0 };
    Triangle rotated;
    rotated.normal = plusX;
    rotated.nodeTransform = "\"rotation\":[0,0,0.7071068,0.7071068]";  // 90 degrees about +Z
    Rhiza::MeshDesc mesh;
    std::string error;
    CHECK( parse( rotated.embeddedGltf(), mesh, error ) );
    REQUIRE( !mesh.vertices.empty() );
    CHECK( near( mesh.vertices[0].normal.y, 1.0f ) );

    // Stretching along X tilts a diagonal normal toward Y, not toward X.
    const float diagonal[3] = { 0.7071068f, 0.7071068f, 0 };
    Triangle stretched;
    stretched.normal = diagonal;
    stretched.nodeTransform = "\"scale\":[2,1,1]";
    CHECK( parse( stretched.embeddedGltf(), mesh, error ) );
    if( mesh.vertices.empty() )
        return;
    const Rhiza::Vec3 n = mesh.vertices[0].normal;
    CHECK( near( n.x, 0.4472136f ) );
    CHECK( near( n.y, 0.8944272f ) );
}

TEST_CASE( "Assets: external buffer goes through the reader" )
{
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
    CHECK( parse( std::vector<uint8_t>( text.begin(), text.end() ), mesh, error, reader, "models/tri.gltf" ) );
    CHECK( requested == "models/tri.bin" );
    CHECK( mesh.vertices.size() == 3 );
}

TEST_CASE( "Assets: GLB container" )
{
    Triangle triangle;
    triangle.indexed = false;
    Rhiza::MeshDesc mesh;
    std::string error;
    CHECK( parse( triangle.glb(), mesh, error ) );
    CHECK( mesh.vertices.size() == 3 );
    REQUIRE( mesh.indices.size() == 3 );
    CHECK( mesh.indices[2] == 2 );
}

TEST_CASE( "Assets: bad input is rejected, not crashed on" )
{
    Rhiza::MeshDesc mesh;
    std::string error;

    const std::string garbage = "this is not gltf";
    CHECK( !parse( std::vector<uint8_t>( garbage.begin(), garbage.end() ), mesh, error ) );
    CHECK( !parse( {}, mesh, error ) );

    Triangle points;
    points.mode = 0;
    CHECK( !parse( points.embeddedGltf(), mesh, error ) );
    CHECK_THAT( error, Catch::Matchers::ContainsSubstring( "no triangle" ) );

    Triangle external;
    const std::string text = external.json( "missing.bin" );
    CHECK( !parse( std::vector<uint8_t>( text.begin(), text.end() ), mesh, error ) );
}

}  // namespace
