#include <Rhiza/RhizaEngine.h>
#include "Renderer.h"
#include "Window.h"

namespace Rhiza
{
    struct RhizaEngine::Impl
    {
        Window window;
        Renderer renderer;
        bool running = false;
    };

    RhizaEngine::RhizaEngine() = default;

    RhizaEngine::~RhizaEngine() {
        shutdown();
    }

    bool RhizaEngine::initialize( const EngineSettings &settings )
    {
        mImpl = std::make_unique<Impl>();

        if( !mImpl->window.initialize( settings.windowTitle, settings.windowWidth, settings.windowHeight ) )
        {
            mImpl.reset();
            return false;
        }

        const NativeWindowHandle handle = mImpl->window.getNativeHandle();
        if( !mImpl->renderer.initialize( handle, settings.windowTitle, settings.windowWidth,
                                         settings.windowHeight ) )
        {
            mImpl->window.shutdown();
            mImpl.reset();
            return false;
        }

        mImpl->running = true;
        return true;
    }

    void RhizaEngine::shutdown()
    {
        if( !mImpl )
            return;

        mImpl->renderer.shutdown();
        mImpl->window.shutdown();
        mImpl.reset();
    }

    bool RhizaEngine::tick()
    {
        if( !mImpl || !mImpl->running )
            return false;

        if( !mImpl->window.pollEvents() )
        {
            mImpl->running = false;
            return false;
        }

        mImpl->renderer.renderOneFrame();
        return true;
    }

    SceneNodeHandle RhizaEngine::createMesh( const MeshDesc &desc )
    {
        SceneNodeHandle handle;
        if( mImpl )
            handle.id = mImpl->renderer.createMesh( desc );
        return handle;
    }

    void RhizaEngine::setPosition( SceneNodeHandle handle, Vec3 position )
    {
        if( mImpl && handle.isValid() )
            mImpl->renderer.setPosition( handle.id, position );
    }

    LightHandle RhizaEngine::createLight( const LightDesc &desc )
    {
        LightHandle handle;
        if( mImpl )
            handle.id = mImpl->renderer.createLight( desc );
        return handle;
    }

    void RhizaEngine::setAmbientLight( Color skyColor, Color groundColor )
    {
        if( mImpl )
            mImpl->renderer.setAmbientLight( skyColor, groundColor );
    }

}  // namespace Rhiza
