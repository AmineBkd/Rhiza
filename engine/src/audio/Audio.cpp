#include "Audio.h"

#include <algorithm>
#include <cstring>
#include <mutex>
#include <unordered_map>

#include "Miniaudio.h"
#include "core/AssetCache.h"

namespace Rhiza
{

namespace
{

constexpr uint64_t kDecodeChunkFrames = 4096;
constexpr size_t kBusCount = 3;  // one per AudioBus

ma_uint64 toMilliseconds( float seconds )
{
    return seconds > 0.0f ? static_cast<ma_uint64>( seconds * 1000.0f ) : 0;
}

}  // namespace

struct Audio::Impl
{
    struct Sound
    {
        std::vector<int16_t> samples;
        uint32_t channels = 1;
        uint64_t frames = 0;
        uint32_t maxCopies = 1;

        // Kept current by reapFinishedVoices().
        uint32_t playingCopies = 0;
        uint64_t slotTakenAt = 0;
    };

    struct Voice
    {
        ma_audio_buffer_ref source;
        ma_sound sound;
        uint32_t generation = 0;
        uint32_t soundHandle = 0;  // 0 while free
        uint64_t startedAt = 0;
        AudioBus bus = AudioBus::Sfx;
    };

    struct Pool
    {
        uint32_t firstVoice = 0;
        uint32_t voiceCount = 0;
        uint32_t slots = 0;
        uint32_t channels = 1;
        uint32_t soundsPlaying = 0;
    };

    struct MusicTrack
    {
        std::vector<uint8_t> encoded;  // the decoder reads straight from this
        ma_decoder decoder;
        ma_sound sound;
        bool initialised = false;
    };

    ma_engine engine;
    ma_sound_group buses[kBusCount];  // indexed by AudioBus
    size_t readyBuses = 0;
    bool initialised = false;

    // Created once and never resized: miniaudio holds pointers into them.
    std::unique_ptr<Voice[]> voices;
    uint32_t voiceCount = 0;
    uint32_t readyVoices = 0;
    Pool pools[2];  // mono, stereo
    uint32_t defaultMaxCopies = 4;
    uint64_t nextPlayOrder = 1;

    MusicTrack music[2];
    int currentMusic = 0;

    std::unordered_map<uint32_t, Sound> sounds;
    AssetCache soundCache;
    uint32_t nextSoundHandle = 1;

    std::mutex mutex;

    Pool &poolFor( const Sound &sound ) { return pools[sound.channels == 1 ? 0 : 1]; }
    ma_sound_group &bus( AudioBus which ) { return buses[static_cast<size_t>( which )]; }

    Voice *findVoice( VoiceId id )
    {
        if( id.id == 0 || id.id > voiceCount )
            return nullptr;
        Voice &voice = voices[id.id - 1];
        return voice.generation == id.generation && voice.soundHandle != 0 ? &voice : nullptr;
    }

    // The voice stops counting against its sound and its sound's slot.
    void releaseVoice( Voice &voice )
    {
        auto it = sounds.find( voice.soundHandle );
        if( it != sounds.end() && it->second.playingCopies > 0 )
        {
            Sound &sound = it->second;
            if( --sound.playingCopies == 0 )
                --poolFor( sound ).soundsPlaying;
        }
        voice.soundHandle = 0;
    }

    // Counts lag behind sounds that ended on their own until this runs.
    void reapFinishedVoices()
    {
        for( uint32_t i = 0; i < readyVoices; ++i )
        {
            if( voices[i].soundHandle != 0 && !ma_sound_is_playing( &voices[i].sound ) )
                releaseVoice( voices[i] );
        }
    }

    void stopAllCopies( uint32_t soundHandle )
    {
        if( soundHandle == 0 )  // would match every free voice
            return;
        for( uint32_t i = 0; i < readyVoices; ++i )
        {
            Voice &voice = voices[i];
            if( voice.soundHandle != soundHandle )
                continue;
            ma_sound_stop( &voice.sound );
            ma_audio_buffer_ref_set_data( &voice.source, nullptr, 0 );
            releaseVoice( voice );
        }
    }

