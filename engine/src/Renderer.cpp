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

// The exact byte layout uploaded to the GPU. Field order must stay in sync
// with the VertexElement2 declaration in createMesh().
struct GpuVertex
{
    float px, py, pz;
    float nx, ny, nz;
};

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

    registerHlms();

    mSceneManager = mRoot->createSceneManager( Ogre::ST_GENERIC, 1, "RhizaSceneManager" );

    // Without any ambient term, surfaces facing away from every light render
    // pure black. A dim sky/ground pair keeps unlit faces readable; callers
    // can override it via setAmbientLight().
    setAmbientLight( Color{ 0.3f, 0.35f, 0.45f, 1.0f }, Color{ 0.15f, 0.14f, 0.13f, 1.0f } );

    // Offset from the axis so a cube shows three faces at three different
    // brightnesses - straight-on, lighting is much harder to judge.
    mCamera = mSceneManager->createCamera( "MainCamera" );
    mCamera->setPosition( Ogre::Vector3( 3.5f, 3.0f, 5.0f ) );
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

    mSceneNodes.clear();
    mLightNodes.clear();

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

    // Position and normal interleaved in one buffer. The declared element
    // order here must match GpuVertex's field order exactly - Ogre reads the
    // buffer as raw bytes and trusts this declaration to interpret them.
    Ogre::VertexElement2Vec vertexElements;
    vertexElements.push_back( Ogre::VertexElement2( Ogre::VET_FLOAT3, Ogre::VES_POSITION ) );
    vertexElements.push_back( Ogre::VertexElement2( Ogre::VET_FLOAT3, Ogre::VES_NORMAL ) );

    const size_t numVertices = desc.vertices.size();
    GpuVertex *vertexData = reinterpret_cast<GpuVertex *>(
        OGRE_MALLOC_SIMD( sizeof( GpuVertex ) * numVertices, Ogre::MEMCATEGORY_GEOMETRY ) );

    Ogre::Aabb bounds = Ogre::Aabb::BOX_NULL;
    for( size_t i = 0; i < numVertices; ++i )
    {
        const Vertex &v = desc.vertices[i];
        vertexData[i] = { v.position.x, v.position.y, v.position.z,
                          v.normal.x,   v.normal.y,   v.normal.z };
        bounds.merge( Ogre::Vector3( v.position.x, v.position.y, v.position.z ) );
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

    item->getSubItem( 0 )->setDatablock( createDatablock( meshName + "_Material", desc.material ) );

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
    mLightNodes[handle] = node;
    return handle;
}

void Renderer::setAmbientLight( const Color &skyColor, const Color &groundColor )
{
    // Ogre models ambient as two hemispheres blended along an axis: light
    // bouncing down from the sky and up off the ground.
    mSceneManager->setAmbientLight( toOgre( skyColor ), toOgre( groundColor ),
                                    Ogre::Vector3::UNIT_Y );
}

}  // namespace Rhiza
