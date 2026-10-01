#pragma once

#include <Rhiza/Components/MeshRenderer.h>
#include <Rhiza/Engine.h>
#include <Rhiza/Registry.h>
#include <Rhiza/Types/Math.h>

namespace Rhiza
{

// The only place in framework/ that holds an Engine reference. Every other
// system deals in components alone, so replacing the renderer means
// rewriting this file and nothing else above it.

// Call once at startup. Destroys an entity's instance with its MeshRenderer;
// the mesh and material are shared, so they stay.
inline void connectRenderSystem( Registry &registry, Engine &engine )
{
    registry.onRemove<MeshRenderer>( [&engine]( Entity, MeshRenderer &renderer ) {
        engine.destroyInstance( renderer.instance );
    } );
}

// The ECS owns transforms; Ogre's scene nodes only mirror them. Pushes
// unconditionally rather than tracking what moved: correct at the hundreds
// of entities this targets, and a dirty flag is the fix if profiling ever
// disagrees.
inline void renderSystem( Registry &registry, Engine &engine )
{
    for( auto [entity, renderer, transform] : registry.view<MeshRenderer, Transform>() )
        engine.setTransform( renderer.instance, transform );
}

}  // namespace Rhiza
