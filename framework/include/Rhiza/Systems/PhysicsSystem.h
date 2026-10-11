#pragma once

#include <Rhiza/Components/RigidBody.h>
#include <Rhiza/Engine.h>
#include <Rhiza/Entity.h>
#include <Rhiza/Registry.h>
#include <Rhiza/Types/Math.h>

#include <cstdint>

namespace Rhiza
{

// The physics bridge between framework/ and engine/, as RenderSystem is for
// rendering: the ECS owns transforms, bodies only drive them.

// An entity packed into BodyDesc::userData, so contact events and raycast
// hits lead back to it. 0 stays "no entity".
inline uint64_t toUserData( Entity entity )
{
    if( !entity.isValid() )
        return 0;
    return ( static_cast<uint64_t>( entity.generation ) << 32 ) |
           ( static_cast<uint64_t>( entity.index ) + 1 );
}

inline Entity entityFromUserData( uint64_t userData )
{
    if( userData == 0 )
        return {};
    return Entity{ static_cast<uint32_t>( userData & 0xFFFFFFFFu ) - 1,
                   static_cast<uint32_t>( userData >> 32 ) };
}

// Call once at startup. Destroys an entity's body with its RigidBody.
inline void connectPhysicsSystem( Registry &registry, Engine &engine )
{
    registry.onRemove<RigidBody>(
        [&engine]( Entity, RigidBody &rigidBody ) { engine.destroyBody( rigidBody.body ); } );
}

// Call after the frame's physics steps. Copies each body's x, y and rotation
// about Z into its Transform; z stays the game's, for layering.
inline void physicsSystem( Registry &registry, Engine &engine )
{
    for( auto [entity, rigidBody, transform] : registry.view<RigidBody, Transform>() )
    {
        const BodyPose pose = engine.bodyPose( rigidBody.body );
        transform.position.x = pose.position.x;
        transform.position.y = pose.position.y;
        transform.rotation = Quat::fromAxisAngle( { 0.0f, 0.0f, 1.0f }, pose.angle );
    }
}

}  // namespace Rhiza
