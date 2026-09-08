#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

#include <Rhiza/Types.h>

#include "core/NativeWindowHandle.h"

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

// Owns every Ogre-Next object. Knows nothing about SDL; Window.h must never
// be included here.
//
// No Ogre exception may escape: Ogre reports failure by throwing, Rhiza's
// public API reports it as a false return or an invalid handle, so every
// entry point translates at the boundary.
class Renderer
{
public:
    ~Renderer();

    bool initialize( const NativeWindowHandle &windowHandle, const EngineSettings &settings );
    void shutdown();

    void renderOneFrame();

    // 0 if the description is unusable or the GPU buffers could not be
    // allocated; the reason goes to the log. Same for createInstance below.
    uint32_t createMeshAsset( const MeshDesc &desc );

    // No-op (logged) unless `handle` was created with MeshDesc::isMutable.
    void updateMesh( uint32_t handle, const MeshDesc &desc );

    // Both refuse (and log) while any instance still references them.
    void destroyMeshAsset( uint32_t handle );
    void destroyMaterial( uint32_t handle );

    uint32_t createMaterial( const MaterialDesc &desc );

    uint32_t createInstance( uint32_t meshHandle, uint32_t materialHandle );
    void destroyInstance( uint32_t handle );

    void setPosition( uint32_t handle, Vec3 position );

    uint32_t createLight( const LightDesc &desc );
    void destroyLight( uint32_t handle );

    void setAmbientLight( const Color &skyColor, const Color &groundColor );
    void setCamera( Vec3 position, Vec3 target );

private:
    // Split out so one try/catch at the call site covers every Ogre call in
    // the startup sequence.
    bool initializeInternal( const NativeWindowHandle &windowHandle, const EngineSettings &settings );

    // Both implementations are always registered, even if a project uses
    // only one: they are how Rhiza expresses 3D (Pbs) and 2D (Unlit), and a
    // scene may mix them freely.
    void registerHlms();

    // `name` must be unique across the whole Hlms manager.
    Ogre::HlmsDatablock *createDatablock( const std::string &name, const MaterialDesc &material );

    // Tracked by name rather than an Ogre::MeshPtr so this header doesn't
    // need Ogre's mesh headers - see the layer rule in ARCHITECTURE.md.
    struct MeshAsset
    {
        std::string name;
        bool isMutable = false;

        // Dropping an asset out from under a still-attached Item would leave
        // it pointing at freed memory, so destroy refuses while this is > 0.
        uint32_t instanceRefCount = 0;
    };

    struct MaterialAsset
    {
        Ogre::HlmsDatablock *datablock = nullptr;
        uint32_t instanceRefCount = 0;
    };

    // Remembers which asset and material it references, so destroyInstance
    // can decrement their ref-counts and updateMesh can find every Item that
    // needs telling its geometry changed.
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

    // One counter shared across every map below, so a handle value is never
    // ambiguous between them.
    uint32_t mNextHandle = 1;
    std::unordered_map<uint32_t, MeshAsset> mMeshAssets;
    std::unordered_map<uint32_t, MaterialAsset> mMaterials;
    std::unordered_map<uint32_t, Instance> mInstances;
    std::unordered_map<uint32_t, LightInstance> mLights;
};

}  // namespace Rhiza
