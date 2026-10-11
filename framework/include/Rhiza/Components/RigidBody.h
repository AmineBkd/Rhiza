#pragma once

#include <Rhiza/Types/Handles.h>

namespace Rhiza
{

// Links an entity to a physics body. With a Transform beside it,
// physicsSystem moves the entity wherever the body goes.
struct RigidBody
{
    BodyHandle body;
};

}  // namespace Rhiza
