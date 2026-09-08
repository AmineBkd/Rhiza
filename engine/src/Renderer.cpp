#include "Renderer.h"

#include <cstdio>
#include <cstring>
#include <string>

#include "OgreRoot.h"
#include "OgreAbiUtils.h"
#include "OgreRenderSystem.h"
#include "OgreWindow.h"
#include "OgreSceneManager.h"
#include "OgreSceneNode.h"
#include "OgreCamera.h"
#include "OgreItem.h"
#include "OgreSubItem.h"
#include "OgreMesh2.h"
#include "OgreSubMesh2.h"
#include "OgreMeshManager2.h"
#include "OgreHlmsManager.h"
#include "OgreHlmsCommon.h"
#include "OgreArchiveManager.h"
#include "OgreLogManager.h"
#include "Compositor/OgreCompositorManager2.h"
#include "Compositor/OgreCompositorWorkspace.h"
#include "Vao/OgreVaoManager.h"
#include "Vao/OgreVertexArrayObject.h"
#include "OgreLight.h"
#include "Hlms/Unlit/OgreHlmsUnlit.h"
#include "Hlms/Unlit/OgreHlmsUnlitDatablock.h"
#include "Hlms/Pbs/OgreHlmsPbs.h"
#include "Hlms/Pbs/OgreHlmsPbsDatablock.h"

// Injected by engine/CMakeLists.txt. RHIZA_MEDIA_DIR is the folder holding
// this engine's own copy of the Hlms shader templates (see engine/media);
// RHIZA_OGRE_INSTALL_ROOT is the vcpkg triplet install root that holds the
// RenderSystem_Direct3D11 plugin DLL (see cmake/FindOgreNext.cmake).
#ifndef RHIZA_MEDIA_DIR
#    error "RHIZA_MEDIA_DIR must be defined by CMake"
#endif
#ifndef RHIZA_OGRE_INSTALL_ROOT
#    error "RHIZA_OGRE_INSTALL_ROOT must be defined by CMake"
#endif

namespace Rhiza
{

namespace
{

// The vcpkg ogre-next port enables one render system per platform. Each
// ships as a runtime-loaded plugin rather than something you link against
// (see FindOgreNext.cmake), found by its Ogre-internal display name.
#if defined( _WIN32 )
constexpr const char *kPluginBaseName = "RenderSystem_Direct3D11";
constexpr const char *kRenderSystemName = "Direct3D11 Rendering Subsystem";
#elif defined( __APPLE__ )
constexpr const char *kPluginBaseName = "RenderSystem_Metal";
constexpr const char *kRenderSystemName = "Metal Rendering Subsystem";
#elif defined( __linux__ )
constexpr const char *kPluginBaseName = "RenderSystem_GL3Plus";
constexpr const char *kRenderSystemName = "OpenGL 3+ Rendering Subsystem";
#else
#    error "Unsupported platform: add its Ogre-Next render system plugin name here."
#endif

// Plugins never carry the Unix "lib" prefix (ogre_config_plugin clears it)
// and land somewhere other than beside the regular libraries, differently
// per platform:
//   Windows  <root>/bin/RenderSystem_Direct3D11.dll
//   macOS    <root>/lib/RenderSystem_Metal.dylib
//   Linux    <root>/lib/OGRE-Next/RenderSystem_GL3Plus.so
// The Linux one has RUNPATH "$ORIGIN:$ORIGIN/.." to resolve libOgreNextMain
// one level up, so it must be loaded in place, not copied beside the exe.
//
// A mismatched Debug/Release pair silently loads the wrong OgreNextMain, so
// this tracks how *this* binary was built (RHIZA_DEBUG_BUILD, set by
// engine/CMakeLists.txt) rather than probing the filesystem or trusting
// ambient debug macros.
std::string getRenderSystemPluginPath()
{
#if defined( _WIN32 )
    constexpr const char *libExtension = ".dll";
    constexpr const char *pluginSubdir = "bin";
#elif defined( __APPLE__ )
    constexpr const char *libExtension = ".dylib";
    constexpr const char *pluginSubdir = "lib";
#else
    constexpr const char *libExtension = ".so";
    constexpr const char *pluginSubdir = "lib/OGRE-Next";
#endif

#if defined( RHIZA_DEBUG_BUILD )
    const std::string dir = std::string( "debug/" ) + pluginSubdir;
    const std::string debugSuffix = "_d";
#else
    const std::string dir = pluginSubdir;
    const std::string debugSuffix;
#endif

    return std::string( RHIZA_OGRE_INSTALL_ROOT ) + "/" + dir + "/" + kPluginBaseName +
           debugSuffix + libExtension;
}

// The exact byte layout uploaded to the GPU. Field order must stay in sync
// with the VertexElement2 declaration in buildVao().
struct GpuVertex
{
    float px, py, pz;
    float nx, ny, nz;
};

// Owns OGRE_MALLOC_SIMD memory until Ogre takes it over. The Vao calls
// adopt the pointer on success (keepAsShadow = true), but if one throws,
// freeing it is still our job - holding it here means that path can't leak.
template <typename T>
class SimdArray
{
public:
    explicit SimdArray( size_t count ) :
        mPtr( static_cast<T *>(
            OGRE_MALLOC_SIMD( sizeof( T ) * count, Ogre::MEMCATEGORY_GEOMETRY ) ) )
    {
    }

