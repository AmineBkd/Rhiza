#include <Rhiza/Engine.h>

#include <algorithm>
#include <cmath>

#include "audio/Audio.h"
#include "core/AssetPath.h"
#include "core/Clock.h"
#include "core/Gltf.h"
#include "platform/AudioOutput.h"
#include "platform/FileSystem.h"
#include "platform/Input.h"
#include "platform/PlatformLifetime.h"
#include "render/Renderer.h"
#include "platform/Window.h"

namespace Rhiza
{
    struct Engine::Impl
    {
        PlatformLifetime platform;  // first, so it is destroyed last
        Window window;
        Renderer renderer;
        Input input;
        Clock clock;
        Audio audio;
        AudioOutput audioOutput;
        std::string assetRoot;
        bool running = false;
        bool listenerFollowsCamera = true;

        // Loads anyway: the spelling works on this file system, just not on
        // case-sensitive ones.
        void warnIfCaseDiffers( const std::string &cacheKey )
        {
            const std::string spelledOnDisk = findCaseMismatch( assetRoot, cacheKey );
            if( !spelledOnDisk.empty() )
                renderer.reportWarning( "asset '" + cacheKey + "' is spelled '" + spelledOnDisk +
                                        "' on disk. It loads here, but asset paths are "
                                        "case-sensitive, so it will fail on Linux and Android." );
        }

        bool readAsset( const std::string &cacheKey, std::vector<uint8_t> &bytes )
        {
            if( !readFile( joinAssetPath( assetRoot, cacheKey ), bytes ) )
                return false;
            warnIfCaseDiffers( cacheKey );
            return true;
        }

        SoundHandle loadSound( const char *path, uint32_t maxCopies )
        {
            const std::string cacheKey = normalizeAssetPath( path );
            SoundHandle handle;
            handle.id = audio.acquireCachedSound( cacheKey );
            if( handle.isValid() )
            {
                if( maxCopies > 0 )
                    audio.setMaxCopies( handle.id, maxCopies );
                return handle;
            }

            std::vector<uint8_t> bytes;
            if( !readAsset( cacheKey, bytes ) )
                return handle;
            std::string error;
            handle.id = audio.createSound( bytes, cacheKey, maxCopies, error );
            if( !handle.isValid() )
                renderer.reportError( "loadSound rejected '" + cacheKey + "' because " + error );
            return handle;
        }

        // An orthographic camera's height says nothing about zoom; its view
        // height does. Half of it puts the ears just above the play plane
        // (z = 0) at normal zoom, and far enough away to quieten everything
        // when zoomed far out.
        static constexpr float kOrthoListenerHeightPerViewHeight = 0.5f;

        void listenFrom( const CameraDesc &camera )
        {
            Vec3 position = camera.position;
            if( camera.projection == Projection::Orthographic )
                position.z = camera.orthoHeight * kOrthoListenerHeightPerViewHeight;
            audio.setListener( position, camera.orientation.rotate( { 0.0f, 0.0f, -1.0f } ),
                               camera.orientation.rotate( { 0.0f, 1.0f, 0.0f } ), {} );
        }

        void listenFrom( Vec3 position, Vec3 target )
        {
            Vec3 forward{ target.x - position.x, target.y - position.y, target.z - position.z };
            const float length = std::sqrt( forward.x * forward.x + forward.y * forward.y + forward.z * forward.z );
            if( length > 0.0f )
                forward = { forward.x / length, forward.y / length, forward.z / length };
            audio.setListener( position, forward, { 0.0f, 1.0f, 0.0f }, {} );
        }
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

        mImpl->assetRoot = settings.assetRoot ? settings.assetRoot : defaultAssetRoot();

        // A missing or broken sound device leaves the game running silent.
        const auto toCount = []( int value ) { return static_cast<uint32_t>( std::max( value, 0 ) ); };
        Audio *audio = &mImpl->audio;
        const bool audioStarted =
            audio->initialize( toCount( settings.monoSoundSlots ), toCount( settings.stereoSoundSlots ),
                               toCount( settings.defaultMaxCopies ) ) &&
            mImpl->audioOutput.start( Audio::kSampleRate, Audio::kChannels,
                                      [audio]( float *frames, int frameCount ) {
                                          audio->mix( frames, static_cast<uint32_t>( frameCount ) );
                                      } );
        if( audioStarted )
        {
            mImpl->listenFrom( settings.cameraPosition, settings.cameraTarget );
        }
        else
        {
            // With no device pulling the mix, voices would never finish and
            // fill every slot.
            audio->shutdown();
            mImpl->renderer.reportWarning( "audio could not start; the game will run silent" );
        }

        mImpl->running = true;
        return true;
    }

