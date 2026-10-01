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

struct SoundHandle
{
    uint32_t id = 0;

    bool isValid() const { return id != 0; }
};

// One playing instance of a sound. Voices are reused, so the generation
// tells a handle to a voice that has since been taken over apart from a
// live one: the stale handle simply does nothing.
struct VoiceHandle
{
    uint32_t id = 0;
    uint32_t generation = 0;

    bool isValid() const { return id != 0; }
};

}  // namespace Rhiza
