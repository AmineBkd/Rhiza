// Tests for the audio mixer. miniaudio mixes into a plain buffer here, with
// no sound card - the same path SDL pulls from in the game - so CI can check
// what is actually heard.

#include "audio/Audio.h"

#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#ifndef RHIZA_TEST_OGG
#    error "RHIZA_TEST_OGG must be defined by CMake"
#endif

namespace
{

constexpr uint32_t kRate = Rhiza::Audio::kSampleRate;

// A 16-bit PCM WAV of a sine wave, built in memory.
std::vector<uint8_t> sineWav( uint32_t channels, float seconds, float hz = 440.0f )
{
    const uint32_t frames = static_cast<uint32_t>( seconds * kRate );
    const uint32_t dataBytes = frames * channels * 2;
    std::vector<uint8_t> out;
    auto put32 = [&out]( uint32_t v ) {
        for( int i = 0; i < 4; ++i )
            out.push_back( static_cast<uint8_t>( v >> ( 8 * i ) ) );
    };
    auto put16 = [&out]( uint16_t v ) {
        out.push_back( static_cast<uint8_t>( v ) );
        out.push_back( static_cast<uint8_t>( v >> 8 ) );
    };
    out.insert( out.end(), { 'R', 'I', 'F', 'F' } );
    put32( 36 + dataBytes );
    out.insert( out.end(), { 'W', 'A', 'V', 'E', 'f', 'm', 't', ' ' } );
    put32( 16 );
    put16( 1 );  // PCM
    put16( static_cast<uint16_t>( channels ) );
    put32( kRate );
    put32( kRate * channels * 2 );
    put16( static_cast<uint16_t>( channels * 2 ) );
    put16( 16 );
    out.insert( out.end(), { 'd', 'a', 't', 'a' } );
    put32( dataBytes );
    for( uint32_t f = 0; f < frames; ++f )
    {
        const auto sample = static_cast<int16_t>( 12000.0f * std::sin( 6.2831853f * hz * f / kRate ) );
        for( uint32_t c = 0; c < channels; ++c )
            put16( static_cast<uint16_t>( sample ) );
    }
    return out;
}

struct Loudness
{
    float left = 0.0f;
    float right = 0.0f;
    float both() const { return left + right; }
};

// Mixes a few buffers first so gain and position changes finish ramping.
Loudness listen( Rhiza::Audio &audio, uint32_t frames = 2048 )
{
    std::vector<float> buffer( frames * 2 );
    for( int warmUp = 0; warmUp < 4; ++warmUp )
        audio.mix( buffer.data(), frames );
    audio.mix( buffer.data(), frames );

    double left = 0.0, right = 0.0;
    for( uint32_t f = 0; f < frames; ++f )
    {
        left += buffer[f * 2] * buffer[f * 2];
        right += buffer[f * 2 + 1] * buffer[f * 2 + 1];
    }
    return { static_cast<float>( std::sqrt( left / frames ) ),
             static_cast<float>( std::sqrt( right / frames ) ) };
}

bool silent( const Loudness &l )
{
    return l.both() < 1e-4f;
}

void advance( Rhiza::Audio &audio, float seconds )
{
    std::vector<float> buffer( 1024 * 2 );
    for( uint32_t done = 0; done < seconds * kRate; done += 1024 )
        audio.mix( buffer.data(), 1024 );
}

uint32_t makeSound( Rhiza::Audio &audio, uint32_t channels, float seconds, const std::string &key,
                    uint32_t maxCopies = 0 )
{
    std::string error;
    const uint32_t sound = audio.createSound( sineWav( channels, seconds ), key, maxCopies, error );
    INFO( "sound '" << key << "': " << error );
    CHECK( sound != 0 );
    return sound;
}

Rhiza::PlayDesc looping()
{
    Rhiza::PlayDesc desc;
    desc.loop = true;
    return desc;
}

TEST_CASE( "Audio: playback" )
{
    Rhiza::Audio audio;
    CHECK( audio.initialize( 4, 2, 4 ) );
    CHECK( silent( listen( audio ) ) );

    const uint32_t beep = makeSound( audio, 1, 0.1f, "beep.wav" );
    const Rhiza::Audio::VoiceId voice = audio.play( beep, {} );
    CHECK( voice.id != 0 );
    CHECK( !silent( listen( audio, 512 ) ) );

    advance( audio, 0.2f );
    CHECK( !audio.isPlaying( voice ) );
    CHECK( audio.playingCopies( beep ) == 0 );
    CHECK( silent( listen( audio ) ) );

    const Rhiza::Audio::VoiceId loop = audio.play( beep, looping() );
    advance( audio, 0.3f );
    CHECK( audio.isPlaying( loop ) );
    audio.stop( loop );
    CHECK( !audio.isPlaying( loop ) );
    CHECK( silent( listen( audio ) ) );
}

TEST_CASE( "Audio: per-sound copy cap" )
{
    Rhiza::Audio audio;
    audio.initialize( 8, 0, 4 );
    const uint32_t shot = makeSound( audio, 1, 0.5f, "shot.wav", 2 );

    const Rhiza::Audio::VoiceId first = audio.play( shot, looping() );
    const Rhiza::Audio::VoiceId second = audio.play( shot, looping() );
    const Rhiza::Audio::VoiceId third = audio.play( shot, looping() );
    CHECK( audio.playingCopies( shot ) == 2 );
    CHECK( !audio.isPlaying( first ) );
    CHECK( audio.isPlaying( second ) );
    CHECK( audio.isPlaying( third ) );

    audio.setMaxCopies( shot, 3 );
    audio.play( shot, looping() );
    CHECK( audio.playingCopies( shot ) == 3 );

    const uint32_t defaulted = makeSound( audio, 1, 0.5f, "default.wav" );
    for( int i = 0; i < 6; ++i )
        audio.play( defaulted, looping() );
    CHECK( audio.playingCopies( defaulted ) == 4 );
}

TEST_CASE( "Audio: slots count unique sounds" )
{
    Rhiza::Audio audio;
    audio.initialize( 2, 0, 4 );
    const uint32_t a = makeSound( audio, 1, 0.5f, "a.wav" );
    const uint32_t b = makeSound( audio, 1, 0.5f, "b.wav" );
    const uint32_t c = makeSound( audio, 1, 0.5f, "c.wav" );

    audio.play( a, looping() );
    audio.play( a, looping() );
    audio.play( a, looping() );
    audio.play( b, looping() );
    CHECK( audio.playingCopies( a ) == 3 );
    CHECK( audio.playingCopies( b ) == 1 );

    audio.play( c, looping() );
    CHECK( audio.playingCopies( a ) == 0 );
    CHECK( audio.playingCopies( b ) == 1 );
    CHECK( audio.playingCopies( c ) == 1 );
}

TEST_CASE( "Audio: mono and stereo pools are separate" )
{
    Rhiza::Audio audio;
    audio.initialize( 1, 1, 4 );
    const uint32_t world = makeSound( audio, 1, 0.5f, "world.wav" );
    const uint32_t ui = makeSound( audio, 2, 0.5f, "ui.wav" );
    audio.play( world, looping() );
    audio.play( ui, looping() );
    CHECK( audio.playingCopies( world ) == 1 );
    CHECK( audio.playingCopies( ui ) == 1 );

    Rhiza::Audio monoOnly;
    monoOnly.initialize( 4, 0, 4 );
    const uint32_t stereo = makeSound( monoOnly, 2, 0.5f, "stereo.wav" );
    CHECK( monoOnly.play( stereo, {} ).id == 0 );
}

TEST_CASE( "Audio: buses" )
{
    Rhiza::Audio audio;
    audio.initialize( 4, 2, 4 );
    const uint32_t hum = makeSound( audio, 1, 0.5f, "hum.wav" );
    const Rhiza::Audio::VoiceId voice = audio.play( hum, looping() );

    audio.setBusVolume( Rhiza::AudioBus::Sfx, 0.0f );
    CHECK( silent( listen( audio ) ) );
    audio.setBusVolume( Rhiza::AudioBus::Sfx, 1.0f );
    CHECK( !silent( listen( audio ) ) );

    audio.setBusPaused( Rhiza::AudioBus::Sfx, true );
    CHECK( silent( listen( audio ) ) );
    CHECK( audio.isPlaying( voice ) );
    audio.setBusPaused( Rhiza::AudioBus::Sfx, false );
    CHECK( !silent( listen( audio ) ) );
}

TEST_CASE( "Audio: UI bus survives a paused game" )
{
    Rhiza::Audio audio;
    audio.initialize( 4, 2, 4 );
    const uint32_t world = makeSound( audio, 1, 0.5f, "world.wav" );
    const uint32_t click = makeSound( audio, 1, 0.5f, "click.wav" );

    // The click reuses the voice the world sound just left, so this also
    // checks that a voice moves to its new bus.
    const Rhiza::Audio::VoiceId worldVoice = audio.play( world, looping() );
    audio.stop( worldVoice );
    Rhiza::PlayDesc ui = looping();
    ui.bus = Rhiza::AudioBus::Ui;
    audio.play( click, ui );

    audio.setBusPaused( Rhiza::AudioBus::Sfx, true );
    CHECK( !silent( listen( audio ) ) );
    audio.setBusVolume( Rhiza::AudioBus::Ui, 0.0f );
    CHECK( silent( listen( audio ) ) );

    audio.setBusVolume( Rhiza::AudioBus::Ui, 1.0f );
    audio.setBusPaused( Rhiza::AudioBus::Sfx, false );
    audio.play( world, looping() );
    audio.setBusPaused( Rhiza::AudioBus::Ui, true );
    CHECK( !silent( listen( audio ) ) );
}

TEST_CASE( "Audio: master volume" )
{
    Rhiza::Audio audio;
    audio.initialize( 4, 2, 4 );
    const uint32_t hum = makeSound( audio, 1, 0.5f, "hum.wav" );
    audio.play( hum, looping() );
    std::string error;
    audio.playMusic( sineWav( 2, 0.5f ), 0.0f, error );
    const Loudness full = listen( audio );

    audio.setMasterVolume( 0.5f );
    const Loudness half = listen( audio );
    CHECK( std::fabs( half.both() - full.both() * 0.5f ) < full.both() * 0.05f );

    audio.setMasterVolume( 0.0f );
    CHECK( silent( listen( audio ) ) );

    audio.setMasterVolume( 1.0f );
    audio.setBusVolume( Rhiza::AudioBus::Sfx, 0.0f );
    audio.setBusVolume( Rhiza::AudioBus::Music, 0.0f );
    CHECK( silent( listen( audio ) ) );
}

TEST_CASE( "Audio: positional audio" )
{
    Rhiza::Audio audio;
    audio.initialize( 4, 0, 4 );
    audio.setListener( { 0, 0, 0 }, { 0, 0, -1 }, { 0, 1, 0 }, {} );
    const uint32_t buzz = makeSound( audio, 1, 0.5f, "buzz.wav" );

    auto heardFrom = [&]( Rhiza::Vec3 position ) {
        Rhiza::PlayDesc desc = looping();
        desc.positional = true;
        desc.position = position;
        const Rhiza::Audio::VoiceId voice = audio.play( buzz, desc );
        const Loudness heard = listen( audio );
        audio.stop( voice );
        return heard;
    };

    const Loudness right = heardFrom( { 5, 0, 0 } );
    CHECK( right.right > right.left * 1.5f );
    const Loudness left = heardFrom( { -5, 0, 0 } );
    CHECK( left.left > left.right * 1.5f );

    const Loudness near = heardFrom( { 0, 0, -5 } );
    const Loudness far = heardFrom( { 0, 0, -80 } );
    CHECK( far.both() < near.both() * 0.5f );
}

TEST_CASE( "Audio: cache and destroy" )
{
    Rhiza::Audio audio;
    audio.initialize( 4, 0, 4 );
    const uint32_t boom = makeSound( audio, 1, 0.5f, "boom.wav" );
    CHECK( audio.acquireCachedSound( "boom.wav" ) == boom );
    CHECK( audio.acquireCachedSound( "other.wav" ) == 0 );

    audio.destroySound( boom );
    CHECK( audio.play( boom, {} ).id != 0 );

    audio.play( boom, looping() );
    audio.destroySound( boom );
    CHECK( silent( listen( audio ) ) );
    CHECK( audio.play( boom, {} ).id == 0 );
    CHECK( audio.acquireCachedSound( "boom.wav" ) == 0 );
}

std::vector<uint8_t> readTestOgg()
{
    std::ifstream file( RHIZA_TEST_OGG, std::ios::binary );
    return std::vector<uint8_t>( std::istreambuf_iterator<char>( file ), {} );
}

TEST_CASE( "Audio: music" )
{
    Rhiza::Audio audio;
    audio.initialize( 4, 0, 4 );
    std::string error;

    const bool playedSine = audio.playMusic( sineWav( 2, 0.1f ), 0.0f, error );
    CAPTURE( error );
    CHECK( playedSine );
    advance( audio, 0.35f );
    CHECK( !silent( listen( audio ) ) );
    audio.stopMusic( 0.0f );
    CHECK( silent( listen( audio ) ) );

    const std::vector<uint8_t> ogg = readTestOgg();
    CHECK( !ogg.empty() );
    const bool playedOgg = audio.playMusic( ogg, 0.0f, error );
    CAPTURE( error );
    CHECK( playedOgg );
    CHECK( !silent( listen( audio ) ) );

    audio.setBusVolume( Rhiza::AudioBus::Music, 0.0f );
    CHECK( silent( listen( audio ) ) );
}

TEST_CASE( "Audio: bad data is rejected" )
{
    Rhiza::Audio audio;
    audio.initialize( 4, 0, 4 );
    std::string error;
    const std::vector<uint8_t> garbage( 64, 0x5A );
    CHECK( audio.createSound( garbage, "bad.wav", 0, error ) == 0 );
    CHECK( !error.empty() );
    CHECK( !audio.playMusic( garbage, 0.0f, error ) );
    CHECK( audio.play( 12345, {} ).id == 0 );
    audio.stop( { 999, 1 } );
    CHECK( !audio.isPlaying( { 999, 1 } ) );
}

// Hidden from ctest; run with: RhizaAudioTests [benchmark]
TEST_CASE( "Audio: mix benchmark", "[.benchmark]" )
{
    Rhiza::Audio audio;
    audio.initialize( 56, 8, 4 );
    for( int s = 0; s < 4; ++s )
    {
        const uint32_t sound = makeSound( audio, 1, 1.0f, "voice" + std::to_string( s ) );
        for( int copy = 0; copy < 4; ++copy )
        {
            Rhiza::PlayDesc desc = looping();
            desc.positional = true;
            desc.position = { static_cast<float>( s * 10 - 15 ), static_cast<float>( copy * 5 ), 0.0f };
            audio.play( sound, desc );
        }
    }

    std::vector<float> buffer( 512 * 2 );
    BENCHMARK( "mix 512 frames, 16 positional voices" )
    {
        audio.mix( buffer.data(), 512 );
        return buffer[0];
    };
}

}  // namespace
