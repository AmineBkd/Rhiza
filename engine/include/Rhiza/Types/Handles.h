#pragma once

#include <cstdint>

namespace Rhiza
{

// Opaque references to things the engine owns. They carry no usable
// information; they only identify which object a later call means.
struct InstanceHandle
{
    uint32_t id = 0;

    bool isValid() const { return id != 0; }
};

struct LightHandle
{
    uint32_t id = 0;

    bool isValid() const { return id != 0; }
};

struct MeshHandle
{
    uint32_t id = 0;

    bool isValid() const { return id != 0; }
};

struct MaterialHandle
{
    uint32_t id = 0;

    bool isValid() const { return id != 0; }
};

struct TextureHandle
{
    uint32_t id = 0;

    bool isValid() const { return id != 0; }
};

}  // namespace Rhiza
