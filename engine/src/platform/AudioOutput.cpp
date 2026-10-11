#include "AudioOutput.h"

#include <Rhiza/Log.h>

#include <SDL3/SDL.h>

#include <string>

namespace Rhiza
{

namespace
{

void SDLCALL onAudioWanted( void *userdata, SDL_AudioStream *stream, int additionalBytes, int )
{
    static_cast<AudioOutput *>( userdata )->feed( stream, additionalBytes );
}

}  // namespace

AudioOutput::~AudioOutput()
{
    stop();
}

bool AudioOutput::start( int sampleRate, int channels, FillFunction fill )
{
    if( !SDL_InitSubSystem( SDL_INIT_AUDIO ) )
    {
        logWarning( "audio", std::string( "audio unavailable: " ) + SDL_GetError() );
        return false;
    }
    mAudioSubsystemStarted = true;
    mFill = std::move( fill );
    mChannels = channels;

    const SDL_AudioSpec spec{ SDL_AUDIO_F32, channels, sampleRate };
    mStream = SDL_OpenAudioDeviceStream( SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, &onAudioWanted, this );
    if( !mStream )
    {
        logWarning( "audio", std::string( "no audio output device: " ) + SDL_GetError() );
        stop();
        return false;
    }
    SDL_ResumeAudioStreamDevice( mStream );
    return true;
}

void AudioOutput::stop()
{
    // Destroying the stream closes its device, so no callback runs after.
    if( mStream )
    {
        SDL_DestroyAudioStream( mStream );
        mStream = nullptr;
    }
    if( mAudioSubsystemStarted )
    {
        SDL_QuitSubSystem( SDL_INIT_AUDIO );
        mAudioSubsystemStarted = false;
    }
}

void AudioOutput::feed( SDL_AudioStream *stream, int bytesWanted )
{
    const int frameBytes = static_cast<int>( sizeof( float ) ) * mChannels;
    const int frameCount = ( bytesWanted + frameBytes - 1 ) / frameBytes;
    if( frameCount <= 0 )
        return;

    mBuffer.resize( static_cast<size_t>( frameCount ) * mChannels );
    mFill( mBuffer.data(), frameCount );
    SDL_PutAudioStreamData( stream, mBuffer.data(), frameCount * frameBytes );
}

}  // namespace Rhiza
