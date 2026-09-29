#include <algorithm>
#include <cmath>
#include <string>

#include <Rhiza/Components/MeshRenderer.h>
#include <Rhiza/Engine.h>
#include <Rhiza/Registry.h>
#include <Rhiza/Shapes.h>
#include <Rhiza/Systems/RenderSystem.h>
#include <Rhiza/Types/Camera.h>
#include <Rhiza/Types/Handles.h>
#include <Rhiza/Types/Input.h>
#include <Rhiza/Types/Light.h>
#include <Rhiza/Types/Material.h>
#include <Rhiza/Types/Math.h>
#include <Rhiza/Types/Texture.h>

#ifndef EXPANSUM_ASSET_DIR
#    error "EXPANSUM_ASSET_DIR must be defined by CMake"
#endif

namespace
{

std::string asset( const char *name )
{
    return std::string( EXPANSUM_ASSET_DIR ) + "/" + name;
}

constexpr Rhiza::Vec3 kZAxis{ 0.0f, 0.0f, 1.0f };

struct Spin
{
    Rhiza::Vec3 axis;
    float radiansPerSecond = 0.0f;
    float angle = 0.0f;
};

}  // namespace

int main( int argc, char *argv[] )
{
    Rhiza::Engine engine;
    if( !engine.initialize() )
        return 1;

    Rhiza::Registry registry;
    Rhiza::connectRenderSystem( registry, engine );

    auto spawn = [&]( Rhiza::MeshHandle mesh, Rhiza::MaterialHandle material,
                      const Rhiza::Transform &transform ) {
        const Rhiza::Entity entity = registry.createEntity();
        registry.addComponent( entity, transform );
        registry.addComponent(
            entity, Rhiza::MeshRenderer{ mesh, material, engine.createInstance( mesh, material ) } );
        return entity;
    };

    Rhiza::LightDesc sun;
    sun.direction = { -1.0f, -1.5f, -2.0f };
    engine.createLight( sun );

    const Rhiza::MeshHandle quad = engine.createMeshAsset( Rhiza::Shapes::quad() );

    Rhiza::TextureDesc pixelArt;
    pixelArt.filter = Rhiza::TextureFilter::Nearest;

    Rhiza::MaterialDesc shipMaterial;
    shipMaterial.shading = Rhiza::ShadingModel::Unlit;
    shipMaterial.texture = engine.loadTexture( asset( "ship.png" ).c_str(), pixelArt );
    shipMaterial.blend = Rhiza::BlendMode::AlphaBlend;
    const Rhiza::Entity ship = spawn( quad, engine.createMaterial( shipMaterial ),
                                      { { 0.0f, 0.0f, 1.0f }, {}, { 1.5f, 1.5f, 1.0f } } );

    Rhiza::MaterialDesc flameMaterial;
    flameMaterial.shading = Rhiza::ShadingModel::Unlit;
    flameMaterial.color = { 1.0f, 0.55f, 0.15f, 1.0f };
    flameMaterial.texture = engine.loadTexture( asset( "glow.png" ).c_str() );
    flameMaterial.blend = Rhiza::BlendMode::Additive;
    const Rhiza::Entity flame = spawn( quad, engine.createMaterial( flameMaterial ), {} );

    // Lit 3D objects in the 2D scene.
    const Rhiza::MeshHandle cube = engine.createMeshAsset( Rhiza::Shapes::cube() );
    Rhiza::MaterialDesc rockDesc;
    rockDesc.color = { 0.45f, 0.4f, 0.38f, 1.0f };
    rockDesc.roughness = 0.9f;
    const Rhiza::MaterialHandle rockMaterial = engine.createMaterial( rockDesc );

    const Rhiza::Vec3 spinAxes[] = { { 0.6f, 0.8f, 0.0f }, { 0.0f, 0.6f, 0.8f }, { 0.8f, 0.0f, 0.6f } };
    for( int i = 0; i < 8; ++i )
    {
        const float around = static_cast<float>( i ) * 0.785f;
        const float size = 0.8f + 0.1f * static_cast<float>( i );
        const Rhiza::Entity rock =
            spawn( cube, rockMaterial,
                   { { std::cos( around ) * 7.0f, std::sin( around ) * 7.0f, 0.0f }, {}, { size, size, size } } );
        registry.addComponent( rock, Spin{ spinAxes[i % 3], 0.4f + 0.15f * static_cast<float>( i ) } );
    }

    Rhiza::CameraDesc camera;
    camera.projection = Rhiza::Projection::Orthographic;
    camera.orthoHeight = 14.0f;

    // Radians; 0 = nose up (+Y).
    float heading = 0.0f;
    Rhiza::Vec3 velocity;
    float time = 0.0f;

    while( engine.beginFrame() )
    {
        const float dt = engine.deltaSeconds();
        time += dt;

        // A/D turn, W thrusts, Q/E zoom.
        constexpr float turnRate = 3.5f;
        constexpr float thrust = 12.0f;
        constexpr float drag = 1.5f;
        if( engine.isKeyDown( Rhiza::Key::A ) )
            heading += turnRate * dt;
        if( engine.isKeyDown( Rhiza::Key::D ) )
            heading -= turnRate * dt;

        const float forwardX = -std::sin( heading );
        const float forwardY = std::cos( heading );
        const bool thrusting = engine.isKeyDown( Rhiza::Key::W );
        if( thrusting )
        {
            velocity.x += forwardX * thrust * dt;
            velocity.y += forwardY * thrust * dt;
        }
        velocity.x -= velocity.x * drag * dt;
        velocity.y -= velocity.y * drag * dt;

        Rhiza::Transform *shipTransform = registry.getComponent<Rhiza::Transform>( ship );
        shipTransform->position.x += velocity.x * dt;
        shipTransform->position.y += velocity.y * dt;
        shipTransform->rotation = Rhiza::Quat::fromAxisAngle( kZAxis, heading );

        Rhiza::Transform *flameTransform = registry.getComponent<Rhiza::Transform>( flame );
        const float flameLength = thrusting ? 1.4f + 0.2f * std::sin( time * 40.0f ) : 0.5f;
        const float behind = 0.5f + flameLength * 0.5f;
        flameTransform->position = { shipTransform->position.x - forwardX * behind,
                                     shipTransform->position.y - forwardY * behind, 0.9f };
        flameTransform->rotation = shipTransform->rotation;
        flameTransform->scale = { 0.8f, flameLength, 1.0f };

        for( auto [entity, spin, transform] : registry.view<Spin, Rhiza::Transform>() )
        {
            spin.angle += spin.radiansPerSecond * dt;
            transform.rotation = Rhiza::Quat::fromAxisAngle( spin.axis, spin.angle );
        }

        // Zoom reads real time so it keeps working if gameplay is slowed.
        const float zoomStep = 1.0f + engine.unscaledDeltaSeconds();
        if( engine.isKeyDown( Rhiza::Key::Q ) )
            camera.orthoHeight *= zoomStep;
        if( engine.isKeyDown( Rhiza::Key::E ) )
            camera.orthoHeight /= zoomStep;
        camera.orthoHeight = std::clamp( camera.orthoHeight, 4.0f, 40.0f );
        camera.position = { shipTransform->position.x, shipTransform->position.y, 10.0f };
        engine.setCamera( camera );

        Rhiza::renderSystem( registry, engine );

        engine.endFrame();
    }

    if( engine.deviceLost() )
        return 2;

    return 0;
}
