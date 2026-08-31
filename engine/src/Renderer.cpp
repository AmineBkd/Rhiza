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

// The vcpkg ogre-next port only enables one render system per platform by
// default (see its vcpkg.json feature conditions): Direct3D11 on Windows,
// GL3Plus on Linux, Metal on macOS/iOS. Each ships as a standalone plugin
// library rather than something you link against (see FindOgreNext.cmake
// for why), loaded by hand at runtime via its Ogre-internal display name.
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

// Ogre-Next's ogre_config_plugin() clears the library prefix on GCC/Clang,
// so a plugin never carries the leading "lib" that the Unix convention would
// otherwise give it, and its OGRE_PLUGIN_PATH puts the file somewhere other
// than beside the regular libraries - differently on each platform:
//   Windows  <root>/bin/RenderSystem_Direct3D11.dll   (RUNTIME destination)
//   macOS    <root>/lib/RenderSystem_Metal.dylib      (OGRE_PLUGIN_PATH "/")
//   Linux    <root>/lib/OGRE-Next/RenderSystem_GL3Plus.so
//                                        (OGRE_PLUGIN_PATH "/OGRE-Next")
// Note the Linux plugin is additionally built with RUNPATH "$ORIGIN:$ORIGIN/.."
// so it can resolve libOgreNextMain and the image/zip dependencies one level
// up: it has to be loaded from that directory in place, not copied next to
// the executable.
//
// Debug and Release builds load each other's OgreNextMain library if
// mismatched, so the choice must track how *this* binary itself was built,
// not what happens to exist on disk - hence RHIZA_DEBUG_BUILD (set
// explicitly by engine/CMakeLists.txt from the actual CMake build
// configuration) rather than probing the filesystem or relying on ambient
// debug macros, whose exact trigger conditions vary by toolchain.
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
// with the VertexElement2 declaration in createMesh().
struct GpuVertex
{
    float px, py, pz;
    float nx, ny, nz;
};

// Owns a block of OGRE_MALLOC_SIMD memory until Ogre takes it over. The Vao
// creation calls adopt the pointer when they succeed - we always pass
// keepAsShadow = true - but if one throws, freeing it is still our job.
// Holding it here means the throwing path cannot leak.
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

// Returns an empty string when the description can be turned into a mesh,
// otherwise a human-readable reason why it cannot. Checking up front means
// a caller mistake produces a log line rather than an Ogre assertion or,
// worse, an out-of-range index that reads garbage on the GPU.
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
    // Ogre signals nearly every startup failure by throwing - a missing
    // render system plugin, an unusable window handle, a missing Hlms
    // template folder. Rhiza's contract is a false return, so the whole
    // sequence is contained here and translated at this boundary.
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

    // "externalWindowHandle" means the same thing on every platform Ogre-Next
    // supports: render directly into a window we already created and own
    // (as opposed to letting Ogre create and own its own OS window). Only
    // the underlying value differs - HWND, X11 XID, or NSWindow* - which
    // Window::getNativeHandle() already resolved into one portable integer.
    Ogre::NameValuePairList params;
    params["externalWindowHandle"] = Ogre::StringConverter::toString( windowHandle.value );
    params["vsync"] = "Yes";

    mRenderWindow =
        mRoot->createRenderWindow( title, static_cast<Ogre::uint32>( settings.windowWidth ),
                                   static_cast<Ogre::uint32>( settings.windowHeight ), false,
                                   &params );

    registerHlms();

    mSceneManager = mRoot->createSceneManager( Ogre::ST_GENERIC, 1, "RhizaSceneManager" );

    // Without any ambient term, surfaces facing away from every light render
    // pure black. A dim sky/ground pair keeps unlit faces readable; callers
    // can override it via setAmbientLight().
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

    // Both implementations describe the folders they need to compile their
    // shaders from, and the paths are relative to our media root. The
    // folders themselves are vendored in engine/media/Hlms - see the
    // ATTRIBUTION.txt there for why they aren't supplied by vcpkg.
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

    // HlmsManager treats HLMS_PBS as the fallback for anything with no
    // material assigned - which every Item briefly is, in the moment between
    // createItem() and our setDatablock() call. Registering Pbs above is
    // what makes that fallback valid; when only Unlit was registered this
    // null-dereferenced inside HlmsManager::getDefaultDatablock().
}

