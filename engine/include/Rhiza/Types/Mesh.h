#pragma once

#include <cstdint>
#include <vector>

#include <Rhiza/Types/Math.h>

namespace Rhiza
{

struct Vertex
{
    Vec3 position;

    // Read only by ShadingModel::Lit. Unlit ignores it, so 2D/sprite
    // geometry can leave it at zero.
    Vec3 normal;

    // (0,0) is the image's top-left, (1,1) its bottom-right.
    Vec2 uv;
};

// Pure geometry, carrying no material, so one mesh can be instantiated any
// number of times with different materials.
struct MeshDesc
{
    std::vector<Vertex> vertices;
    std::vector<uint16_t> indices;

    // Static geometry uploads as GPU-read-only (BT_IMMUTABLE), which is
    // cheaper but permanently locks the buffer. updateMesh() needs this true.
    bool isMutable = false;
};

}  // namespace Rhiza
