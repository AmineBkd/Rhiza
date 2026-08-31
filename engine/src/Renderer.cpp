#include "Renderer.h"

#include <cstring>

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
#include "Compositor/OgreCompositorManager2.h"
#include "Compositor/OgreCompositorWorkspace.h"
#include "Vao/OgreVaoManager.h"
#include "Vao/OgreVertexArrayObject.h"
#include "Hlms/Unlit/OgreHlmsUnlit.h"
#include "Hlms/Unlit/OgreHlmsUnlitDatablock.h"

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

// Debug and Release builds load each other's OgreNextMain library if
// mismatched, so the choice must track how *this* binary itself was built,
// not what happens to exist on disk - hence RHIZA_DEBUG_BUILD (set
// explicitly by engine/CMakeLists.txt from the actual CMake build
// configuration) rather than probing the filesystem or relying on ambient
// debug macros, whose exact trigger conditions vary by toolchain.
std::string getRenderSystemPluginPath()
{
#if defined( _WIN32 )
    constexpr const char *libPrefix = "";
    constexpr const char *libExtension = ".dll";
    constexpr const char *runtimeSubdir = "bin";
#elif defined( __APPLE__ )
    constexpr const char *libPrefix = "lib";
    constexpr const char *libExtension = ".dylib";
    constexpr const char *runtimeSubdir = "lib";
#else
    constexpr const char *libPrefix = "lib";
    constexpr const char *libExtension = ".so";
    constexpr const char *runtimeSubdir = "lib";
#endif

#if defined( RHIZA_DEBUG_BUILD )
    const std::string dir = std::string( "debug/" ) + runtimeSubdir;
    const std::string debugSuffix = "_d";
#else
    const std::string dir = runtimeSubdir;
    const std::string debugSuffix;
#endif

    return std::string( RHIZA_OGRE_INSTALL_ROOT ) + "/" + dir + "/" + libPrefix + kPluginBaseName +
           debugSuffix + libExtension;
}

}  // namespace

Renderer::~Renderer()
{
    shutdown();
}

