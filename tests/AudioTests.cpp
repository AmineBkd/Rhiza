// Tests for the audio mixer. miniaudio mixes into a plain buffer here, with
// no sound card - the same path SDL pulls from in the game - so CI can check
// what is actually heard.

#include "audio/Audio.h"

#include <cmath>
#include <cstdio>
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

int gFailures = 0;

void check( bool condition, const std::string &what )
{
    if( !condition )
    {
        std::printf( "  FAIL: %s\n", what.c_str() );
        ++gFailures;
    }
}

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
    return { static_cast<float>( std::sqrt( left / frames ) ), static_cast<float>( std::sqrt( right / frames ) ) };
}

bool silent( const Loudness &l ) { return l.both() < 1e-4f; }

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
    check( sound != 0, "sound '" + key + "' created: " + error );
    return sound;
}

Rhiza::PlayDesc looping()
{
    Rhiza::PlayDesc desc;
    desc.loop = true;
    return desc;
}

void testSilenceAndPlayback()
{
    std::printf( "playback:\n" );
    Rhiza::Audio audio;
    check( audio.initialize( 4, 2, 4 ), "initialises without a sound card" );
    check( silent( listen( audio ) ), "silent before anything plays" );

    const uint32_t beep = makeSound( audio, 1, 0.1f, "beep.wav" );
    const Rhiza::Audio::VoiceId voice = audio.play( beep, {} );
    check( voice.id != 0, "play returns a voice" );
    check( !silent( listen( audio, 512 ) ), "playing a sound is heard" );

    advance( audio, 0.2f );
    check( !audio.isPlaying( voice ), "a sound stops on its own at its end" );
    check( audio.playingCopies( beep ) == 0, "and stops being counted" );
    check( silent( listen( audio ) ), "silent afterwards" );

    const Rhiza::Audio::VoiceId loop = audio.play( beep, looping() );
    advance( audio, 0.3f );
    check( audio.isPlaying( loop ), "a looping sound keeps going past its end" );
    audio.stop( loop );
    check( !audio.isPlaying( loop ), "stop stops it" );
    check( silent( listen( audio ) ), "and silences it" );
}

void testCopyCap()
{
    std::printf( "per-sound copy cap:\n" );
    Rhiza::Audio audio;
    audio.initialize( 8, 0, 4 );
    const uint32_t shot = makeSound( audio, 1, 0.5f, "shot.wav", 2 );

    const Rhiza::Audio::VoiceId first = audio.play( shot, looping() );
    const Rhiza::Audio::VoiceId second = audio.play( shot, looping() );
    const Rhiza::Audio::VoiceId third = audio.play( shot, looping() );
    check( audio.playingCopies( shot ) == 2, "never more copies than the cap" );
    check( !audio.isPlaying( first ), "the oldest copy was restarted for the new one" );
    check( audio.isPlaying( second ) && audio.isPlaying( third ), "the newer copies play" );

    audio.setMaxCopies( shot, 3 );
    audio.play( shot, looping() );
    check( audio.playingCopies( shot ) == 3, "a raised cap allows more" );

    const uint32_t defaulted = makeSound( audio, 1, 0.5f, "default.wav" );
    for( int i = 0; i < 6; ++i )
        audio.play( defaulted, looping() );
    check( audio.playingCopies( defaulted ) == 4, "the default cap applies when none is given" );
}

void testSlotsCountUniqueSounds()
{
    std::printf( "slots count unique sounds:\n" );
    Rhiza::Audio audio;
    audio.initialize( 2, 0, 4 );
    const uint32_t a = makeSound( audio, 1, 0.5f, "a.wav" );
    const uint32_t b = makeSound( audio, 1, 0.5f, "b.wav" );
    const uint32_t c = makeSound( audio, 1, 0.5f, "c.wav" );

    audio.play( a, looping() );
    audio.play( a, looping() );
    audio.play( a, looping() );
    audio.play( b, looping() );
    check( audio.playingCopies( a ) == 3 && audio.playingCopies( b ) == 1,
           "three copies of one sound take one slot, leaving one for another" );

    audio.play( c, looping() );
    check( audio.playingCopies( a ) == 0, "a third sound takes over the oldest slot, all its copies" );
    check( audio.playingCopies( b ) == 1 && audio.playingCopies( c ) == 1, "the other slots are untouched" );
}

void testStereoPool()
{
    std::printf( "mono and stereo pools are separate:\n" );
    Rhiza::Audio audio;
    audio.initialize( 1, 1, 4 );
    const uint32_t world = makeSound( audio, 1, 0.5f, "world.wav" );
    const uint32_t ui = makeSound( audio, 2, 0.5f, "ui.wav" );
    audio.play( world, looping() );
    audio.play( ui, looping() );
    check( audio.playingCopies( world ) == 1 && audio.playingCopies( ui ) == 1,
           "a full mono pool leaves the stereo one free" );

    Rhiza::Audio monoOnly;
    monoOnly.initialize( 4, 0, 4 );
    const uint32_t stereo = makeSound( monoOnly, 2, 0.5f, "stereo.wav" );
    check( monoOnly.play( stereo, {} ).id == 0, "no stereo slots: a stereo sound does not play" );
}

