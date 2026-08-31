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
};

// A plain-data description of a mesh, built entirely from primitive types so
// callers never need to know Ogre's vertex/index buffer types exist.
struct MeshDesc
{
    std::vector<Vertex> vertices;
    std::vector<uint16_t> indices;
    Color color;
};

// Opaque reference to an object the engine placed in the scene. It carries no
// usable information on its own; it only identifies "which object" to later
// callers such as RhizaEngine::setPosition.
struct SceneNodeHandle
{
    uint32_t id = 0;

    bool isValid() const { return id != 0; }
};

struct EngineSettings
{
    const char *windowTitle = "Rhiza";
    int windowWidth = 1280;
    int windowHeight = 720;
};

}  // namespace Rhiza
