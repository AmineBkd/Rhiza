#include <Rhiza/RhizaEngine.h>

int main( int argc, char *argv[] )
{
    Rhiza::RhizaEngine engine;
    if( !engine.initialize() )
        return 1;

    Rhiza::MeshDesc triangle;
    triangle.vertices = {
        { { 0.0f, 0.5f, 0.0f } },
        { { 0.5f, -0.5f, 0.0f } },
        { { -0.5f, -0.5f, 0.0f } },
    };
    triangle.indices = { 0, 1, 2 };
    triangle.color = { 0.9f, 0.3f, 0.2f, 1.0f };
    engine.createMesh( triangle );

    while( engine.tick() ) {
        
    }

    return 0;
}
