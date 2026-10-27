// Tests for the renderer on Ogre's NULL render system: every Ogre call runs
// and every Hlms material is built, but no GPU or window is needed, so CI can
// catch a renderer regression that would otherwise only show by eye.

#include "render/Renderer.h"

#include <Rhiza/Shapes.h>

#include <catch2/catch_test_macros.hpp>

#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#ifndef RHIZA_TEST_PNG
#    error "RHIZA_TEST_PNG must be defined by CMake"
#endif

namespace
{

Rhiza::EngineSettings headlessSettings()
{
    Rhiza::EngineSettings settings;
    settings.headless = true;
    return settings;
}

std::vector<uint8_t> readFile( const char *path )
{
    std::ifstream file( path, std::ios::binary );
    return { std::istreambuf_iterator<char>( file ), std::istreambuf_iterator<char>() };
}

void renderFrames( Rhiza::Renderer &renderer, int count = 3 )
{
    for( int i = 0; i < count; ++i )
        renderer.renderOneFrame();
    REQUIRE_FALSE( renderer.hasRenderFailed() );
    REQUIRE_FALSE( renderer.isDeviceLost() );
}

}  // namespace

TEST_CASE( "A headless renderer starts and draws empty frames" )
{
    Rhiza::Renderer renderer;
    REQUIRE( renderer.initialize( {}, headlessSettings() ) );
    renderFrames( renderer );
}

TEST_CASE( "Every material combination renders" )
{
    Rhiza::Renderer renderer;
    REQUIRE( renderer.initialize( {}, headlessSettings() ) );

    const uint32_t texture = renderer.createTexture( readFile( RHIZA_TEST_PNG ), "ship.png", {} );
    REQUIRE( texture != 0 );
    const uint32_t mesh = renderer.createMeshAsset( Rhiza::Shapes::quad() );
    REQUIRE( mesh != 0 );

    for( Rhiza::ShadingModel shading : { Rhiza::ShadingModel::Lit, Rhiza::ShadingModel::Unlit } )
    {
        for( Rhiza::BlendMode blend :
             { Rhiza::BlendMode::Opaque, Rhiza::BlendMode::AlphaBlend, Rhiza::BlendMode::Additive } )
        {
            for( bool textured : { false, true } )
            {
                Rhiza::MaterialDesc desc;
                desc.shading = shading;
                desc.blend = blend;
                desc.texture.id = textured ? texture : 0;
                const uint32_t material = renderer.createMaterial( desc );
                REQUIRE( material != 0 );
                REQUIRE( renderer.createInstance( mesh, material ) != 0 );
            }
        }
    }

    REQUIRE( renderer.createLight( {} ) != 0 );
    Rhiza::LightDesc point;
    point.type = Rhiza::LightType::Point;
    REQUIRE( renderer.createLight( point ) != 0 );

    renderFrames( renderer );
}

TEST_CASE( "Both camera projections render" )
{
    Rhiza::Renderer renderer;
    REQUIRE( renderer.initialize( {}, headlessSettings() ) );
    const uint32_t material = renderer.createMaterial( {} );
    REQUIRE( renderer.createInstance( renderer.createMeshAsset( Rhiza::Shapes::cube() ), material ) != 0 );

    Rhiza::CameraDesc camera;
    renderer.setCamera( camera );
    renderFrames( renderer );

    camera.projection = Rhiza::Projection::Orthographic;
    renderer.setCamera( camera );
    renderFrames( renderer );
}

TEST_CASE( "Assets in use refuse to be destroyed, then free once unused" )
{
    Rhiza::Renderer renderer;
    REQUIRE( renderer.initialize( {}, headlessSettings() ) );

    const uint32_t mesh = renderer.createMeshAsset( Rhiza::Shapes::cube() );
    const uint32_t material = renderer.createMaterial( {} );
    const uint32_t instance = renderer.createInstance( mesh, material );
    REQUIRE( instance != 0 );

    renderer.destroyMeshAsset( mesh );
    renderer.destroyMaterial( material );
    const uint32_t second = renderer.createInstance( mesh, material );
    CHECK( second != 0 );

    renderer.destroyInstance( instance );
    renderer.destroyInstance( second );
    renderer.destroyMeshAsset( mesh );
    renderer.destroyMaterial( material );
    CHECK( renderer.createInstance( mesh, material ) == 0 );
    renderFrames( renderer );
}

TEST_CASE( "A mutable mesh can change size while instanced" )
{
    Rhiza::Renderer renderer;
    REQUIRE( renderer.initialize( {}, headlessSettings() ) );

    Rhiza::MeshDesc desc = Rhiza::Shapes::quad();
    desc.isMutable = true;
    const uint32_t mesh = renderer.createMeshAsset( desc );
    REQUIRE( renderer.createInstance( mesh, renderer.createMaterial( {} ) ) != 0 );
    renderFrames( renderer );

    Rhiza::MeshDesc bigger = Rhiza::Shapes::cube();
    bigger.isMutable = true;
    renderer.updateMesh( mesh, bigger );
    renderFrames( renderer );
}

TEST_CASE( "A texture loaded twice under one key is one texture" )
{
    Rhiza::Renderer renderer;
    REQUIRE( renderer.initialize( {}, headlessSettings() ) );

    const uint32_t first = renderer.createTexture( readFile( RHIZA_TEST_PNG ), "ship.png", {} );
    REQUIRE( first != 0 );
    CHECK( renderer.acquireCachedTexture( "ship.png", {} ) == first );

    Rhiza::TextureDesc nearest;
    nearest.filter = Rhiza::TextureFilter::Nearest;
    CHECK( renderer.acquireCachedTexture( "ship.png", nearest ) == 0 );
}
