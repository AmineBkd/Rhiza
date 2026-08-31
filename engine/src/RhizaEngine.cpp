#include <Rhiza/RhizaEngine.h>
#include "Clock.h"
#include "Input.h"
#include "Renderer.h"
#include "Window.h"

namespace Rhiza
{
    struct RhizaEngine::Impl
    {
        Window window;
        Renderer renderer;
        Input input;
        Clock clock;
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
        if( !mImpl->renderer.initialize( handle, settings ) )
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

        // Measures the time the *previous* iteration of this loop took
        // (everything below, plus whatever the OS/compositor made us wait
        // for) - the conventional meaning of "this frame's delta time".
        mImpl->clock.tick();

        if( !mImpl->window.pollEvents( mImpl->input ) )
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

    void RhizaEngine::destroyMesh( SceneNodeHandle handle )
    {
        if( mImpl && handle.isValid() )
            mImpl->renderer.destroyMesh( handle.id );
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

    void RhizaEngine::destroyLight( LightHandle handle )
    {
        if( mImpl && handle.isValid() )
            mImpl->renderer.destroyLight( handle.id );
    }

    void RhizaEngine::setAmbientLight( Color skyColor, Color groundColor )
    {
        if( mImpl )
            mImpl->renderer.setAmbientLight( skyColor, groundColor );
    }

    void RhizaEngine::setCamera( Vec3 position, Vec3 target )
    {
        if( mImpl )
            mImpl->renderer.setCamera( position, target );
    }

    bool RhizaEngine::isKeyDown( Key key ) const
    {
        return mImpl && mImpl->input.isKeyDown( key );
    }

    bool RhizaEngine::wasKeyPressed( Key key ) const
    {
        return mImpl && mImpl->input.wasKeyPressed( key );
    }

    bool RhizaEngine::wasKeyReleased( Key key ) const
    {
        return mImpl && mImpl->input.wasKeyReleased( key );
    }

    float RhizaEngine::deltaSeconds() const
    {
        return mImpl ? mImpl->clock.scaledDeltaSeconds() : 0.0f;
    }

    float RhizaEngine::unscaledDeltaSeconds() const
    {
        return mImpl ? mImpl->clock.realDeltaSeconds() : 0.0f;
    }

    float RhizaEngine::timeScale() const
    {
        return mImpl ? mImpl->clock.timeScale() : 1.0f;
    }

    void RhizaEngine::setTimeScale( float scale )
    {
        if( mImpl )
            mImpl->clock.setTimeScale( scale );
    }

}  // namespace Rhiza
