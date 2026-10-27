// Tests for the whole engine through its public API, headless: the real frame
// loop, renderer, audio and physics together, with no display, GPU or sound
// card. Its own scene, so no game's code is needed to run it.

#include <Rhiza/Engine.h>
#include <Rhiza/Shapes.h>

#include <catch2/catch_test_macros.hpp>

#include <chrono>

#ifndef RHIZA_TEST_ASSETS
#    error "RHIZA_TEST_ASSETS must be defined by CMake"
#endif

namespace
{

Rhiza::EngineSettings headlessSettings()
{
    Rhiza::EngineSettings settings;
    settings.headless = true;
    settings.assetRoot = RHIZA_TEST_ASSETS;
    settings.application = "EngineTests";  // keeps the game's own Rhiza.log intact
    return settings;
}

// One frame the way a game runs it: physics catches up, then it draws.
bool runFrame( Rhiza::Engine &engine )
{
    if( !engine.beginFrame() )
        return false;
    while( engine.physicsStepDue() )
        engine.stepPhysics();
    engine.endFrame();
    return true;
}

}  // namespace

TEST_CASE( "A headless engine runs frames and shuts down" )
{
    Rhiza::Engine engine;
    REQUIRE( engine.initialize( headlessSettings() ) );
    for( int i = 0; i < 60; ++i )
        REQUIRE( runFrame( engine ) );
    CHECK_FALSE( engine.deviceLost() );
    CHECK_FALSE( engine.renderFailed() );
    engine.shutdown();
}

TEST_CASE( "Every asset kind loads from the asset root" )
{
    Rhiza::Engine engine;
    REQUIRE( engine.initialize( headlessSettings() ) );

    CHECK( engine.loadTexture( "ship.png" ).isValid() );
    CHECK( engine.loadMesh( "rock.glb" ).isValid() );
    CHECK( engine.loadSound( "ping.wav" ).isValid() );
    CHECK_FALSE( engine.loadTexture( "missing.png" ).isValid() );
    CHECK_FALSE( engine.loadSound( "missing.wav" ).isValid() );
}

TEST_CASE( "A scene using every service runs until a body lands" )
{
    Rhiza::EngineSettings settings = headlessSettings();
    settings.gravity = { 0.0f, -10.0f };
    Rhiza::Engine engine;
    REQUIRE( engine.initialize( settings ) );

    Rhiza::MaterialDesc spriteDesc;
    spriteDesc.shading = Rhiza::ShadingModel::Unlit;
    spriteDesc.blend = Rhiza::BlendMode::AlphaBlend;
    spriteDesc.texture = engine.loadTexture( "ship.png" );
    const Rhiza::InstanceHandle sprite = engine.createInstance(
        engine.createMeshAsset( Rhiza::Shapes::quad() ), engine.createMaterial( spriteDesc ) );
    const Rhiza::InstanceHandle rock =
        engine.createInstance( engine.loadMesh( "rock.glb" ), engine.createMaterial( {} ) );
    REQUIRE( sprite.isValid() );
    REQUIRE( rock.isValid() );
    REQUIRE( engine.createLight( {} ).isValid() );

    Rhiza::CameraDesc camera;
    camera.projection = Rhiza::Projection::Orthographic;
    engine.setCamera( camera );

    engine.playMusic( "music.ogg" );
    const Rhiza::VoiceHandle ping = engine.playSound( engine.loadSound( "ping.wav" ) );
    CHECK( engine.isVoicePlaying( ping ) );

    Rhiza::BodyDesc groundDesc;
    groundDesc.type = Rhiza::BodyType::Static;
    const Rhiza::BodyHandle ground = engine.createBody( groundDesc );
    REQUIRE( engine.addShape( ground, Rhiza::ShapeDesc::box( { 5.0f, 0.5f } ) ) );

    Rhiza::BodyDesc ballDesc;
    ballDesc.position = { 0.0f, 2.0f };
    const Rhiza::BodyHandle ball = engine.createBody( ballDesc );
    REQUIRE( engine.addShape( ball, Rhiza::ShapeDesc::circle( 0.5f ) ) );

    // Physics runs on frame time, so landing takes real seconds, not a frame count.
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds( 5 );
    bool landed = false;
    while( !landed && std::chrono::steady_clock::now() < deadline )
    {
        REQUIRE( runFrame( engine ) );
        const Rhiza::BodyPose pose = engine.bodyPose( ball );
        engine.setTransform( sprite, { { pose.position.x, pose.position.y, 0.0f } } );
        for( const Rhiza::ContactEvent &contact : engine.contactEvents() )
            landed |= contact.type == Rhiza::ContactEventType::Begin;
    }
    CHECK( landed );
    CHECK_FALSE( engine.renderFailed() );
}
