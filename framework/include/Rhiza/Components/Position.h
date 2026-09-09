#pragma once

#include <Rhiza/Types.h>

namespace Rhiza
{

// Deliberately not called Transform: the engine can only place an instance,
// so rotation, scale and parenting have nowhere to go yet. Renaming this is
// the moment to answer whether the ECS or Ogre's scene graph owns transforms.
struct Position
{
    Vec3 value;
};

}  // namespace Rhiza