    Voice *oldestCopy( uint32_t soundHandle )
    {
        Voice *oldest = nullptr;
        for( uint32_t i = 0; i < readyVoices; ++i )
        {
            Voice &voice = voices[i];
            if( voice.soundHandle == soundHandle && ( !oldest || voice.startedAt < oldest->startedAt ) )
                oldest = &voice;
        }
        return oldest;
    }

    uint32_t oldestSlot( const Pool &pool )
    {
        uint32_t oldest = 0;
        uint64_t oldestTakenAt = 0;
        for( const auto &entry : sounds )
        {
            const Sound &sound = entry.second;
            if( sound.playingCopies == 0 || &poolFor( sound ) != &pool )
                continue;
            if( oldest == 0 || sound.slotTakenAt < oldestTakenAt )
            {
                oldest = entry.first;
                oldestTakenAt = sound.slotTakenAt;
            }
        }
        return oldest;
    }

    // A free voice, or the pool's oldest if per-sound overrides above the
    // default have used them all.
    Voice &voiceToUse( const Pool &pool )
    {
        Voice *oldest = nullptr;
        for( uint32_t i = pool.firstVoice; i < pool.firstVoice + pool.voiceCount; ++i )
        {
            Voice &voice = voices[i];
            if( voice.soundHandle == 0 )
                return voice;
            if( !oldest || voice.startedAt < oldest->startedAt )
                oldest = &voice;
        }
        return *oldest;
    }

    void releaseTrack( MusicTrack &track )
    {
        if( track.initialised )
        {
            ma_sound_uninit( &track.sound );
            ma_decoder_uninit( &track.decoder );
            track.initialised = false;
        }
        track.encoded.clear();
    }

