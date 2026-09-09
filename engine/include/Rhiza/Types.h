#pragma once

#include <cstdint>
#include <vector>

namespace Rhiza
{

struct Vec3
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct Color
{
    float r = 1.0f;
    float g = 1.0f;
    float b = 1.0f;
    float a = 1.0f;
};

struct Vertex
{
    Vec3 position;

    // Read only by ShadingModel::Lit. Unlit ignores it, so 2D/sprite
    // geometry can leave it at zero.
    Vec3 normal;
};

// Picks the shading path per material rather than globally, so a lit 3D
// world with a flat 2D HUD over it is the normal case. Maps onto Ogre-Next's
// two Hlms implementations: Lit -> HlmsPbs, Unlit -> HlmsUnlit ("great for
// GUI, billboards, particle FXs"), which is Rhiza's 2D/effects path.
// https://ogrecave.github.io/ogre-next/api/latest/hlms.html
enum class ShadingModel
{
    Lit,
    Unlit,
};

struct MaterialDesc
{
    ShadingModel shading = ShadingModel::Lit;
    Color color;

    // Lit only. 0 = mirror-smooth, 1 = fully diffuse.
    float roughness = 0.6f;

    // Lit only. 0 = dielectric, 1 = raw metal. Values in between are
    // physically meaningless, so prefer the extremes.
    float metalness = 0.0f;

    // Defaults to true because MeshDesc makes no promise about winding
    // order; false enables backface culling.
    bool doubleSided = true;
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

enum class LightType
{
    // Infinitely far away, so only its direction matters. The sun.
    Directional,
    // Radiates from a point, fading with distance.
    Point,
};

struct LightDesc
{
    LightType type = LightType::Directional;

    // Directional only: the direction light travels, not where it comes
    // from. The default points down-and-away, like afternoon sun.
    Vec3 direction{ -1.0f, -1.0f, -1.0f };

    // Point only.
    Vec3 position{ 0.0f, 0.0f, 0.0f };

    Color color;

    // 1.0 is "normal" exposure for a scene with no HDR tonemapping, which is
    // what Rhiza currently renders.
    float power = 1.0f;
};

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

// A physical key, identified by position rather than what it types - the
// same key is Key::W on QWERTY and AZERTY. Right for gameplay bindings,
// wrong for "press X to continue" UI, which Rhiza doesn't need yet.
//
// Extending the list is one line here plus one in Window.cpp's table.
enum class Key
{
    Unknown = 0,

    A, B, C, D, E, F, G, H, I, J, K, L, M,
    N, O, P, Q, R, S, T, U, V, W, X, Y, Z,

    Num0, Num1, Num2, Num3, Num4, Num5, Num6, Num7, Num8, Num9,

    Up, Down, Left, Right,

    Space, Return, Escape, Tab, Backspace,

    LeftShift, RightShift,
    LeftCtrl, RightCtrl,
    LeftAlt, RightAlt,

    // Sizes Input's storage, so it can't go stale.
    Count
};

struct EngineSettings
{
    const char *windowTitle = "Rhiza";
    int windowWidth = 1280;
    int windowHeight = 720;

    // Off-axis by default so a solid object shows more than one face, which
    // makes lighting legible.
    Vec3 cameraPosition{ 3.5f, 3.0f, 5.0f };
    Vec3 cameraTarget{ 0.0f, 0.0f, 0.0f };
};

}  // namespace Rhiza