bool Renderer::initialize( const NativeWindowHandle &windowHandle, const std::string &title,
                           int width, int height )
{
    const Ogre::AbiCookie abiCookie = Ogre::generateAbiCookie();
    mRoot = OGRE_NEW Ogre::Root( &abiCookie, Ogre::BLANKSTRING, Ogre::BLANKSTRING, "Rhiza.log", title );

    mRoot->loadPlugin( getRenderSystemPluginPath(), false, nullptr );

    Ogre::RenderSystem *renderSystem = mRoot->getRenderSystemByName( kRenderSystemName );
    if( !renderSystem )
        return false;

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

    mRenderWindow = mRoot->createRenderWindow( title, static_cast<Ogre::uint32>( width ),
                                               static_cast<Ogre::uint32>( height ), false, &params );

    registerUnlitHlms();

    mSceneManager = mRoot->createSceneManager( Ogre::ST_GENERIC, 1, "RhizaSceneManager" );

    mCamera = mSceneManager->createCamera( "MainCamera" );
    mCamera->setPosition( Ogre::Vector3( 0.0f, 0.0f, 5.0f ) );
    mCamera->lookAt( Ogre::Vector3::ZERO );
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

void Renderer::registerUnlitHlms()
{
    Ogre::String mainFolderPath;
    Ogre::StringVector libraryFoldersPaths;
    Ogre::HlmsUnlit::getDefaultPaths( mainFolderPath, libraryFoldersPaths );

    Ogre::ArchiveManager &archiveManager = Ogre::ArchiveManager::getSingleton();
    const Ogre::String mediaRoot = std::string( RHIZA_MEDIA_DIR ) + "/";

    Ogre::Archive *mainArchive = archiveManager.load( mediaRoot + mainFolderPath, "FileSystem", true );

    Ogre::ArchiveVec libraryFolders;
    for( const Ogre::String &libPath : libraryFoldersPaths )
        libraryFolders.push_back( archiveManager.load( mediaRoot + libPath, "FileSystem", true ) );

    Ogre::HlmsUnlit *hlmsUnlit = OGRE_NEW Ogre::HlmsUnlit( mainArchive, &libraryFolders );
    Ogre::HlmsManager *hlmsManager = mRoot->getHlmsManager();
    hlmsManager->registerHlms( hlmsUnlit );

    // HlmsManager defaults to treating HLMS_PBS as "the" default Hlms - e.g.
    // for submeshes with no material assigned, which is exactly our
    // generated meshes. We only register Unlit, so without this,
    // HlmsManager::getDefaultDatablock() dereferences a null HlmsPbs pointer.
    hlmsManager->useDefaultDatablockFrom( Ogre::HLMS_UNLIT );
}

void Renderer::shutdown()
{
    if( !mRoot )
        return;

    mSceneNodes.clear();

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
    Ogre::VaoManager *vaoManager = mRoot->getRenderSystem()->getVaoManager();

    Ogre::VertexElement2Vec vertexElements;
    vertexElements.push_back( Ogre::VertexElement2( Ogre::VET_FLOAT3, Ogre::VES_POSITION ) );

    const size_t numVertices = desc.vertices.size();
    Ogre::Vector3 *vertexData = reinterpret_cast<Ogre::Vector3 *>(
        OGRE_MALLOC_SIMD( sizeof( Ogre::Vector3 ) * numVertices, Ogre::MEMCATEGORY_GEOMETRY ) );

    Ogre::Aabb bounds = Ogre::Aabb::BOX_NULL;
    for( size_t i = 0; i < numVertices; ++i )
    {
        const Vec3 &p = desc.vertices[i].position;
        vertexData[i] = Ogre::Vector3( p.x, p.y, p.z );
        bounds.merge( vertexData[i] );
    }

    Ogre::VertexBufferPacked *vertexBuffer =
        vaoManager->createVertexBuffer( vertexElements, numVertices, Ogre::BT_IMMUTABLE, vertexData,
                                        true );

    const size_t numIndices = desc.indices.size();
    Ogre::uint16 *indexData = reinterpret_cast<Ogre::uint16 *>(
        OGRE_MALLOC_SIMD( sizeof( Ogre::uint16 ) * numIndices, Ogre::MEMCATEGORY_GEOMETRY ) );
    std::memcpy( indexData, desc.indices.data(), sizeof( Ogre::uint16 ) * numIndices );

    Ogre::IndexBufferPacked *indexBuffer = vaoManager->createIndexBuffer(
        Ogre::IndexBufferPacked::IT_16BIT, numIndices, Ogre::BT_IMMUTABLE, indexData, true );

    Ogre::VertexBufferPackedVec vertexBuffers;
    vertexBuffers.push_back( vertexBuffer );
    Ogre::VertexArrayObject *vao =
        vaoManager->createVertexArrayObject( vertexBuffers, indexBuffer, Ogre::OT_TRIANGLE_LIST );

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

    Ogre::Hlms *hlms = mRoot->getHlmsManager()->getHlms( Ogre::HLMS_UNLIT );
    Ogre::HlmsUnlit *hlmsUnlit = static_cast<Ogre::HlmsUnlit *>( hlms );

    // MeshDesc is a generic "here are some vertices and indices" API with no
    // documented winding convention, so we can't assume callers give us
    // triangles wound the way Ogre-Next's default cull mode expects
    // (CULL_CLOCKWISE keeps only anticlockwise-wound triangles - the exact
    // opposite of what it sounds like). Disabling culling is the correct
    // choice for arbitrary caller-supplied geometry, not a workaround.
    Ogre::HlmsMacroblock macroblock;
    macroblock.mCullMode = Ogre::CULL_NONE;

    Ogre::HlmsUnlitDatablock *datablock = static_cast<Ogre::HlmsUnlitDatablock *>(
        hlmsUnlit->createDatablock( meshName + "_Material", meshName + "_Material", macroblock,
                                    Ogre::HlmsBlendblock(), Ogre::HlmsParamVec() ) );
    datablock->setUseColour( true );
    datablock->setColour(
        Ogre::ColourValue( desc.color.r, desc.color.g, desc.color.b, desc.color.a ) );
    item->getSubItem( 0 )->setDatablock( datablock );

    Ogre::SceneNode *sceneNode = mSceneManager->getRootSceneNode( Ogre::SCENE_DYNAMIC )
                                     ->createChildSceneNode( Ogre::SCENE_DYNAMIC );
    sceneNode->attachObject( item );

    mSceneNodes[handle] = sceneNode;
    return handle;
}

void Renderer::setPosition( uint32_t handle, Vec3 position )
{
    auto it = mSceneNodes.find( handle );
    if( it == mSceneNodes.end() )
        return;
    it->second->setPosition( position.x, position.y, position.z );
}

}  // namespace Rhiza
