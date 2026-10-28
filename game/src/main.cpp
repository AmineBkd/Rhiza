#include <algorithm>
#include <cmath>

#include <Rhiza/Components/MeshRenderer.h>
#include <Rhiza/Components/RigidBody.h>
#include <Rhiza/Engine.h>
#include <Rhiza/Main.h>
#include <Rhiza/Registry.h>
#include <Rhiza/Shapes.h>
#include <Rhiza/Systems/PhysicsSystem.h>
#include <Rhiza/Systems/RenderSystem.h>
#include <Rhiza/Types/Camera.h>
#include <Rhiza/Types/Handles.h>
#include <Rhiza/Types/Input.h>
#include <Rhiza/Types/Light.h>
#include <Rhiza/Types/Material.h>
#include <Rhiza/Types/Math.h>
#include <Rhiza/Types/Physics.h>
#include <Rhiza/Types/Texture.h>

namespace
{

struct Spin
{
    Rhiza::Vec3 axis;
    float radiansPerSecond = 0.0f;
    float angle = 0.0f;
};

}  // namespace

int main( int, char *[] )
{
    Rhiza::Engine engine;
    if( !engine.initialize() )
        return 1;

    Rhiza::Registry registry;
    Rhiza::connectRenderSystem( registry, engine );
    Rhiza::connectPhysicsSystem( registry, engine );

    auto addBody = [&]( Rhiza::Entity entity, Rhiza::BodyDesc body, const Rhiza::ShapeDesc &shape ) {
        body.userData = Rhiza::toUserData( entity );
        const Rhiza::BodyHandle handle = engine.createBody( body );
        engine.addShape( handle, shape );
        registry.addComponent( entity, Rhiza::RigidBody{ handle } );
        return handle;
    };

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
    shipMaterial.texture = engine.loadTexture( "ship.png", pixelArt );
    shipMaterial.blend = Rhiza::BlendMode::AlphaBlend;
    const Rhiza::Entity ship = spawn( quad, engine.createMaterial( shipMaterial ),
                                      { { 0.0f, 0.0f, 1.0f }, {}, { 1.5f, 1.5f, 1.0f } } );
    Rhiza::BodyDesc shipBodyDesc;
    shipBodyDesc.linearDamping = 1.5f;
    const Rhiza::BodyHandle shipBody = addBody( ship, shipBodyDesc, Rhiza::ShapeDesc::circle( 0.6f ) );

    Rhiza::MaterialDesc flameMaterial;
    flameMaterial.shading = Rhiza::ShadingModel::Unlit;
    flameMaterial.color = { 1.0f, 0.55f, 0.15f, 1.0f };
    flameMaterial.texture = engine.loadTexture( "glow.png" );
    flameMaterial.blend = Rhiza::BlendMode::Additive;
    const Rhiza::Entity flame = spawn( quad, engine.createMaterial( flameMaterial ), {} );

    // Lit 3D objects in the 2D scene.
    const Rhiza::MeshHandle rockMesh = engine.loadMesh( "rock.glb" );
    Rhiza::MaterialDesc rockDesc;
    rockDesc.color = { 0.45f, 0.4f, 0.38f, 1.0f };
    rockDesc.roughness = 0.9f;
    const Rhiza::MaterialHandle rockMaterial = engine.createMaterial( rockDesc );

    const Rhiza::Vec3 spinAxes[] = { { 0.6f, 0.8f, 0.0f }, { 0.0f, 0.6f, 0.8f }, { 0.8f, 0.0f, 0.6f } };
    for( int i = 0; i < 8; ++i )
    {
        const float around = static_cast<float>( i ) * 0.785f;
        const float size = 0.8f + 0.1f * static_cast<float>( i );
        const Rhiza::Entity rock = spawn(
            rockMesh, rockMaterial,
            { { std::cos( around ) * 7.0f, std::sin( around ) * 7.0f, 0.0f }, {}, { size, size, size } } );
        registry.addComponent( rock, Spin{ spinAxes[i % 3], 0.4f + 0.15f * static_cast<float>( i ) } );

        // The tumble is Spin's, so physics leaves rotation alone. 0.58 is
        // rock.glb's radius at scale 1.
        Rhiza::BodyDesc rockBody;
        rockBody.position = { std::cos( around ) * 7.0f, std::sin( around ) * 7.0f };
        rockBody.linearDamping = 0.5f;
        rockBody.fixedRotation = true;
        Rhiza::ShapeDesc rockShape = Rhiza::ShapeDesc::circle( 0.58f * size );
        rockShape.density = 3.0f;
        rockShape.restitution = 0.4f;
        addBody( rock, rockBody, rockShape );
    }

    Rhiza::CameraDesc camera;
    camera.projection = Rhiza::Projection::Orthographic;
    camera.orthoHeight = 14.0f;

    const Rhiza::SoundHandle thrustSound = engine.loadSound( "thrust.wav" );
    const Rhiza::SoundHandle pingSound = engine.loadSound( "ping.wav" );
    Rhiza::VoiceHandle thrustVoice;
    engine.playMusic( "music.ogg", 2.0f );

    const uint64_t shipUserData = Rhiza::toUserData( ship );
    float time = 0.0f;

    while( engine.beginFrame() )
    {
        const float dt = engine.deltaSeconds();
        time += dt;

        // A/D turn, W thrusts, Space pings (so does bumping a rock), Q/E zoom.
        constexpr float turnRate = 3.5f;
        constexpr float thrustForce = 14.0f;
        float turn = 0.0f;
        if( engine.isKeyDown( Rhiza::Key::A ) )
            turn += turnRate;
        if( engine.isKeyDown( Rhiza::Key::D ) )
            turn -= turnRate;
        engine.setAngularVelocity( shipBody, turn );

        // Radians; 0 = nose up (+Y).
        auto forwardOf = []( float heading ) {
            return Rhiza::Vec2{ -std::sin( heading ), std::cos( heading ) };
        };
        const bool thrusting = engine.isKeyDown( Rhiza::Key::W );
        while( engine.physicsStepDue() )
        {
            if( thrusting )
                engine.applyForce( shipBody, forwardOf( engine.bodyPose( shipBody ).angle ) * thrustForce );
            engine.stepPhysics();
        }
        Rhiza::physicsSystem( registry, engine );

        Rhiza::Transform *shipTransform = registry.getComponent<Rhiza::Transform>( ship );
        const Rhiza::Vec2 forward2d = forwardOf( engine.bodyPose( shipBody ).angle );
        const Rhiza::Vec3 forward{ forward2d.x, forward2d.y, 0.0f };

        // No velocity for the ship's own sounds: the listener rides along with
        // it, and Doppler is for things moving relative to the ears.
        const Rhiza::Vec3 shipPosition = shipTransform->position;
        if( thrusting && !engine.isVoicePlaying( thrustVoice ) )
        {
            Rhiza::PlayDesc rumble;
            rumble.loop = true;
            rumble.positional = true;
            rumble.position = shipPosition;
            thrustVoice = engine.playSound( thrustSound, rumble );
        }
        else if( !thrusting )
        {
            engine.stopVoice( thrustVoice );
        }
        engine.setVoicePosition( thrustVoice, shipPosition );

        bool pinged = engine.wasKeyPressed( Rhiza::Key::Space );
        for( const Rhiza::ContactEvent &contact : engine.contactEvents() )
            pinged |= contact.type == Rhiza::ContactEventType::Begin &&
                      ( contact.userDataA == shipUserData || contact.userDataB == shipUserData );
        if( pinged )
        {
            Rhiza::PlayDesc ping;
            ping.positional = true;
            ping.position = shipPosition;
            engine.playSound( pingSound, ping );
        }

        Rhiza::Transform *flameTransform = registry.getComponent<Rhiza::Transform>( flame );
        const float flameLength = thrusting ? 1.4f + 0.2f * std::sin( time * 40.0f ) : 0.5f;
        const float behind = 0.5f + flameLength * 0.5f;
        flameTransform->position = shipTransform->position - forward * behind;
        flameTransform->position.z = 0.9f;
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
    if( engine.renderFailed() )
        return 3;

    return 0;
}
