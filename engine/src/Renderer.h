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

private:
    void registerUnlitHlms();

    Ogre::Root *mRoot = nullptr;
    Ogre::SceneManager *mSceneManager = nullptr;
    Ogre::Camera *mCamera = nullptr;
    Ogre::Window *mRenderWindow = nullptr;
    Ogre::CompositorWorkspace *mWorkspace = nullptr;

    uint32_t mNextHandle = 1;
    std::unordered_map<uint32_t, Ogre::SceneNode *> mSceneNodes;
};

}  // namespace Rhiza
