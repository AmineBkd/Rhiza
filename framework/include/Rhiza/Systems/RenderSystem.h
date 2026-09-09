#pragma once

#include <Rhiza/Components/MeshRenderer.h>
#include <Rhiza/Components/Position.h>
#include <Rhiza/Engine.h>
#include <Rhiza/Registry.h>

namespace Rhiza
{

// The only place in framework/ that holds an Engine reference. Every other
// system deals in components alone, so replacing the renderer means
// rewriting this file and nothing else above it.
//
// Pushes unconditionally rather than tracking what moved: correct at the
// hundreds of entities this targets, and a dirty flag is the fix if
// profiling ever disagrees.
inline void renderSystem( Registry &registry, Engine &engine )
{
    for( auto [entity, renderer, position] : registry.view<MeshRenderer, Position>() )
        engine.setPosition( renderer.instance, position.value );
}

}  // namespace Rhiza
