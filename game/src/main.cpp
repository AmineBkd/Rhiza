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

    // A lit cube: each face carries its own normal, so the three faces we
    // can see catch the light at three different angles.
    Rhiza::MeshDesc cube = Rhiza::Shapes::cube( 2.0f );
    cube.material.shading = Rhiza::ShadingModel::Lit;
    cube.material.color = { 0.9f, 0.3f, 0.2f, 1.0f };
    cube.material.roughness = 0.5f;
    engine.createMesh( cube );

    // The same geometry with an Unlit material, for comparison: flat colour,
    // completely ignoring the light above. This is the 2D/sprite path.
    Rhiza::MeshDesc flatCube = Rhiza::Shapes::cube( 2.0f );
    flatCube.material.shading = Rhiza::ShadingModel::Unlit;
    flatCube.material.color = { 0.9f, 0.3f, 0.2f, 1.0f };
    Rhiza::SceneNodeHandle flat = engine.createMesh( flatCube );
    engine.setPosition( flat, { -3.5f, 0.0f, 0.0f } );

    // A ground plane, to catch the light and give the cubes context.
    Rhiza::MeshDesc ground = Rhiza::Shapes::plane( 20.0f );
    ground.material.color = { 0.35f, 0.38f, 0.4f, 1.0f };
    ground.material.roughness = 0.9f;
    Rhiza::SceneNodeHandle groundNode = engine.createMesh( ground );
    engine.setPosition( groundNode, { 0.0f, -1.5f, 0.0f } );

    while( engine.tick() )
    {
    }

    return 0;
}
