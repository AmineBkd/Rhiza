#pragma once

#include <Rhiza/Types.h>

namespace Rhiza::Shapes
{

// Plain MeshDesc factories - no GPU state, no renderer knowledge. Every
// primitive carries correct per-face normals, so they light properly under
// ShadingModel::Lit. Ogre-Next has no v2 primitives of its own (PT_CUBE and
// friends exist only on the deprecated v1 pipeline), so these are Rhiza's.

// An axis-aligned cube centred on the origin.
MeshDesc cube( float size = 1.0f );

// A flat square in the XZ plane centred on the origin, facing up (+Y).
MeshDesc plane( float size = 1.0f );

// A rectangle in the XY plane centred on the origin, facing the default
// camera (+Z). The sprite primitive.
MeshDesc quad( float width = 1.0f, float height = 1.0f );

}  // namespace Rhiza::Shapes
