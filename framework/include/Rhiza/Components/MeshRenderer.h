#pragma once

#include <Rhiza/Types.h>

namespace Rhiza
{

// What an entity looks like: shared geometry, a shared material, and the one
// placed copy of them that belongs to this entity.
struct MeshRenderer
{
    MeshHandle mesh;
    MaterialHandle material;
    InstanceHandle instance;
};

}  // namespace Rhiza
