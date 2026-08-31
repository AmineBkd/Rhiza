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
    uint32_t createMeshAsset( const MeshDesc &desc );

    // No-op (logged) if `handle` doesn't name a mesh asset created with
    // MeshDesc::isMutable = true, or the new description is unusable.
    void updateMesh( uint32_t handle, const MeshDesc &desc );

    // No-op (logged) while any instance still references this asset.
    void destroyMeshAsset( uint32_t handle );

    uint32_t createMaterial( const MaterialDesc &desc );

    // No-op (logged) while any instance still references this material.
    void destroyMaterial( uint32_t handle );

    // Returns 0 if either handle is invalid.
    uint32_t createInstance( uint32_t meshHandle, uint32_t materialHandle );
    void destroyInstance( uint32_t handle );

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

    // Geometry uploaded once via createMeshAsset, instantiated any number of
    // times via createInstance. Tracked by name rather than an Ogre::MeshPtr
    // so Renderer.h doesn't need Ogre's mesh headers - see the layer rule in
    // ARCHITECTURE.md.
    struct MeshAsset
    {
        std::string name;
        bool isMutable = false;

        // How many live instances reference this asset. destroyMeshAsset
        // refuses (and logs) while this is nonzero - dropping the asset out
        // from under a still-attached Item would leave it pointing at a
        // freed mesh.
        uint32_t instanceRefCount = 0;
    };

    struct MaterialAsset
    {
        Ogre::HlmsDatablock *datablock = nullptr;
        uint32_t instanceRefCount = 0;
    };

    // One placed copy of a mesh asset, shaded with a material. Tracked
    // together with which asset/material it references so destroyInstance
    // can decrement their ref-counts, and updateMesh can find every Item
    // that needs telling its mesh's geometry changed.
    struct Instance
    {
        Ogre::SceneNode *node = nullptr;
        Ogre::Item *item = nullptr;
        uint32_t meshAssetHandle = 0;
        uint32_t materialHandle = 0;
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

    // Handles are handed out from one counter shared across every map
    // below, so a handle value is never ambiguous between them.
    uint32_t mNextHandle = 1;
    std::unordered_map<uint32_t, MeshAsset> mMeshAssets;
    std::unordered_map<uint32_t, MaterialAsset> mMaterials;
    std::unordered_map<uint32_t, Instance> mInstances;
    std::unordered_map<uint32_t, LightInstance> mLights;
};

}  // namespace Rhiza