    ~SimdArray()
    {
        if( mPtr )
            OGRE_FREE_SIMD( mPtr, Ogre::MEMCATEGORY_GEOMETRY );
    }

    SimdArray( const SimdArray & ) = delete;
    SimdArray &operator=( const SimdArray & ) = delete;

    T *get() const { return mPtr; }
    T &operator[]( size_t i ) { return mPtr[i]; }

    // Ogre now owns the block; stop tracking it.
    void release() { mPtr = nullptr; }

private:
    T *mPtr;
};

// Empty string if the description is usable, otherwise why it isn't.
// Checking up front turns a caller mistake into a log line rather than an
// Ogre assertion or an out-of-range index reading garbage on the GPU.
std::string describeProblem( const MeshDesc &desc )
{
    if( desc.vertices.empty() )
        return "it has no vertices";
    if( desc.indices.empty() )
        return "it has no indices";
    if( desc.indices.size() % 3u != 0u )
        return "its index count " + std::to_string( desc.indices.size() ) +
               " is not a multiple of 3, so it is not a triangle list";

    // MeshDesc::indices is uint16_t, so anything past 65536 vertices simply
    // cannot be addressed. Say so rather than silently ignoring the tail.
    if( desc.vertices.size() > 65536u )
        return "it has " + std::to_string( desc.vertices.size() ) +
               " vertices, more than 16-bit indices can address (65536)";

    const size_t vertexCount = desc.vertices.size();
    for( size_t i = 0; i < desc.indices.size(); ++i )
    {
        if( desc.indices[i] >= vertexCount )
        {
            return "index " + std::to_string( i ) + " refers to vertex " +
                   std::to_string( desc.indices[i] ) + ", but there are only " +
                   std::to_string( vertexCount );
        }
    }

    return {};
}

// Builds the position+normal interleaved Vao. Shared by createMeshAsset and
// updateMesh's rebuild path so the two can't drift on element layout.
// Throws Ogre::Exception on allocation failure; callers own the try/catch.
Ogre::VertexArrayObject *buildVao( const MeshDesc &desc, Ogre::VaoManager *vaoManager,
                                   Ogre::BufferType bufferType, Ogre::Aabb &outBounds )
{
    // Element order must match GpuVertex exactly: Ogre reads the buffer as
    // raw bytes and trusts this declaration to interpret them.
    Ogre::VertexElement2Vec vertexElements;
    vertexElements.push_back( Ogre::VertexElement2( Ogre::VET_FLOAT3, Ogre::VES_POSITION ) );
    vertexElements.push_back( Ogre::VertexElement2( Ogre::VET_FLOAT3, Ogre::VES_NORMAL ) );

    const size_t numVertices = desc.vertices.size();
    const size_t numIndices = desc.indices.size();

    SimdArray<GpuVertex> vertexData( numVertices );
    outBounds = Ogre::Aabb::BOX_NULL;
    for( size_t i = 0; i < numVertices; ++i )
    {
        const Vertex &v = desc.vertices[i];
        vertexData[i] = { v.position.x, v.position.y, v.position.z,
                          v.normal.x,   v.normal.y,   v.normal.z };
        outBounds.merge( Ogre::Vector3( v.position.x, v.position.y, v.position.z ) );
    }

    SimdArray<Ogre::uint16> indexData( numIndices );
    std::memcpy( indexData.get(), desc.indices.data(), sizeof( Ogre::uint16 ) * numIndices );

    Ogre::VertexBufferPacked *vertexBuffer = nullptr;
    Ogre::IndexBufferPacked *indexBuffer = nullptr;
    try
    {
        vertexBuffer = vaoManager->createVertexBuffer( vertexElements, numVertices, bufferType,
                                                       vertexData.get(), true );
        vertexData.release();

        indexBuffer = vaoManager->createIndexBuffer( Ogre::IndexBufferPacked::IT_16BIT, numIndices,
                                                     bufferType, indexData.get(), true );
        indexData.release();

        Ogre::VertexBufferPackedVec vertexBuffers;
        vertexBuffers.push_back( vertexBuffer );
        return vaoManager->createVertexArrayObject( vertexBuffers, indexBuffer,
                                                     Ogre::OT_TRIANGLE_LIST );
    }
    catch( Ogre::Exception & )
    {
        // Whatever succeeded before the throw is still ours: nothing has
        // taken ownership of these yet, since only a SubMesh's Vao does.
        if( indexBuffer )
            vaoManager->destroyIndexBuffer( indexBuffer );
        if( vertexBuffer )
            vaoManager->destroyVertexBuffer( vertexBuffer );
        throw;
    }
}

// The inverse of buildVao: frees a Vao and the buffers behind it. Used by
// updateMesh's rebuild path to drop the old geometry before building the new.
void destroyVao( Ogre::VertexArrayObject *vao, Ogre::VaoManager *vaoManager )
{
    Ogre::IndexBufferPacked *indexBuffer = vao->getIndexBuffer();
    const Ogre::VertexBufferPackedVec vertexBuffers = vao->getVertexBuffers();

    vaoManager->destroyVertexArrayObject( vao );
    if( indexBuffer )
        vaoManager->destroyIndexBuffer( indexBuffer );
    for( Ogre::VertexBufferPacked *vertexBuffer : vertexBuffers )
        vaoManager->destroyVertexBuffer( vertexBuffer );
}

// Ogre's log is the one place these messages are useful, but it only exists
// once Root has been constructed - and initialize() can fail before that.
void logError( const std::string &message )
{
    if( Ogre::LogManager::getSingletonPtr() )
        Ogre::LogManager::getSingleton().logMessage( "[Rhiza] " + message, Ogre::LML_CRITICAL );
    else
        fprintf( stderr, "[Rhiza] %s\n", message.c_str() );
}

Ogre::Vector3 toOgre( const Vec3 &v )
{
    return Ogre::Vector3( v.x, v.y, v.z );
}

Ogre::ColourValue toOgre( const Color &c )
{
    return Ogre::ColourValue( c.r, c.g, c.b, c.a );
}

}  // namespace

Renderer::~Renderer()
{
    shutdown();
}

bool Renderer::initialize( const NativeWindowHandle &windowHandle, const EngineSettings &settings )
{
    // Ogre signals nearly every startup failure by throwing; Rhiza's
    // contract is a false return, so the whole sequence translates here.
    try
    {
        return initializeInternal( windowHandle, settings );
    }
    catch( Ogre::Exception &e )
    {
        logError( "renderer initialization failed: " + e.getDescription() );
        return false;
    }
}

bool Renderer::initializeInternal( const NativeWindowHandle &windowHandle,
                                   const EngineSettings &settings )
{
    const std::string title = settings.windowTitle;

    const Ogre::AbiCookie abiCookie = Ogre::generateAbiCookie();
    mRoot = OGRE_NEW Ogre::Root( &abiCookie, Ogre::BLANKSTRING, Ogre::BLANKSTRING, "Rhiza.log", title );

    mRoot->loadPlugin( getRenderSystemPluginPath(), false, nullptr );

    Ogre::RenderSystem *renderSystem = mRoot->getRenderSystemByName( kRenderSystemName );
    if( !renderSystem )
    {
        logError( std::string( "render system '" ) + kRenderSystemName +
                  "' was not found after loading " + getRenderSystemPluginPath() );
        return false;
    }

    mRoot->setRenderSystem( renderSystem );
    mRoot->initialise( false );

    // "externalWindowHandle" means the same thing everywhere: render into a
    // window we already own rather than one Ogre creates. Only the
    // underlying value differs, which getNativeHandle() already resolved.
    Ogre::NameValuePairList params;
    params["externalWindowHandle"] = Ogre::StringConverter::toString( windowHandle.value );
    params["vsync"] = "Yes";

    mRenderWindow =
        mRoot->createRenderWindow( title, static_cast<Ogre::uint32>( settings.windowWidth ),
                                   static_cast<Ogre::uint32>( settings.windowHeight ), false,
                                   &params );

    registerHlms();

    mSceneManager = mRoot->createSceneManager( Ogre::ST_GENERIC, 1, "RhizaSceneManager" );

    // Without an ambient term, surfaces facing away from every light render
    // pure black. Callers can override via setAmbientLight().
    setAmbientLight( Color{ 0.3f, 0.35f, 0.45f, 1.0f }, Color{ 0.15f, 0.14f, 0.13f, 1.0f } );

    mCamera = mSceneManager->createCamera( "MainCamera" );
    setCamera( settings.cameraPosition, settings.cameraTarget );
    mCamera->setNearClipDistance( 0.1f );
    mCamera->setFarClipDistance( 1000.0f );
    mCamera->setAutoAspectRatio( true );

    Ogre::CompositorManager2 *compositorManager = mRoot->getCompositorManager2();
    const Ogre::String workspaceName( "RhizaWorkspace" );
    const Ogre::ColourValue backgroundColour( 0.15f, 0.15f, 0.2f );
    if( !compositorManager->hasWorkspaceDefinition( workspaceName ) )
        compositorManager->createBasicWorkspaceDef( workspaceName, backgroundColour, Ogre::IdString() );
    mWorkspace = compositorManager->addWorkspace( mSceneManager, mRenderWindow->getTexture(), mCamera,
                                                  workspaceName, true );

    return true;
}

void Renderer::registerHlms()
{
    Ogre::ArchiveManager &archiveManager = Ogre::ArchiveManager::getSingleton();
    const Ogre::String mediaRoot = std::string( RHIZA_MEDIA_DIR ) + "/";

    // Each implementation names the shader-template folders it needs,
    // relative to our media root. They're vendored in engine/media/Hlms -
    // see ATTRIBUTION.txt there for why vcpkg doesn't supply them.
    auto loadArchives = [&]( const Ogre::String &mainFolderPath,
                             const Ogre::StringVector &libraryFoldersPaths,
                             Ogre::ArchiveVec &outLibraryFolders ) -> Ogre::Archive * {
        for( const Ogre::String &libPath : libraryFoldersPaths )
            outLibraryFolders.push_back( archiveManager.load( mediaRoot + libPath, "FileSystem", true ) );
        return archiveManager.load( mediaRoot + mainFolderPath, "FileSystem", true );
    };

    Ogre::HlmsManager *hlmsManager = mRoot->getHlmsManager();

    Ogre::String mainFolderPath;
    Ogre::StringVector libraryFoldersPaths;

    {
        Ogre::HlmsUnlit::getDefaultPaths( mainFolderPath, libraryFoldersPaths );
        Ogre::ArchiveVec libraryFolders;
        Ogre::Archive *mainArchive = loadArchives( mainFolderPath, libraryFoldersPaths, libraryFolders );
        hlmsManager->registerHlms( OGRE_NEW Ogre::HlmsUnlit( mainArchive, &libraryFolders ) );
    }

    {
        Ogre::HlmsPbs::getDefaultPaths( mainFolderPath, libraryFoldersPaths );
        Ogre::ArchiveVec libraryFolders;
        Ogre::Archive *mainArchive = loadArchives( mainFolderPath, libraryFoldersPaths, libraryFolders );
        hlmsManager->registerHlms( OGRE_NEW Ogre::HlmsPbs( mainArchive, &libraryFolders ) );
    }

    // HlmsManager falls back to HLMS_PBS for anything with no material -
    // which every Item briefly is, between createItem() and setDatablock().
    // With only Unlit registered this null-dereferenced inside
    // HlmsManager::getDefaultDatablock().
}

void Renderer::shutdown()
{
    if( !mRoot )
        return;

    // ~Root tears down the scene manager and everything in it, so these
    // only need forgetting, not individually destroying.
    mMeshAssets.clear();
    mMaterials.clear();
    mInstances.clear();
    mLights.clear();

    if( mWorkspace )
    {
        mRoot->getCompositorManager2()->removeWorkspace( mWorkspace );
        mWorkspace = nullptr;
    }
    if( mSceneManager )
    {
        mRoot->destroySceneManager( mSceneManager );
        mSceneManager = nullptr;
    }

    OGRE_DELETE mRoot;
    mRoot = nullptr;
    mRenderWindow = nullptr;
    mCamera = nullptr;
}

void Renderer::renderOneFrame()
{
    mRoot->renderOneFrame();
}

uint32_t Renderer::createMeshAsset( const MeshDesc &desc )
{
    const std::string problem = describeProblem( desc );
    if( !problem.empty() )
    {
        logError( "createMeshAsset rejected a mesh because " + problem );
        return 0;
    }

    Ogre::VaoManager *vaoManager = mRoot->getRenderSystem()->getVaoManager();
    const Ogre::BufferType bufferType = desc.isMutable ? Ogre::BT_DEFAULT : Ogre::BT_IMMUTABLE;

    Ogre::VertexArrayObject *vao = nullptr;
    Ogre::Aabb bounds = Ogre::Aabb::BOX_NULL;
    try
    {
        vao = buildVao( desc, vaoManager, bufferType, bounds );
    }
    catch( Ogre::Exception &e )
    {
        logError( "createMeshAsset could not allocate GPU buffers: " + e.getDescription() );
        return 0;
    }

    const uint32_t handle = mNextHandle++;
    const Ogre::String meshName = "RhizaMesh_" + Ogre::StringConverter::toString( handle );

    Ogre::MeshPtr mesh = Ogre::MeshManager::getSingleton().createManual(
        meshName, Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME );
    Ogre::SubMesh *subMesh = mesh->createSubMesh();
    subMesh->mVao[Ogre::VpNormal].push_back( vao );
    subMesh->mVao[Ogre::VpShadow].push_back( vao );

    mesh->_setBounds( bounds, false );
    mesh->_setBoundingSphereRadius( bounds.getRadius() );

    MeshAsset asset;
    asset.name = meshName;
    asset.isMutable = desc.isMutable;
    mMeshAssets[handle] = std::move( asset );

    return handle;
}

void Renderer::updateMesh( uint32_t handle, const MeshDesc &desc )
{
    auto it = mMeshAssets.find( handle );
    if( it == mMeshAssets.end() )
    {
        logError( "updateMesh given a mesh handle that does not exist" );
        return;
    }

    MeshAsset &asset = it->second;
    if( !asset.isMutable )
    {
        logError( "updateMesh called on '" + asset.name +
                  "', which was not created with MeshDesc::isMutable" );
        return;
    }

    const std::string problem = describeProblem( desc );
    if( !problem.empty() )
    {
        logError( "updateMesh rejected new geometry for '" + asset.name + "' because " + problem );
        return;
    }

    Ogre::MeshPtr mesh = Ogre::MeshManager::getSingleton().getByName(
        asset.name, Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME );
    Ogre::SubMesh *subMesh = mesh->getSubMesh( 0 );
    Ogre::VertexArrayObject *oldVao = subMesh->mVao[Ogre::VpNormal][0];
    Ogre::VaoManager *vaoManager = mRoot->getRenderSystem()->getVaoManager();

    // Same counts as before: a plain re-upload into the existing buffers,
    // no GPU allocation. The common path for deformation without topology
    // change.
    const Ogre::VertexBufferPackedVec &vertexBuffers = oldVao->getVertexBuffers();
    const bool sameSize = vertexBuffers.size() == 1 &&
                          vertexBuffers[0]->getNumElements() == desc.vertices.size() &&
                          oldVao->getIndexBuffer() != nullptr &&
                          oldVao->getIndexBuffer()->getNumElements() == desc.indices.size();

    if( sameSize )
    {
        std::vector<GpuVertex> vertexData( desc.vertices.size() );
        for( size_t i = 0; i < desc.vertices.size(); ++i )
        {
            const Vertex &v = desc.vertices[i];
            vertexData[i] = { v.position.x, v.position.y, v.position.z,
                              v.normal.x,   v.normal.y,   v.normal.z };
        }
        vertexBuffers[0]->upload( vertexData.data(), 0, vertexData.size() );
        oldVao->getIndexBuffer()->upload( desc.indices.data(), 0, desc.indices.size() );

        Ogre::Aabb bounds = Ogre::Aabb::BOX_NULL;
        for( const Vertex &v : desc.vertices )
            bounds.merge( Ogre::Vector3( v.position.x, v.position.y, v.position.z ) );
        mesh->_setBounds( bounds, false );
        mesh->_setBoundingSphereRadius( bounds.getRadius() );
    }
    else
    {
        // Topology changed, so the buffers are the wrong size and there is
        // no partial-upload path - replace them outright.
        Ogre::Aabb bounds = Ogre::Aabb::BOX_NULL;
        Ogre::VertexArrayObject *newVao = nullptr;
        try
        {
            newVao = buildVao( desc, vaoManager, Ogre::BT_DEFAULT, bounds );
        }
        catch( Ogre::Exception &e )
        {
            logError( "updateMesh could not allocate GPU buffers for '" + asset.name +
                      "': " + e.getDescription() );
            return;
        }

        destroyVao( oldVao, vaoManager );
        subMesh->mVao[Ogre::VpNormal][0] = newVao;
        subMesh->mVao[Ogre::VpShadow][0] = newVao;
        mesh->_setBounds( bounds, false );
        mesh->_setBoundingSphereRadius( bounds.getRadius() );
    }

    // Every Item cached its own Vao at creation and has no idea it just
    // changed. _initialise(true) forces the rebuild - "useful if you changed
    // the content of a Mesh or Skeleton at runtime".
    // https://ogrecave.github.io/ogre-next/api/latest/class_ogre_1_1_item.html
    for( auto &instanceEntry : mInstances )
    {
        if( instanceEntry.second.meshAssetHandle == handle )
            instanceEntry.second.item->_initialise( true );
    }
}

void Renderer::destroyMeshAsset( uint32_t handle )
{
    auto it = mMeshAssets.find( handle );
    if( it == mMeshAssets.end() )
        return;

    if( it->second.instanceRefCount != 0 )
    {
        logError( "destroyMeshAsset refused: '" + it->second.name + "' still has " +
                  std::to_string( it->second.instanceRefCount ) + " instance(s) referencing it" );
        return;
    }

    // Cascades: ~SubMesh destroys its Vaos and the buffers behind them, and
    // handles our one Vao shared between VpNormal and VpShadow without
    // double-freeing.
    Ogre::MeshManager::getSingleton().remove( it->second.name );
    mMeshAssets.erase( it );
}

uint32_t Renderer::createMaterial( const MaterialDesc &desc )
{
    const uint32_t handle = mNextHandle++;
    const Ogre::String name = "RhizaMaterial_" + Ogre::StringConverter::toString( handle );

    MaterialAsset asset;
    asset.datablock = createDatablock( name, desc );
    mMaterials[handle] = asset;

    return handle;
}

void Renderer::destroyMaterial( uint32_t handle )
{
    auto it = mMaterials.find( handle );
    if( it == mMaterials.end() )
        return;

    if( it->second.instanceRefCount != 0 )
    {
        logError( "destroyMaterial refused: still has " +
                  std::to_string( it->second.instanceRefCount ) + " instance(s) referencing it" );
        return;
    }

    Ogre::HlmsDatablock *datablock = it->second.datablock;
    datablock->getCreator()->destroyDatablock( datablock->getName() );
    mMaterials.erase( it );
}

uint32_t Renderer::createInstance( uint32_t meshHandle, uint32_t materialHandle )
{
    auto meshIt = mMeshAssets.find( meshHandle );
    if( meshIt == mMeshAssets.end() )
    {
        logError( "createInstance given a mesh handle that does not exist" );
        return 0;
    }
    auto materialIt = mMaterials.find( materialHandle );
    if( materialIt == mMaterials.end() )
    {
        logError( "createInstance given a material handle that does not exist" );
        return 0;
    }

    Ogre::MeshPtr mesh = Ogre::MeshManager::getSingleton().getByName(
        meshIt->second.name, Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME );

    Ogre::Item *item = mSceneManager->createItem( mesh, Ogre::SCENE_DYNAMIC );
    item->getSubItem( 0 )->setDatablock( materialIt->second.datablock );

    Ogre::SceneNode *sceneNode = mSceneManager->getRootSceneNode( Ogre::SCENE_DYNAMIC )
                                     ->createChildSceneNode( Ogre::SCENE_DYNAMIC );
    sceneNode->attachObject( item );

    const uint32_t handle = mNextHandle++;
    Instance instance;
    instance.node = sceneNode;
    instance.item = item;
    instance.meshAssetHandle = meshHandle;
    instance.materialHandle = materialHandle;
    mInstances[handle] = instance;

    ++meshIt->second.instanceRefCount;
    ++materialIt->second.instanceRefCount;

    return handle;
}

void Renderer::destroyInstance( uint32_t handle )
{
    auto it = mInstances.find( handle );
    if( it == mInstances.end() )
        return;

    const Instance &instance = it->second;

    mSceneManager->destroyItem( instance.item );
    mSceneManager->destroySceneNode( instance.node );

    auto meshIt = mMeshAssets.find( instance.meshAssetHandle );
    if( meshIt != mMeshAssets.end() )
        --meshIt->second.instanceRefCount;

    auto materialIt = mMaterials.find( instance.materialHandle );
    if( materialIt != mMaterials.end() )
        --materialIt->second.instanceRefCount;

    mInstances.erase( it );
}

void Renderer::setPosition( uint32_t handle, Vec3 position )
{
    auto it = mInstances.find( handle );
    if( it == mInstances.end() )
        return;
    it->second.node->setPosition( position.x, position.y, position.z );
}

Ogre::HlmsDatablock *Renderer::createDatablock( const std::string &name,
                                                const MaterialDesc &material )
{
    // Culling stays off unless the material opts in, since winding is only
    // trustworthy when the caller promised it. CULL_CLOCKWISE is Ogre's
    // default and, confusingly, means "keep anticlockwise faces".
    Ogre::HlmsMacroblock macroblock;
    macroblock.mCullMode = material.doubleSided ? Ogre::CULL_NONE : Ogre::CULL_CLOCKWISE;

    Ogre::HlmsManager *hlmsManager = mRoot->getHlmsManager();

    if( material.shading == ShadingModel::Unlit )
    {
        Ogre::HlmsUnlit *hlmsUnlit =
            static_cast<Ogre::HlmsUnlit *>( hlmsManager->getHlms( Ogre::HLMS_UNLIT ) );

        Ogre::HlmsUnlitDatablock *datablock =
            static_cast<Ogre::HlmsUnlitDatablock *>( hlmsUnlit->createDatablock(
                name, name, macroblock, Ogre::HlmsBlendblock(), Ogre::HlmsParamVec() ) );

        // Unlit ignores its colour entirely unless told to use it.
        datablock->setUseColour( true );
        datablock->setColour( toOgre( material.color ) );
        return datablock;
    }

    Ogre::HlmsPbs *hlmsPbs = static_cast<Ogre::HlmsPbs *>( hlmsManager->getHlms( Ogre::HLMS_PBS ) );

    Ogre::HlmsPbsDatablock *datablock =
        static_cast<Ogre::HlmsPbsDatablock *>( hlmsPbs->createDatablock(
            name, name, macroblock, Ogre::HlmsBlendblock(), Ogre::HlmsParamVec() ) );

    // setMetalness is only respected under the metallic workflow; in the
    // default specular workflow it is silently ignored.
    datablock->setWorkflow( Ogre::HlmsPbsDatablock::MetallicWorkflow );
    datablock->setDiffuse( Ogre::Vector3( material.color.r, material.color.g, material.color.b ) );
    datablock->setRoughness( material.roughness );
    datablock->setMetalness( material.metalness );
    return datablock;
}

uint32_t Renderer::createLight( const LightDesc &desc )
{
    Ogre::Light *light = mSceneManager->createLight();
    Ogre::SceneNode *node = mSceneManager->getRootSceneNode()->createChildSceneNode();
    node->attachObject( light );

    light->setDiffuseColour( toOgre( desc.color ) );
    light->setSpecularColour( toOgre( desc.color ) );

    // Ogre's PBS divides incoming light by PI, correct for an HDR pipeline
    // that later tonemaps. Rhiza renders straight to LDR, so fold it back in
    // and let callers think in plain multiples.
    light->setPowerScale( desc.power * Ogre::Math::PI );

    if( desc.type == LightType::Directional )
    {
        light->setType( Ogre::Light::LT_DIRECTIONAL );
        light->setDirection( toOgre( desc.direction ).normalisedCopy() );
    }
    else
    {
        light->setType( Ogre::Light::LT_POINT );
        node->setPosition( toOgre( desc.position ) );
    }

    const uint32_t handle = mNextHandle++;
    mLights[handle] = LightInstance{ node, light };
    return handle;
}

void Renderer::destroyLight( uint32_t handle )
{
    auto it = mLights.find( handle );
    if( it == mLights.end() )
        return;

    mSceneManager->destroyLight( it->second.light );
    mSceneManager->destroySceneNode( it->second.node );
    mLights.erase( it );
}

void Renderer::setAmbientLight( const Color &skyColor, const Color &groundColor )
{
    // Ogre models ambient as two hemispheres blended along an axis: light
    // bouncing down from the sky and up off the ground.
    mSceneManager->setAmbientLight( toOgre( skyColor ), toOgre( groundColor ),
                                    Ogre::Vector3::UNIT_Y );
}

void Renderer::setCamera( Vec3 position, Vec3 target )
{
    mCamera->setPosition( toOgre( position ) );
    mCamera->lookAt( toOgre( target ) );
}

}  // namespace Rhiza
