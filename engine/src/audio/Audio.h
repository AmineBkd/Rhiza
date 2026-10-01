#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <Rhiza/Types/Audio.h>
#include <Rhiza/Types/Math.h>

namespace Rhiza
{

// Owns every miniaudio object and knows nothing about SDL: platform's
// AudioOutput pulls the finished mix through mix(), on SDL's audio thread.
// Every other call comes from the game thread; one mutex keeps them apart.
class Audio
{
public:
    static constexpr uint32_t kSampleRate = 48000;
    static constexpr uint32_t kChannels = 2;

    struct VoiceId
    {
        uint32_t id = 0;
        uint32_t generation = 0;
    };

    Audio();
    ~Audio();

    // Voices are created here, slots x default copies per pool, so playing a
    // sound never allocates.
    bool initialize( uint32_t monoSlots, uint32_t stereoSlots, uint32_t defaultMaxCopies );
    void shutdown();

    // Fills `frameCount` interleaved stereo float frames; silence if not
    // initialised. Safe to call from any thread.
    void mix( float *out, uint32_t frameCount );

    // WAV, MP3, FLAC or Ogg Vorbis, decoded in full. 0 on failure, with the
    // reason in `error`. `maxCopies` 0 means the default.
    uint32_t createSound( const std::vector<uint8_t> &encoded, const std::string &cacheKey,
                          uint32_t maxCopies, std::string &error );
    uint32_t acquireCachedSound( const std::string &cacheKey );
    void setMaxCopies( uint32_t sound, uint32_t maxCopies );

    // Stops every voice still playing it once the last load is destroyed.
    void destroySound( uint32_t sound );

    // At the sound's copy cap, restarts its oldest copy; at the slot limit,
    // takes over the oldest slot. Invalid if the sound doesn't exist or its
    // pool has no slots.
    VoiceId play( uint32_t sound, const PlayDesc &desc );

    // Copies of `sound` playing right now.
    uint32_t playingCopies( uint32_t sound );
    void stop( VoiceId voice );
    void setVoicePosition( VoiceId voice, Vec3 position, Vec3 velocity );
    bool isPlaying( VoiceId voice ) const;

    // Keeps `encoded` and decodes it while playing; loops. Crossfades over
    // the same time from whatever was playing.
    bool playMusic( std::vector<uint8_t> encoded, float fadeInSeconds, std::string &error );
    void stopMusic( float fadeOutSeconds );

    void setBusVolume( AudioBus bus, float volume );
    void setBusPaused( AudioBus bus, bool paused );
    void setMasterVolume( float volume );

    void setListener( Vec3 position, Vec3 forward, Vec3 up, Vec3 velocity );

private:
    // Keeps miniaudio's 90,000-line header out of everything including this.
    struct Impl;
    std::unique_ptr<Impl> mImpl;
};

}  // namespace Rhiza
