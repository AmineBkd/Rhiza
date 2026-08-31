#pragma once

#include <memory>
#include <Rhiza/Types.h>

namespace Rhiza
{

// Facade over the engine's window (SDL3) and renderer (Ogre-Next) subsystems.
// Nothing outside engine/src ever needs to include an SDL or Ogre header:
// this class and Types.h are the entire public surface.
class RhizaEngine
{
public:
    RhizaEngine();
    ~RhizaEngine();

    RhizaEngine(const RhizaEngine &) = delete;
    RhizaEngine &operator=(const RhizaEngine &) = delete;

    // Creates the window and the renderer. Returns false on failure (check
    // the log for details); the engine is unusable until this succeeds.
    bool initialize(const EngineSettings &settings = {});

    // Releases the renderer and window. Safe to call more than once; also
    // called automatically by the destructor if you don't call it yourself.
    void shutdown();

    // Pumps window/input events and renders one frame.
    // Returns false once the user has requested to close the window, at
    // which point the caller's main loop should stop calling tick().
    bool tick();

    // Adds a mesh to the scene and returns a handle to it. The handle stays
    // valid until shutdown().
    SceneNodeHandle createMesh(const MeshDesc &desc);

    // Moves a previously created mesh. No-op if the handle is invalid.
    void setPosition(SceneNodeHandle handle, Vec3 position);

    // Adds a light. Only materials using ShadingModel::Lit respond to it;
    // ShadingModel::Unlit surfaces are unaffected by any light in the scene.
    LightHandle createLight(const LightDesc &desc);

    // Sets the light that arrives from every direction at once, standing in
    // for bounced light the renderer doesn't simulate. Without it, surfaces
    // facing away from every light are pure black. Ogre blends between the
    // two colours by how far a surface tilts up toward the sky or down
    // toward the ground.
    void setAmbientLight(Color skyColor, Color groundColor);

private:
    struct Impl;
    std::unique_ptr<Impl> mImpl;
};

}  // namespace Rhiza
