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

    // Adds a mesh to the scene. Returns an invalid handle if the description
    // is unusable - empty, not a triangle list, or with indices pointing past
    // the end of the vertex list - with the reason written to the log.
    SceneNodeHandle createMesh(const MeshDesc &desc);

    // Removes a mesh and frees its GPU buffers and material. The handle is
    // dead afterwards; passing it to anything else is a harmless no-op.
    void destroyMesh(SceneNodeHandle handle);

    // Moves a previously created mesh. No-op if the handle is invalid.
    void setPosition(SceneNodeHandle handle, Vec3 position);

    // Adds a light. Only materials using ShadingModel::Lit respond to it;
    // ShadingModel::Unlit surfaces are unaffected by any light in the scene.
    LightHandle createLight(const LightDesc &desc);

    // Removes a light. No-op if the handle is invalid.
    void destroyLight(LightHandle handle);

    // Sets the light that arrives from every direction at once, standing in
    // for bounced light the renderer doesn't simulate. Without it, surfaces
    // facing away from every light are pure black. Ogre blends between the
    // two colours by how far a surface tilts up toward the sky or down
    // toward the ground.
    void setAmbientLight(Color skyColor, Color groundColor);

    // Moves the camera and aims it. Initial values come from EngineSettings.
    void setCamera(Vec3 position, Vec3 target);

    // Whether `key` is held down right now. Keys are identified by physical
    // position - see the Key comment in Types.h.
    bool isKeyDown(Key key) const;

    // True only on the frame `key` went from up to down. Holding the key
    // does not repeat this - use isKeyDown for that.
    bool wasKeyPressed(Key key) const;

    // True only on the frame `key` went from down to up.
    bool wasKeyReleased(Key key) const;

    // Seconds since the previous tick(), scaled by timeScale(). Drives
    // gameplay and in-world animation - this is what hitstop freezes or
    // slows down. Zero before the first tick().
    float deltaSeconds() const;

    // Seconds since the previous tick(), never scaled. Drives input, UI and
    // anything else that must keep moving through a hitstop.
    float unscaledDeltaSeconds() const;

    // Multiplier applied to unscaledDeltaSeconds() to produce deltaSeconds().
    // 1 = normal speed, 0 = frozen (hitstop), negative = time running
    // backward. Defaults to 1.
    float timeScale() const;
    void setTimeScale(float scale);

private:
    struct Impl;
    std::unique_ptr<Impl> mImpl;
};

}  // namespace Rhiza