void testBuses()
{
    std::printf( "buses:\n" );
    Rhiza::Audio audio;
    audio.initialize( 4, 2, 4 );
    const uint32_t hum = makeSound( audio, 1, 0.5f, "hum.wav" );
    const Rhiza::Audio::VoiceId voice = audio.play( hum, looping() );

    audio.setBusVolume( Rhiza::AudioBus::Sfx, 0.0f );
    check( silent( listen( audio ) ), "bus volume 0 silences it" );
    audio.setBusVolume( Rhiza::AudioBus::Sfx, 1.0f );
    check( !silent( listen( audio ) ), "volume back up, heard again" );

    audio.setBusPaused( Rhiza::AudioBus::Sfx, true );
    check( silent( listen( audio ) ), "a paused bus is silent" );
    check( audio.isPlaying( voice ), "but its sounds are paused, not stopped" );
    audio.setBusPaused( Rhiza::AudioBus::Sfx, false );
    check( !silent( listen( audio ) ), "and resume" );
}

void testPositional()
{
    std::printf( "positional audio:\n" );
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
    check( right.right > right.left * 1.5f, "a sound on the right is louder on the right" );
    const Loudness left = heardFrom( { -5, 0, 0 } );
    check( left.left > left.right * 1.5f, "a sound on the left is louder on the left" );

    const Loudness near = heardFrom( { 0, 0, -5 } );
    const Loudness far = heardFrom( { 0, 0, -80 } );
    check( far.both() < near.both() * 0.5f, "a distant sound is quieter" );
}

void testCacheAndDestroy()
{
    std::printf( "cache and destroy:\n" );
    Rhiza::Audio audio;
    audio.initialize( 4, 0, 4 );
    const uint32_t boom = makeSound( audio, 1, 0.5f, "boom.wav" );
    check( audio.acquireCachedSound( "boom.wav" ) == boom, "a second load gets the same sound" );
    check( audio.acquireCachedSound( "other.wav" ) == 0, "a different path is not a hit" );

    audio.destroySound( boom );
    check( audio.play( boom, {} ).id != 0, "still alive while one load remains" );

    audio.play( boom, looping() );
    audio.destroySound( boom );
    check( silent( listen( audio ) ), "destroying the last load silences its voices" );
    check( audio.play( boom, {} ).id == 0, "and it is gone" );
    check( audio.acquireCachedSound( "boom.wav" ) == 0, "from the cache too" );
}

std::vector<uint8_t> readTestOgg()
{
    std::ifstream file( RHIZA_TEST_OGG, std::ios::binary );
    return std::vector<uint8_t>( std::istreambuf_iterator<char>( file ), {} );
}

void testMusic()
{
    std::printf( "music:\n" );
    Rhiza::Audio audio;
    audio.initialize( 4, 0, 4 );
    std::string error;

    check( audio.playMusic( sineWav( 2, 0.1f ), 0.0f, error ), "plays: " + error );
    advance( audio, 0.35f );
    check( !silent( listen( audio ) ), "loops past the end of the track" );
    audio.stopMusic( 0.0f );
    check( silent( listen( audio ) ), "stops" );

    const std::vector<uint8_t> ogg = readTestOgg();
    check( !ogg.empty(), "test Ogg file found" );
    check( audio.playMusic( ogg, 0.0f, error ), "Ogg Vorbis decodes: " + error );
    check( !silent( listen( audio ) ), "and is heard" );

    audio.setBusVolume( Rhiza::AudioBus::Music, 0.0f );
    check( silent( listen( audio ) ), "the music bus controls it" );
}

void testRejections()
{
    std::printf( "bad data is rejected:\n" );
    Rhiza::Audio audio;
    audio.initialize( 4, 0, 4 );
    std::string error;
    const std::vector<uint8_t> garbage( 64, 0x5A );
    check( audio.createSound( garbage, "bad.wav", 0, error ) == 0, "garbage sound rejected" );
    check( !error.empty(), "with a reason" );
    check( !audio.playMusic( garbage, 0.0f, error ), "garbage music rejected" );
    check( audio.play( 12345, {} ).id == 0, "an unknown sound does not play" );
    audio.stop( { 999, 1 } );
    check( !audio.isPlaying( { 999, 1 } ), "an unknown voice is harmless" );
}

}  // namespace

int main()
{
    testSilenceAndPlayback();
    testCopyCap();
    testSlotsCountUniqueSounds();
    testStereoPool();
    testBuses();
    testPositional();
    testCacheAndDestroy();
    testMusic();
    testRejections();

    if( gFailures == 0 )
    {
        std::printf( "\nAll audio tests passed.\n" );
        return 0;
    }

    std::printf( "\n%d check(s) failed.\n", gFailures );
    return 1;
}