    void Engine::shutdown()
    {
        if( !mImpl )
            return;

        // Output first: its callback reads from the mixer.
        mImpl->audioOutput.stop();
        mImpl->audio.shutdown();
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

        // Stop the loop rather than render into a dead device or a frame
        // that keeps throwing. The game asks deviceLost() and renderFailed()
        // afterwards to tell either apart from the user closing the window.
        if( mImpl->renderer.isDeviceLost() || mImpl->renderer.hasRenderFailed() )
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

    bool Engine::renderFailed() const
    {
        return mImpl && mImpl->renderer.hasRenderFailed();
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

    MeshHandle Engine::loadMesh( const char *path )
    {
        MeshHandle handle;
        if( !mImpl )
            return handle;

        const std::string cacheKey = normalizeAssetPath( path );
        handle.id = mImpl->renderer.acquireCachedMesh( cacheKey );
        if( handle.isValid() )
            return handle;

        const std::string fullPath = joinAssetPath( mImpl->assetRoot, cacheKey );
        std::vector<uint8_t> bytes;
        if( !readFile( fullPath, bytes ) )
            return handle;
        mImpl->warnIfCaseDiffers( cacheKey );

        MeshDesc desc;
        std::string error;
        if( !parseGltfMesh( bytes, fullPath, readFile, desc, error ) )
        {
            mImpl->renderer.reportError( "loadMesh rejected '" + cacheKey + "' because " + error );
            return handle;
        }

        handle.id = mImpl->renderer.createMeshAsset( desc, cacheKey );
        return handle;
    }

    TextureHandle Engine::loadTexture( const char *path, const TextureDesc &desc )
    {
        TextureHandle handle;
        if( !mImpl )
            return handle;

        const std::string cacheKey = normalizeAssetPath( path );
        handle.id = mImpl->renderer.acquireCachedTexture( cacheKey, desc );
        if( handle.isValid() )
            return handle;

        std::vector<uint8_t> encoded;
        if( !readFile( joinAssetPath( mImpl->assetRoot, cacheKey ), encoded ) )
            return handle;
        mImpl->warnIfCaseDiffers( cacheKey );

        handle.id = mImpl->renderer.createTexture( encoded, cacheKey, desc );
        return handle;
    }

    void Engine::destroyTexture( TextureHandle texture )
    {
        if( mImpl && texture.isValid() )
            mImpl->renderer.destroyTexture( texture.id );
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

    void Engine::setTransform( InstanceHandle handle, const Transform &transform )
    {
        if( mImpl && handle.isValid() )
            mImpl->renderer.setTransform( handle.id, transform );
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
        if( !mImpl )
            return;
        mImpl->renderer.setCamera( position, target );
        if( mImpl->listenerFollowsCamera )
            mImpl->listenFrom( position, target );
    }

    void Engine::setCamera( const CameraDesc &camera )
    {
        if( !mImpl )
            return;
        mImpl->renderer.setCamera( camera );
        if( mImpl->listenerFollowsCamera )
            mImpl->listenFrom( camera );
    }

    SoundHandle Engine::loadSound( const char *path )
    {
        return mImpl ? mImpl->loadSound( path, 0 ) : SoundHandle{};
    }

    SoundHandle Engine::loadSound( const char *path, int maxCopies )
    {
        return mImpl ? mImpl->loadSound( path, static_cast<uint32_t>( std::max( maxCopies, 1 ) ) ) : SoundHandle{};
    }

    void Engine::destroySound( SoundHandle sound )
    {
        if( mImpl && sound.isValid() )
            mImpl->audio.destroySound( sound.id );
    }

    VoiceHandle Engine::playSound( SoundHandle sound, const PlayDesc &desc )
    {
        VoiceHandle handle;
        if( mImpl && sound.isValid() )
        {
            const Audio::VoiceId voice = mImpl->audio.play( sound.id, desc );
            handle = VoiceHandle{ voice.id, voice.generation };
        }
        return handle;
    }

    void Engine::stopVoice( VoiceHandle voice )
    {
        if( mImpl && voice.isValid() )
            mImpl->audio.stop( { voice.id, voice.generation } );
    }

    void Engine::setVoicePosition( VoiceHandle voice, Vec3 position, Vec3 velocity )
    {
        if( mImpl && voice.isValid() )
            mImpl->audio.setVoicePosition( { voice.id, voice.generation }, position, velocity );
    }

    bool Engine::isVoicePlaying( VoiceHandle voice ) const
    {
        return mImpl && voice.isValid() && mImpl->audio.isPlaying( { voice.id, voice.generation } );
    }

    void Engine::playMusic( const char *path, float fadeSeconds )
    {
        if( !mImpl )
            return;
        const std::string cacheKey = normalizeAssetPath( path );
        std::vector<uint8_t> bytes;
        if( !mImpl->readAsset( cacheKey, bytes ) )
            return;
        std::string error;
        if( !mImpl->audio.playMusic( std::move( bytes ), fadeSeconds, error ) )
            mImpl->renderer.reportError( "playMusic rejected '" + cacheKey + "' because " + error );
    }

    void Engine::stopMusic( float fadeSeconds )
    {
        if( mImpl )
            mImpl->audio.stopMusic( fadeSeconds );
    }

    void Engine::setBusVolume( AudioBus bus, float volume )
    {
        if( mImpl )
            mImpl->audio.setBusVolume( bus, volume );
    }

    void Engine::setBusPaused( AudioBus bus, bool paused )
    {
        if( mImpl )
            mImpl->audio.setBusPaused( bus, paused );
    }

    void Engine::setMasterVolume( float volume )
    {
        if( mImpl )
            mImpl->audio.setMasterVolume( volume );
    }

    void Engine::setListener( const ListenerDesc &listener )
    {
        if( !mImpl )
            return;
        mImpl->listenerFollowsCamera = false;
        mImpl->audio.setListener( listener.position, listener.orientation.rotate( { 0.0f, 0.0f, -1.0f } ),
                                  listener.orientation.rotate( { 0.0f, 1.0f, 0.0f } ), listener.velocity );
    }

    void Engine::followCameraWithListener()
    {
        if( mImpl )
            mImpl->listenerFollowsCamera = true;
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
