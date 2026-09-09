#pragma once

#include <memory>
#include <Rhiza/Types.h>

namespace Rhiza
{

// Facade over the engine's window (SDL3) and renderer (Ogre-Next). Nothing
// outside engine/src ever includes an SDL or Ogre header: this class and
// Types.h are the entire public surface.
class RhizaEngine
{
public:
    RhizaEngine();
    ~RhizaEngine();

    RhizaEngine(const RhizaEngine &) = delete;
    RhizaEngine &operator=(const RhizaEngine &) = delete;

    // False on failure, with the reason in the log; the engine is unusable
    // until this succeeds.
    bool initialize(const EngineSettings &settings = {});

    // Safe to call more than once; the destructor calls it too.
    void shutdown();

    // Starts a frame: advances the clock, then pumps window and input
    // events. False once the user has asked to close the window - stop the
    // loop without calling endFrame().
    bool beginFrame();

    // True once the GPU device has been lost - a driver reset, a GPU
    // removed, or VK_ERROR_DEVICE_LOST on Android. beginFrame() returns
    // false from then on, so the loop exits; check this afterwards to tell a
    // crash apart from the user closing the window. The engine does not
    // recover in place: save and restart the process.
    bool deviceLost() const;

    // Renders the frame. Call once per successful beginFrame(), after game
    // code has finished mutating the scene - anything changed after this
    // lands on screen a frame late.
    //
    //   while( engine.beginFrame() )
    //   {
    //       ... update the world ...
    //       engine.endFrame();
    //   }
    void endFrame();

    // Uploads geometry once; place copies with createInstance(). Invalid
    // handle if the description is unusable - empty, not a triangle list, or
    // indices past the end of the vertex list.
    MeshHandle createMeshAsset(const MeshDesc &desc);

    // Re-uploads geometry in place. Requires MeshDesc::isMutable; every
    // instance picks up the change with nothing to update per-instance.
    void updateMesh(MeshHandle mesh, const MeshDesc &desc);

    // Refused while any instance still references it.
    void destroyMeshAsset(MeshHandle mesh);

    // A material (Ogre's "datablock"), shared across instances like a mesh.
    MaterialHandle createMaterial(const MaterialDesc &desc);

    // Refused while any instance still references it.
    void destroyMaterial(MaterialHandle material);

    SceneNodeHandle createInstance(MeshHandle mesh, MaterialHandle material);

    // The mesh asset and material survive; others may still reference them.
    void destroyInstance(SceneNodeHandle instance);

    void setPosition(SceneNodeHandle handle, Vec3 position);

    // Only ShadingModel::Lit materials respond to lights.
    LightHandle createLight(const LightDesc &desc);
    void destroyLight(LightHandle handle);

    // Stands in for bounced light the renderer doesn't simulate; without it,
    // surfaces facing away from every light are pure black. Ogre blends the
    // two colours by how far a surface tilts toward sky or ground.
    void setAmbientLight(Color skyColor, Color groundColor);

    void setCamera(Vec3 position, Vec3 target);

    // Keys are identified by physical position - see Key in Types.h.
    // wasKeyPressed/Released are true only on the frame of the transition;
    // holding a key does not repeat them.
    bool isKeyDown(Key key) const;
    bool wasKeyPressed(Key key) const;
    bool wasKeyReleased(Key key) const;

    // deltaSeconds drives gameplay and in-world animation - this is what
    // hitstop freezes. unscaledDeltaSeconds drives input, UI and anything
    // else that must keep moving through one. Both zero before the first
    // beginFrame(). timeScale: 1 = normal, 0 = frozen, negative = backward.
    float deltaSeconds() const;
    float unscaledDeltaSeconds() const;
    float timeScale() const;
    void setTimeScale(float scale);

private:
    struct Impl;
    std::unique_ptr<Impl> mImpl;
};

}  // namespace Rhiza
