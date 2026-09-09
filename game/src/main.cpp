#include <Rhiza/Components/MeshRenderer.h>
#include <Rhiza/Components/Position.h>
#include <Rhiza/Engine.h>
#include <Rhiza/Registry.h>
#include <Rhiza/Shapes.h>
#include <Rhiza/Systems/RenderSystem.h>

int main( int argc, char *argv[] )
{
    Rhiza::Engine engine;
    Rhiza::EngineSettings settings;
    if( !engine.initialize( settings ) )
        return 1;

    Rhiza::Registry registry;

    auto spawn = [&]( Rhiza::MeshHandle mesh, Rhiza::MaterialHandle material,
                      Rhiza::Vec3 position ) {
        const Rhiza::Entity entity = registry.createEntity();
        registry.addComponent( entity, Rhiza::Position{ position } );
        registry.addComponent(
            entity, Rhiza::MeshRenderer{ mesh, material, engine.createInstance( mesh, material ) } );
        return entity;
    };

    Rhiza::LightDesc sun;
    sun.type = Rhiza::LightType::Directional;
    sun.direction = { -1.0f, -1.5f, -0.8f };
    engine.createLight( sun );

    // One mesh asset, uploaded once, instantiated twice below with two
    // different materials.
    Rhiza::MeshHandle cubeMesh = engine.createMeshAsset( Rhiza::Shapes::cube( 2.0f ) );

    Rhiza::MaterialDesc litMaterial;
    litMaterial.shading = Rhiza::ShadingModel::Lit;
    litMaterial.color = { 0.1f, 0.3f, 0.2f, 1.0f };
    litMaterial.roughness = 0.5f;
    spawn( cubeMesh, engine.createMaterial( litMaterial ), { 0.0f, 0.0f, 0.0f } );

    // The same mesh, unlit: flat colour, ignoring the light entirely.
    Rhiza::MaterialDesc unlitMaterial;
    unlitMaterial.shading = Rhiza::ShadingModel::Unlit;
    unlitMaterial.color = { 0.9f, 0.3f, 0.2f, 1.0f };
    const Rhiza::Entity flatCube =
        spawn( cubeMesh, engine.createMaterial( unlitMaterial ), { -3.5f, 0.0f, 0.0f } );

    Rhiza::MeshHandle groundMesh = engine.createMeshAsset( Rhiza::Shapes::plane( 20.0f ) );
    Rhiza::MaterialDesc groundMaterial;
    groundMaterial.color = { 0.35f, 0.38f, 0.4f, 1.0f };
    groundMaterial.roughness = 0.9f;
    spawn( groundMesh, engine.createMaterial( groundMaterial ), { 0.0f, -1.5f, 0.0f } );

    // No control system yet - plain game code reading input directly. The
    // camera's position and target aren't queryable from Engine, so they're
    // tracked here and shifted by the same delta, which pans the view without
    // changing its angle.
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
        if( Rhiza::Position *position = registry.getComponent<Rhiza::Position>( flatCube ) )
        {
            if( engine.isKeyDown( Rhiza::Key::Left ) )
                position->value.x -= step;
            if( engine.isKeyDown( Rhiza::Key::Right ) )
                position->value.x += step;
            if( engine.isKeyDown( Rhiza::Key::Up ) )
                position->value.z -= step;
            if( engine.isKeyDown( Rhiza::Key::Down ) )
                position->value.z += step;
        }

        // Systems run in the order this loop calls them; the render bridge
        // goes last so it sees this frame's changes.
        Rhiza::renderSystem( registry, engine );

        engine.endFrame();
    }

    // No save system yet, so a lost device only reports. Once there is one,
    // this branch saves and relaunches instead of returning.
    if( engine.deviceLost() )
        return 2;

    return 0;
}
