#include <Rhiza/RhizaEngine.h>
#include <Rhiza/Shapes.h>

int main( int argc, char *argv[] )
{
    Rhiza::RhizaEngine engine;
    if( !engine.initialize() )
        return 1;

    // The sun. Every Lit material in the scene responds to it.
    Rhiza::LightDesc sun;
    sun.type = Rhiza::LightType::Directional;
    sun.direction = { -1.0f, -1.5f, -0.8f };
    engine.createLight( sun );

    // One mesh asset, uploaded once, instantiated twice below with two
    // different materials - the reason createMeshAsset/createInstance are
    // separate calls instead of one createMesh doing both.
    Rhiza::MeshHandle cubeMesh = engine.createMeshAsset( Rhiza::Shapes::cube( 2.0f ) );

    // A lit material: each face carries its own normal, so the three faces
    // we can see catch the light at three different angles.
    Rhiza::MaterialDesc litMaterial;
    litMaterial.shading = Rhiza::ShadingModel::Lit;
    litMaterial.color = { 0.9f, 0.3f, 0.2f, 1.0f };
    litMaterial.roughness = 0.5f;
    Rhiza::MaterialHandle lit = engine.createMaterial( litMaterial );
    engine.createInstance( cubeMesh, lit );

    // The same mesh again with an Unlit material, for comparison: flat
    // colour, completely ignoring the light above. This is the 2D/sprite
    // path.
    Rhiza::MaterialDesc unlitMaterial;
    unlitMaterial.shading = Rhiza::ShadingModel::Unlit;
    unlitMaterial.color = { 0.9f, 0.3f, 0.2f, 1.0f };
    Rhiza::MaterialHandle unlit = engine.createMaterial( unlitMaterial );
    Rhiza::SceneNodeHandle flat = engine.createInstance( cubeMesh, unlit );
    engine.setPosition( flat, { -3.5f, 0.0f, 0.0f } );

    // A ground plane, to catch the light and give the cubes context. Its
    // own asset and material - different geometry, nothing to share with
    // the cubes above.
    Rhiza::MeshHandle groundMesh = engine.createMeshAsset( Rhiza::Shapes::plane( 20.0f ) );
    Rhiza::MaterialDesc groundMaterial;
    groundMaterial.color = { 0.35f, 0.38f, 0.4f, 1.0f };
    groundMaterial.roughness = 0.9f;
    Rhiza::MaterialHandle groundMaterialHandle = engine.createMaterial( groundMaterial );
    Rhiza::SceneNodeHandle groundNode = engine.createInstance( groundMesh, groundMaterialHandle );
    engine.setPosition( groundNode, { 0.0f, -1.5f, 0.0f } );

    Rhiza::Vec3 flatPosition{ -3.5f, 0.0f, 0.0f };

    while( engine.tick() )
    {
        // Arrow-key smoke test for Rhiza::Key and deltaSeconds(): moves the
        // flat cube at a fixed speed regardless of frame rate.
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
    }

    return 0;
}
