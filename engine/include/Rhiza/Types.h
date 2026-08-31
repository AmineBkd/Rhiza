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

    // Which way the surface faces. Only meaningful for ShadingModel::Lit -
    // it is what makes a lit surface brighter facing a light and darker
    // facing away. Unlit materials ignore it entirely, so 2D/sprite geometry
    // can leave it at zero.
    Vec3 normal;
};

// Which shading path a surface uses. This is the single knob that decides
// whether something is treated as a 3D object or as flat 2D content, and it
// maps directly onto Ogre-Next's two Hlms implementations:
//
//   Lit   -> HlmsPbs, physically based, responds to lights and shadows.
//   Unlit -> HlmsUnlit, flat colour, ignores every light in the scene.
//            Ogre documents it as intended for "GUIs, overlays, particle
//            FXs, self-illuminating billboards" - i.e. the 2D/effects path.
//
// Because it is per-material rather than a global engine mode, a 3D lit
// world with a 2D unlit HUD over it is the normal case, not a special one.
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

    // Lit only. 0 = dielectric (plastic, wood, stone), 1 = raw metal.
    // Values in between are physically meaningless - real surfaces are one
    // or the other - so prefer the extremes.
    float metalness = 0.0f;

    // false lets the GPU discard triangles facing away from the camera,
    // which is free performance but requires consistent winding order.
    // Defaults to true because MeshDesc makes no promise about winding.
    bool doubleSided = true;
};

// A plain-data description of a mesh, built entirely from primitive types so
// callers never need to know Ogre's vertex/index buffer types exist.
struct MeshDesc
{
    std::vector<Vertex> vertices;
    std::vector<uint16_t> indices;
    MaterialDesc material;
};

enum class LightType
{
    // Infinitely far away, so only its direction matters. The sun.
    Directional,
    // Radiates in all directions from a point, fading with distance.
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

    // Brightness multiplier. 1.0 is "normal" exposure for a scene with no
    // HDR tonemapping, which is what Rhiza currently renders.
    float power = 1.0f;
};

// Opaque reference to an object the engine placed in the scene. It carries no
// usable information on its own; it only identifies "which object" to later
// callers such as RhizaEngine::setPosition.
struct SceneNodeHandle
{
    uint32_t id = 0;

    bool isValid() const { return id != 0; }
};

struct LightHandle
{
    uint32_t id = 0;

    bool isValid() const { return id != 0; }
};

struct EngineSettings
{
    const char *windowTitle = "Rhiza";
    int windowWidth = 1280;
    int windowHeight = 720;

    // Where the camera starts and what it points at. Off-axis by default so
    // a solid object shows more than one face, which makes lighting legible.
    Vec3 cameraPosition{ 3.5f, 3.0f, 5.0f };
    Vec3 cameraTarget{ 0.0f, 0.0f, 0.0f };
};

}  // namespace Rhiza
