#include <Rhiza/Engine.h>
#include <Rhiza/Shapes.h>

int main( int argc, char *argv[] )
{
    Rhiza::Engine engine;
    Rhiza::EngineSettings settings;
    if( !engine.initialize( settings ) )
        return 1;

    Rhiza::LightDesc sun;
    sun.type = Rhiza::LightType::Directional;
    sun.direction = { -1.0f, -1.5f, -0.8f };
    engine.createLight( sun );

    // One mesh asset, uploaded once, instantiated twice below with two
    // different materials - the reason createMeshAsset and createInstance
    // are separate calls.
    Rhiza::MeshHandle cubeMesh = engine.createMeshAsset( Rhiza::Shapes::cube( 2.0f ) );

    Rhiza::MaterialDesc litMaterial;
    litMaterial.shading = Rhiza::ShadingModel::Lit;
    litMaterial.color = { 0.9f, 0.3f, 0.2f, 1.0f };
    litMaterial.roughness = 0.5f;
    Rhiza::MaterialHandle lit = engine.createMaterial( litMaterial );
    engine.createInstance( cubeMesh, lit );

    // The same mesh, unlit: flat colour, ignoring the light entirely.
    Rhiza::MaterialDesc unlitMaterial;
    unlitMaterial.shading = Rhiza::ShadingModel::Unlit;
    unlitMaterial.color = { 0.9f, 0.3f, 0.2f, 1.0f };
    Rhiza::MaterialHandle unlit = engine.createMaterial( unlitMaterial );
    Rhiza::InstanceHandle flat = engine.createInstance( cubeMesh, unlit );
    engine.setPosition( flat, { -3.5f, 0.0f, 0.0f } );

    Rhiza::MeshHandle groundMesh = engine.createMeshAsset( Rhiza::Shapes::plane( 20.0f ) );
    Rhiza::MaterialDesc groundMaterial;
    groundMaterial.color = { 0.35f, 0.38f, 0.4f, 1.0f };
    groundMaterial.roughness = 0.9f;
    Rhiza::MaterialHandle groundMaterialHandle = engine.createMaterial( groundMaterial );
    Rhiza::InstanceHandle groundNode = engine.createInstance( groundMesh, groundMaterialHandle );
    engine.setPosition( groundNode, { 0.0f, -1.5f, 0.0f } );

    Rhiza::Vec3 flatPosition{ -3.5f, 0.0f, 0.0f };

    // No control system yet - plain game code reading input directly. The
    // camera's position and target aren't queryable from Engine, so
    // they're tracked here and shifted by the same delta, which pans the
    // view without changing its angle.
    Rhiza::Vec3 cameraPosition = settings.cameraPosition;
    Rhiza::Vec3 cameraTarget = settings.cameraTarget;

    while( engine.beginFrame() )
    {
        // WASD pans the camera, arrows move the unlit cube. Both scale by
        // deltaSeconds, so speed is the same at any frame rate.
        constexpr float cameraUnitsPerSecond = 4.0f;
        const float camStep = cameraUnitsPerSecond * engine.deltaSeconds();
        if( engine.isKeyDown( Rhiza::Key::W ) )
        {
            cameraPosition.z -= camStep;
            cameraTarget.z -= camStep;
        }
        if( engine.isKeyDown( Rhiza::Key::S ) )
        {
            cameraPosition.z += camStep;
            cameraTarget.z += camStep;
        }
        if( engine.isKeyDown( Rhiza::Key::A ) )
        {
            cameraPosition.x -= camStep;
            cameraTarget.x -= camStep;
        }
        if( engine.isKeyDown( Rhiza::Key::D ) )
        {
            cameraPosition.x += camStep;
            cameraTarget.x += camStep;
        }
        engine.setCamera( cameraPosition, cameraTarget );

        constexpr float unitsPerSecond = 3.0f;
        const float step = unitsPerSecond * engine.deltaSeconds();
        if( engine.isKeyDown( Rhiza::Key::Left ) )
            flatPosition.x -= step;
        if( engine.isKeyDown( Rhiza::Key::Right ) )
            flatPosition.x += step;
        if( engine.isKeyDown( Rhiza::Key::Up ) )
            flatPosition.z -= step;
        if( engine.isKeyDown( Rhiza::Key::Down ) )
            flatPosition.z += step;
        engine.setPosition( flat, flatPosition );

        // Everything above ran before the draw, so this frame's input is on
        // screen this frame.
        engine.endFrame();
    }

    // No save system yet, so a lost device only reports. Once there is one,
    // this branch saves and relaunches instead of returning.
    if( engine.deviceLost() )
        return 2;

    return 0;
}
