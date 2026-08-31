#pragma once

#include <Rhiza/Types.h>

namespace Rhiza::Shapes
{

// Built-in primitive geometry. These are plain MeshDesc factories - they
// touch no GPU state and know nothing about the renderer, so they are just
// as usable for building a mesh by hand as they are for feeding straight
// into RhizaEngine::createMesh.
//
// Every primitive carries correct per-face normals, so they light properly
// under ShadingModel::Lit. Ogre-Next itself offers no v2 primitives - its
// PT_CUBE/PT_SPHERE prefabs exist only on the deprecated v1 pipeline - so
// these are Rhiza's own.

// An axis-aligned cube centred on the origin.
// Uses 24 vertices rather than 8: a cube corner belongs to three faces
// pointing three different ways, and a vertex can only carry one normal, so
// corners cannot be shared without smearing the lighting across the edges.
MeshDesc cube( float size = 1.0f );

// A flat square in the XZ plane centred on the origin, facing up (+Y).
// Useful as a ground plane.
MeshDesc plane( float size = 1.0f );

}  // namespace Rhiza::Shapes
