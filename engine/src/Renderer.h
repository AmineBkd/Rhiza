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
class HlmsDatablock;
}  // namespace Ogre

namespace Rhiza
{

// Owns every Ogre-Next object: Root, the render window, the scene manager,
// the Hlms setup and the compositor workspace. Knows nothing about SDL;
// Window.h must never be included here. The only thing it receives from the
// window is a NativeWindowHandle to hand to Ogre as the render target.
class Renderer
{
public:
    ~Renderer();

    bool initialize( const NativeWindowHandle &windowHandle, const std::string &title, int width,
                     int height );
    void shutdown();

    void renderOneFrame();

    uint32_t createMesh( const MeshDesc &desc );
    void setPosition( uint32_t handle, Vec3 position );

    uint32_t createLight( const LightDesc &desc );
    void setAmbientLight( const Color &skyColor, const Color &groundColor );

private:
    // Registers both Hlms implementations. Both are always registered even
    // if a project only uses one, because they are how Rhiza expresses 3D
    // (Pbs) versus 2D (Unlit) and a scene may freely mix them.
    void registerHlms();

    // Builds the datablock (Ogre's term for a material) matching `material`,
    // choosing the Pbs or Unlit implementation from its ShadingModel.
    // `name` must be unique across the whole Hlms manager.
    Ogre::HlmsDatablock *createDatablock( const std::string &name, const MaterialDesc &material );

    Ogre::Root *mRoot = nullptr;
    Ogre::SceneManager *mSceneManager = nullptr;
    Ogre::Camera *mCamera = nullptr;
    Ogre::Window *mRenderWindow = nullptr;
    Ogre::CompositorWorkspace *mWorkspace = nullptr;

    // Handles are handed out from one counter shared by meshes and lights,
    // so a handle value is never ambiguous between the two maps.
    uint32_t mNextHandle = 1;
    std::unordered_map<uint32_t, Ogre::SceneNode *> mSceneNodes;
    std::unordered_map<uint32_t, Ogre::SceneNode *> mLightNodes;
};

}  // namespace Rhiza
