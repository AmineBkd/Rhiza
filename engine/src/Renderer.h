#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

#include <Rhiza/Types.h>

#include "NativeWindowHandle.h"

namespace Ogre
{
class Root;
class SceneManager;
class Camera;
class Window;
class CompositorWorkspace;
class SceneNode;
class Item;
class Light;
class HlmsDatablock;
}  // namespace Ogre

namespace Rhiza
{

// Owns every Ogre-Next object: Root, the render window, the scene manager,
// the Hlms setup and the compositor workspace. Knows nothing about SDL;
// Window.h must never be included here. The only thing it receives from the
// window is a NativeWindowHandle to hand to Ogre as the render target.
//
// No Ogre exception is allowed to escape this class. Ogre reports most
// failures by throwing, but Rhiza's public API reports them as a false
// return or an invalid handle, so every entry point that can trigger Ogre
// work translates at the boundary.
class Renderer
{
public:
    ~Renderer();

    bool initialize( const NativeWindowHandle &windowHandle, const EngineSettings &settings );
    void shutdown();

    void renderOneFrame();

    // Returns 0 if the description is unusable or the GPU buffers could not
    // be allocated; the reason is written to the log.
    uint32_t createMesh( const MeshDesc &desc );
    void destroyMesh( uint32_t handle );
    void setPosition( uint32_t handle, Vec3 position );

    uint32_t createLight( const LightDesc &desc );
    void destroyLight( uint32_t handle );

    void setAmbientLight( const Color &skyColor, const Color &groundColor );
    void setCamera( Vec3 position, Vec3 target );

private:
    // The body of initialize(), split out so that one try/catch around the
    // call site covers every Ogre call in the startup sequence.
    bool initializeInternal( const NativeWindowHandle &windowHandle, const EngineSettings &settings );

    // Registers both Hlms implementations. Both are always registered even
    // if a project only uses one, because they are how Rhiza expresses 3D
    // (Pbs) versus 2D (Unlit) and a scene may freely mix them.
    void registerHlms();

    // Builds the datablock (Ogre's term for a material) matching `material`,
    // choosing the Pbs or Unlit implementation from its ShadingModel.
    // `name` must be unique across the whole Hlms manager.
    Ogre::HlmsDatablock *createDatablock( const std::string &name, const MaterialDesc &material );

    // Everything created on behalf of one createMesh call. Tracked together
    // so destroyMesh can take it all back down without interrogating Ogre
    // about what is attached to what.
    struct MeshInstance
    {
        Ogre::SceneNode *node = nullptr;
        Ogre::Item *item = nullptr;
        std::string meshName;
        std::string datablockName;
    };

    struct LightInstance
    {
        Ogre::SceneNode *node = nullptr;
        Ogre::Light *light = nullptr;
    };

    Ogre::Root *mRoot = nullptr;
    Ogre::SceneManager *mSceneManager = nullptr;
    Ogre::Camera *mCamera = nullptr;
    Ogre::Window *mRenderWindow = nullptr;
    Ogre::CompositorWorkspace *mWorkspace = nullptr;

    // Handles are handed out from one counter shared by meshes and lights,
    // so a handle value is never ambiguous between the two maps.
    uint32_t mNextHandle = 1;
    std::unordered_map<uint32_t, MeshInstance> mMeshes;
    std::unordered_map<uint32_t, LightInstance> mLights;
};

}  // namespace Rhiza