    void uninitialiseAll()
    {
        releaseTrack( music[0] );
        releaseTrack( music[1] );
        for( uint32_t i = 0; i < readyVoices; ++i )
        {
            ma_sound_uninit( &voices[i].sound );
            ma_audio_buffer_ref_uninit( &voices[i].source );
        }
        readyVoices = 0;
        voices.reset();
        voiceCount = 0;
        while( readyBuses > 0 )
            ma_sound_group_uninit( &buses[--readyBuses] );
        ma_engine_uninit( &engine );
        sounds.clear();
        soundCache.clear();
        initialised = false;
    }
};

Audio::Audio() : mImpl( std::make_unique<Impl>() ) {}

Audio::~Audio()
{
    shutdown();
}

bool Audio::initialize( uint32_t monoSlots, uint32_t stereoSlots, uint32_t defaultMaxCopies )
{
    std::lock_guard<std::mutex> lock( mImpl->mutex );
    Impl &a = *mImpl;
    if( a.initialised )
        return true;

    ma_engine_config config = ma_engine_config_init();
    config.noDevice = MA_TRUE;
    config.channels = kChannels;
    config.sampleRate = kSampleRate;
    config.listenerCount = 1;
    if( ma_engine_init( &config, &a.engine ) != MA_SUCCESS )
        return false;
    a.initialised = true;

    for( ma_sound_group &bus : a.buses )
    {
        if( ma_sound_group_init( &a.engine, 0, nullptr, &bus ) != MA_SUCCESS )
        {
            a.uninitialiseAll();
            return false;
        }
        ++a.readyBuses;
    }

    a.defaultMaxCopies = std::max( defaultMaxCopies, 1u );
    a.pools[0] = { 0, monoSlots * a.defaultMaxCopies, monoSlots, 1, 0 };
    a.pools[1] = { a.pools[0].voiceCount, stereoSlots * a.defaultMaxCopies, stereoSlots, 2, 0 };
    a.voiceCount = a.pools[0].voiceCount + a.pools[1].voiceCount;
    a.voices = std::make_unique<Impl::Voice[]>( a.voiceCount );

    for( uint32_t i = 0; i < a.voiceCount; ++i )
    {
        Impl::Voice &voice = a.voices[i];
        const ma_uint32 channels = i < a.pools[0].voiceCount ? 1 : 2;
        ma_audio_buffer_ref_init( ma_format_s16, channels, nullptr, 0, &voice.source );
        // miniaudio 0.11 leaves a buffer's rate at 0 ("TODO: 0.12"); say it
        // outright rather than depend on how 0 is read.
        voice.source.sampleRate = kSampleRate;
        if( ma_sound_init_from_data_source( &a.engine, &voice.source, 0, &a.bus( AudioBus::Sfx ),
                                            &voice.sound ) != MA_SUCCESS )
        {
            ma_audio_buffer_ref_uninit( &voice.source );
            a.uninitialiseAll();
            return false;
        }
        a.readyVoices = i + 1;
    }
    return true;
}

void Audio::shutdown()
{
    std::lock_guard<std::mutex> lock( mImpl->mutex );
    if( mImpl->initialised )
        mImpl->uninitialiseAll();
}

void Audio::mix( float *out, uint32_t frameCount )
{
    std::lock_guard<std::mutex> lock( mImpl->mutex );
    ma_uint64 framesRead = 0;
    if( mImpl->initialised )
        ma_engine_read_pcm_frames( &mImpl->engine, out, frameCount, &framesRead );
    if( framesRead < frameCount )
        std::memset( out + framesRead * kChannels, 0, ( frameCount - framesRead ) * kChannels * sizeof( float ) );
}

uint32_t Audio::createSound( const std::vector<uint8_t> &encoded, const std::string &cacheKey,
                             uint32_t maxCopies, std::string &error )
{
    // Probed first so mono stays mono; anything wider folds to stereo.
    ma_decoder probe;
    ma_decoder_config probeConfig = ma_decoder_config_init_default();
    if( ma_decoder_init_memory( encoded.data(), encoded.size(), &probeConfig, &probe ) != MA_SUCCESS )
    {
        error = "it is not a WAV, MP3, FLAC or Ogg Vorbis file";
        return 0;
    }
    ma_format sourceFormat;
    ma_uint32 sourceChannels = 0;
    ma_uint32 sourceRate = 0;
    ma_decoder_get_data_format( &probe, &sourceFormat, &sourceChannels, &sourceRate, nullptr, 0 );
    ma_decoder_uninit( &probe );

    Impl::Sound sound;
    sound.channels = sourceChannels >= 2 ? 2 : 1;

    ma_decoder decoder;
    ma_decoder_config config = ma_decoder_config_init( ma_format_s16, sound.channels, kSampleRate );
    if( ma_decoder_init_memory( encoded.data(), encoded.size(), &config, &decoder ) != MA_SUCCESS )
    {
        error = "it could not be decoded";
        return 0;
    }
    std::vector<int16_t> chunk( kDecodeChunkFrames * sound.channels );
    for( ;; )
    {
        ma_uint64 framesRead = 0;
        const ma_result result = ma_decoder_read_pcm_frames( &decoder, chunk.data(), kDecodeChunkFrames, &framesRead );
        sound.samples.insert( sound.samples.end(), chunk.begin(), chunk.begin() + framesRead * sound.channels );
        if( result != MA_SUCCESS || framesRead == 0 )
            break;
    }
    ma_decoder_uninit( &decoder );

    sound.frames = sound.samples.size() / sound.channels;
    if( sound.frames == 0 )
    {
        error = "it contains no audio";
        return 0;
    }

    std::lock_guard<std::mutex> lock( mImpl->mutex );
    sound.maxCopies = maxCopies > 0 ? maxCopies : mImpl->defaultMaxCopies;
    const uint32_t handle = mImpl->nextSoundHandle++;
    mImpl->sounds[handle] = std::move( sound );
    mImpl->soundCache.add( cacheKey, handle );
    return handle;
}

uint32_t Audio::acquireCachedSound( const std::string &cacheKey )
{
    std::lock_guard<std::mutex> lock( mImpl->mutex );
    return mImpl->soundCache.acquire( cacheKey );
}

void Audio::setMaxCopies( uint32_t soundHandle, uint32_t maxCopies )
{
    std::lock_guard<std::mutex> lock( mImpl->mutex );
    auto it = mImpl->sounds.find( soundHandle );
    if( it != mImpl->sounds.end() )
        it->second.maxCopies = std::max( maxCopies, 1u );
}

void Audio::destroySound( uint32_t soundHandle )
{
    std::lock_guard<std::mutex> lock( mImpl->mutex );
    Impl &a = *mImpl;
    if( a.sounds.find( soundHandle ) == a.sounds.end() || a.soundCache.releaseIfShared( soundHandle ) )
        return;

    a.stopAllCopies( soundHandle );
    a.sounds.erase( soundHandle );
    a.soundCache.forget( soundHandle );
}

Audio::VoiceId Audio::play( uint32_t soundHandle, const PlayDesc &desc )
{
    std::lock_guard<std::mutex> lock( mImpl->mutex );
    Impl &a = *mImpl;
    auto it = a.sounds.find( soundHandle );
    if( !a.initialised || it == a.sounds.end() )
        return {};
    Impl::Sound &sound = it->second;
    Impl::Pool &pool = a.poolFor( sound );
    if( pool.slots == 0 || pool.voiceCount == 0 )
        return {};

    a.reapFinishedVoices();

    Impl::Voice *voice;
    if( sound.playingCopies >= sound.maxCopies )
    {
        voice = a.oldestCopy( soundHandle );
    }
    else
    {
        if( sound.playingCopies == 0 && pool.soundsPlaying >= pool.slots )
            a.stopAllCopies( a.oldestSlot( pool ) );
        voice = &a.voiceToUse( pool );
    }
    if( voice->soundHandle != 0 )
    {
        ma_sound_stop( &voice->sound );
        a.releaseVoice( *voice );
    }

    ma_sound &s = voice->sound;
    if( voice->bus != desc.bus )
    {
        // Detaches it from the old bus too.
        ma_node_attach_output_bus( &s, 0, &a.bus( desc.bus ), 0 );
        voice->bus = desc.bus;
    }
    ma_audio_buffer_ref_set_data( &voice->source, sound.samples.data(), sound.frames );
    ma_sound_reset_stop_time_and_fade( &s );
    ma_sound_seek_to_pcm_frame( &s, 0 );
    ma_sound_set_volume( &s, desc.volume );
    ma_sound_set_pitch( &s, desc.pitch );
    ma_sound_set_looping( &s, desc.loop ? MA_TRUE : MA_FALSE );
    ma_sound_set_spatialization_enabled( &s, desc.positional ? MA_TRUE : MA_FALSE );
    if( desc.positional )
    {
        ma_sound_set_pan( &s, 0.0f );
        ma_sound_set_position( &s, desc.position.x, desc.position.y, desc.position.z );
        ma_sound_set_velocity( &s, desc.velocity.x, desc.velocity.y, desc.velocity.z );
        ma_sound_set_min_distance( &s, desc.minDistance );
        ma_sound_set_max_distance( &s, desc.maxDistance );
    }
    else
    {
        ma_sound_set_pan( &s, desc.pan );
    }
    ma_sound_start( &s );

    if( sound.playingCopies++ == 0 )
    {
        ++pool.soundsPlaying;
        sound.slotTakenAt = a.nextPlayOrder;
    }
    voice->soundHandle = soundHandle;
    voice->startedAt = a.nextPlayOrder++;
    ++voice->generation;
    return { static_cast<uint32_t>( voice - a.voices.get() ) + 1, voice->generation };
}

uint32_t Audio::playingCopies( uint32_t soundHandle )
{
    std::lock_guard<std::mutex> lock( mImpl->mutex );
    mImpl->reapFinishedVoices();
    auto it = mImpl->sounds.find( soundHandle );
    return it == mImpl->sounds.end() ? 0 : it->second.playingCopies;
}

void Audio::stop( VoiceId id )
{
    std::lock_guard<std::mutex> lock( mImpl->mutex );
    if( Impl::Voice *voice = mImpl->findVoice( id ) )
    {
        ma_sound_stop( &voice->sound );
        mImpl->releaseVoice( *voice );
    }
}

void Audio::setVoicePosition( VoiceId id, Vec3 position, Vec3 velocity )
{
    std::lock_guard<std::mutex> lock( mImpl->mutex );
    if( Impl::Voice *voice = mImpl->findVoice( id ) )
    {
        ma_sound_set_position( &voice->sound, position.x, position.y, position.z );
        ma_sound_set_velocity( &voice->sound, velocity.x, velocity.y, velocity.z );
    }
}

bool Audio::isPlaying( VoiceId id ) const
{
    std::lock_guard<std::mutex> lock( mImpl->mutex );
    Impl::Voice *voice = mImpl->findVoice( id );
    return voice && ma_sound_is_playing( &voice->sound );
}

bool Audio::playMusic( std::vector<uint8_t> encoded, float fadeInSeconds, std::string &error )
{
    std::lock_guard<std::mutex> lock( mImpl->mutex );
    Impl &a = *mImpl;
    if( !a.initialised )
    {
        error = "audio is not running";
        return false;
    }

    const int nextIndex = 1 - a.currentMusic;
    Impl::MusicTrack &next = a.music[nextIndex];
    a.releaseTrack( next );  // still fading out from an earlier switch
    next.encoded = std::move( encoded );

    ma_decoder_config config = ma_decoder_config_init( ma_format_f32, kChannels, kSampleRate );
    if( ma_decoder_init_memory( next.encoded.data(), next.encoded.size(), &config, &next.decoder ) != MA_SUCCESS )
    {
        next.encoded.clear();
        error = "it is not a WAV, MP3, FLAC or Ogg Vorbis file";
        return false;
    }
    if( ma_sound_init_from_data_source( &a.engine, &next.decoder, MA_SOUND_FLAG_NO_SPATIALIZATION,
                                        &a.bus( AudioBus::Music ), &next.sound ) != MA_SUCCESS )
    {
        ma_decoder_uninit( &next.decoder );
        next.encoded.clear();
        error = "it could not be played";
        return false;
    }
    next.initialised = true;

    const ma_uint64 fadeMilliseconds = toMilliseconds( fadeInSeconds );
    ma_sound_set_looping( &next.sound, MA_TRUE );
    if( fadeMilliseconds > 0 )
        ma_sound_set_fade_in_milliseconds( &next.sound, 0.0f, 1.0f, fadeMilliseconds );
    ma_sound_start( &next.sound );

    Impl::MusicTrack &previous = a.music[a.currentMusic];
    if( previous.initialised )
    {
        if( fadeMilliseconds > 0 )
            ma_sound_stop_with_fade_in_milliseconds( &previous.sound, fadeMilliseconds );
        else
            a.releaseTrack( previous );
    }
    a.currentMusic = nextIndex;
    return true;
}

void Audio::stopMusic( float fadeOutSeconds )
{
    std::lock_guard<std::mutex> lock( mImpl->mutex );
    Impl::MusicTrack &track = mImpl->music[mImpl->currentMusic];
    if( !track.initialised )
        return;

    const ma_uint64 fadeMilliseconds = toMilliseconds( fadeOutSeconds );
    if( fadeMilliseconds > 0 )
        ma_sound_stop_with_fade_in_milliseconds( &track.sound, fadeMilliseconds );
    else
        mImpl->releaseTrack( track );
}

void Audio::setBusVolume( AudioBus bus, float volume )
{
    std::lock_guard<std::mutex> lock( mImpl->mutex );
    if( mImpl->initialised )
        ma_sound_group_set_volume( &mImpl->bus( bus ), volume );
}

void Audio::setBusPaused( AudioBus bus, bool paused )
{
    std::lock_guard<std::mutex> lock( mImpl->mutex );
    if( !mImpl->initialised )
        return;
    if( paused )
        ma_sound_group_stop( &mImpl->bus( bus ) );
    else
        ma_sound_group_start( &mImpl->bus( bus ) );
}

void Audio::setMasterVolume( float volume )
{
    std::lock_guard<std::mutex> lock( mImpl->mutex );
    if( mImpl->initialised )
        ma_engine_set_volume( &mImpl->engine, volume );
}

void Audio::setListener( Vec3 position, Vec3 forward, Vec3 up, Vec3 velocity )
{
    std::lock_guard<std::mutex> lock( mImpl->mutex );
    if( !mImpl->initialised )
        return;
    ma_engine_listener_set_position( &mImpl->engine, 0, position.x, position.y, position.z );
    ma_engine_listener_set_direction( &mImpl->engine, 0, forward.x, forward.y, forward.z );
    ma_engine_listener_set_world_up( &mImpl->engine, 0, up.x, up.y, up.z );
    ma_engine_listener_set_velocity( &mImpl->engine, 0, velocity.x, velocity.y, velocity.z );
}

}  // namespace Rhiza
