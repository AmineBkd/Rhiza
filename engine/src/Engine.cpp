#include <Rhiza/Engine.h>
#include "core/Clock.h"
#include "platform/Input.h"
#include "render/Renderer.h"
#include "platform/Window.h"

namespace Rhiza
{
    struct Engine::Impl
    {
        Window window;
        Renderer renderer;
        Input input;
        Clock clock;
        bool running = false;
    };

    Engine::Engine() = default;

    Engine::~Engine() {
        shutdown();
    }

    bool Engine::initialize( const EngineSettings &settings )
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

    void Engine::shutdown()
    {
        if( !mImpl )
            return;

        mImpl->renderer.shutdown();
        mImpl->window.shutdown();
        mImpl.reset();
    }

    bool Engine::beginFrame()
    {
        if( !mImpl || !mImpl->running )
            return false;

        // Measures the previous whole iteration - the conventional meaning
        // of "this frame's delta time".
        mImpl->clock.tick();

        if( !mImpl->window.pollEvents( mImpl->input ) )
        {
            mImpl->running = false;
            return false;
        }

        // Stop the loop rather than render into a dead device. The game asks
        // deviceLost() afterwards to decide between a clean exit and a
        // save-and-restart.
        if( mImpl->renderer.isDeviceLost() )
        {
            mImpl->running = false;
            return false;
        }

        return true;
    }

    bool Engine::deviceLost() const
    {
        return mImpl && mImpl->renderer.isDeviceLost();
    }

    void Engine::endFrame()
    {
        if( mImpl && mImpl->running )
            mImpl->renderer.renderOneFrame();
    }

    MeshHandle Engine::createMeshAsset( const MeshDesc &desc )
    {
        MeshHandle handle;
        if( mImpl )
            handle.id = mImpl->renderer.createMeshAsset( desc );
        return handle;
    }

    void Engine::updateMesh( MeshHandle mesh, const MeshDesc &desc )
    {
        if( mImpl && mesh.isValid() )
            mImpl->renderer.updateMesh( mesh.id, desc );
    }

    void Engine::destroyMeshAsset( MeshHandle mesh )
    {
        if( mImpl && mesh.isValid() )
            mImpl->renderer.destroyMeshAsset( mesh.id );
    }

    MaterialHandle Engine::createMaterial( const MaterialDesc &desc )
    {
        MaterialHandle handle;
        if( mImpl )
            handle.id = mImpl->renderer.createMaterial( desc );
        return handle;
    }

    void Engine::destroyMaterial( MaterialHandle material )
    {
        if( mImpl && material.isValid() )
            mImpl->renderer.destroyMaterial( material.id );
    }

    InstanceHandle Engine::createInstance( MeshHandle mesh, MaterialHandle material )
    {
        InstanceHandle handle;
        if( mImpl && mesh.isValid() && material.isValid() )
            handle.id = mImpl->renderer.createInstance( mesh.id, material.id );
        return handle;
    }

    void Engine::destroyInstance( InstanceHandle instance )
    {
        if( mImpl && instance.isValid() )
            mImpl->renderer.destroyInstance( instance.id );
    }

    void Engine::setPosition( InstanceHandle handle, Vec3 position )
    {
        if( mImpl && handle.isValid() )
            mImpl->renderer.setPosition( handle.id, position );
    }

    LightHandle Engine::createLight( const LightDesc &desc )
    {
        LightHandle handle;
        if( mImpl )
            handle.id = mImpl->renderer.createLight( desc );
        return handle;
    }

    void Engine::destroyLight( LightHandle handle )
    {
        if( mImpl && handle.isValid() )
            mImpl->renderer.destroyLight( handle.id );
    }

    void Engine::setAmbientLight( Color skyColor, Color groundColor )
    {
        if( mImpl )
            mImpl->renderer.setAmbientLight( skyColor, groundColor );
    }

    void Engine::setCamera( Vec3 position, Vec3 target )
    {
        if( mImpl )
            mImpl->renderer.setCamera( position, target );
    }

    bool Engine::isKeyDown( Key key ) const
    {
        return mImpl && mImpl->input.isKeyDown( key );
    }

    bool Engine::wasKeyPressed( Key key ) const
    {
        return mImpl && mImpl->input.wasKeyPressed( key );
    }

    bool Engine::wasKeyReleased( Key key ) const
    {
        return mImpl && mImpl->input.wasKeyReleased( key );
    }

    float Engine::deltaSeconds() const
    {
        return mImpl ? mImpl->clock.scaledDeltaSeconds() : 0.0f;
    }

    float Engine::unscaledDeltaSeconds() const
    {
        return mImpl ? mImpl->clock.realDeltaSeconds() : 0.0f;
    }

    float Engine::timeScale() const
    {
        return mImpl ? mImpl->clock.timeScale() : 1.0f;
    }

    void Engine::setTimeScale( float scale )
    {
        if( mImpl )
            mImpl->clock.setTimeScale( scale );
    }

}  // namespace Rhiza