void Renderer::shutdown()
{
    if( !mRoot )
        return;

    // Ogre::Root's destructor tears down the scene manager and everything
    // in it, so these only need forgetting, not individually destroying.
    mMeshes.clear();
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

uint32_t Renderer::createMesh( const MeshDesc &desc )
{
    const std::string problem = describeProblem( desc );
    if( !problem.empty() )
    {
        logError( "createMesh rejected a mesh because " + problem );
        return 0;
    }

    Ogre::VaoManager *vaoManager = mRoot->getRenderSystem()->getVaoManager();

    // Position and normal interleaved in one buffer. The declared element
    // order here must match GpuVertex's field order exactly - Ogre reads the
    // buffer as raw bytes and trusts this declaration to interpret them.
    Ogre::VertexElement2Vec vertexElements;
    vertexElements.push_back( Ogre::VertexElement2( Ogre::VET_FLOAT3, Ogre::VES_POSITION ) );
    vertexElements.push_back( Ogre::VertexElement2( Ogre::VET_FLOAT3, Ogre::VES_NORMAL ) );

    const size_t numVertices = desc.vertices.size();
    const size_t numIndices = desc.indices.size();

    SimdArray<GpuVertex> vertexData( numVertices );
    Ogre::Aabb bounds = Ogre::Aabb::BOX_NULL;
    for( size_t i = 0; i < numVertices; ++i )
    {
        const Vertex &v = desc.vertices[i];
        vertexData[i] = { v.position.x, v.position.y, v.position.z,
                          v.normal.x,   v.normal.y,   v.normal.z };
        bounds.merge( Ogre::Vector3( v.position.x, v.position.y, v.position.z ) );
    }

    SimdArray<Ogre::uint16> indexData( numIndices );
    std::memcpy( indexData.get(), desc.indices.data(), sizeof( Ogre::uint16 ) * numIndices );

    Ogre::VertexBufferPacked *vertexBuffer = nullptr;
    Ogre::IndexBufferPacked *indexBuffer = nullptr;
    Ogre::VertexArrayObject *vao = nullptr;
    try
    {
        vertexBuffer = vaoManager->createVertexBuffer( vertexElements, numVertices,
                                                       Ogre::BT_IMMUTABLE, vertexData.get(), true );
        vertexData.release();

        indexBuffer = vaoManager->createIndexBuffer( Ogre::IndexBufferPacked::IT_16BIT, numIndices,
                                                     Ogre::BT_IMMUTABLE, indexData.get(), true );
        indexData.release();

        Ogre::VertexBufferPackedVec vertexBuffers;
        vertexBuffers.push_back( vertexBuffer );
        vao = vaoManager->createVertexArrayObject( vertexBuffers, indexBuffer,
                                                   Ogre::OT_TRIANGLE_LIST );
    }
    catch( Ogre::Exception &e )
    {
        // Whatever succeeded before the throw is still ours: nothing has
        // taken ownership of these yet, since only a SubMesh does that.
        if( indexBuffer )
            vaoManager->destroyIndexBuffer( indexBuffer );
        if( vertexBuffer )
            vaoManager->destroyVertexBuffer( vertexBuffer );

        logError( "createMesh could not allocate GPU buffers: " + e.getDescription() );
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

    Ogre::Item *item = mSceneManager->createItem( mesh, Ogre::SCENE_DYNAMIC );

    item->getSubItem( 0 )->setDatablock( createDatablock( meshName + "_Material", desc.material ) );

    Ogre::SceneNode *sceneNode = mSceneManager->getRootSceneNode( Ogre::SCENE_DYNAMIC )
                                     ->createChildSceneNode( Ogre::SCENE_DYNAMIC );
    sceneNode->attachObject( item );

    MeshInstance instance;
    instance.node = sceneNode;
    instance.item = item;
    instance.meshName = meshName;
    instance.datablockName = meshName + "_Material";
    mMeshes[handle] = std::move( instance );

    return handle;
}

void Renderer::destroyMesh( uint32_t handle )
{
    auto it = mMeshes.find( handle );
    if( it == mMeshes.end() )
        return;

    const MeshInstance &instance = it->second;

    // Order matters: the Item has to go before its datablock, so that the
    // datablock has no renderables still pointing at it when destroyed.
    mSceneManager->destroyItem( instance.item );
    mSceneManager->destroySceneNode( instance.node );

    // Removing the mesh resource cascades - ~SubMesh destroys its Vaos and,
    // through them, the vertex and index buffers. Ogre also handles our
    // sharing one Vao between VpNormal and VpShadow without double-freeing.
    Ogre::MeshManager::getSingleton().remove( instance.meshName );

    Ogre::HlmsDatablock *datablock =
        mRoot->getHlmsManager()->getDatablockNoDefault( instance.datablockName );
    if( datablock )
        datablock->getCreator()->destroyDatablock( instance.datablockName );

    mMeshes.erase( it );
}

void Renderer::setPosition( uint32_t handle, Vec3 position )
{
    auto it = mMeshes.find( handle );
    if( it == mMeshes.end() )
        return;
    it->second.node->setPosition( position.x, position.y, position.z );
}

Ogre::HlmsDatablock *Renderer::createDatablock( const std::string &name,
                                                const MaterialDesc &material )
{
    // Winding order is only trustworthy when the caller promised it, so
    // culling stays off unless the material opts in. CULL_CLOCKWISE is
    // Ogre's default and, confusingly, means "keep anticlockwise faces".
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

    // Ogre's PBS divides incoming light by PI, which is correct for an HDR
    // pipeline that later tonemaps. Rhiza renders straight to an LDR target,
    // so we fold PI back in here and let callers think in plain multiples.
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
