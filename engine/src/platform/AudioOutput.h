#pragma once

#include <functional>
#include <vector>

struct SDL_AudioStream;

namespace Rhiza
{

// Opens the default playback device and keeps it fed by pulling interleaved
// float frames from `fill`, on SDL's audio thread. Knows nothing about how
// the sound is made - audio/ never includes this, nor this audio/.
class AudioOutput
{
public:
    using FillFunction = std::function<void( float *frames, int frameCount )>;

    ~AudioOutput();

    // False if there is no usable device; the game then runs silent.
    bool start( int sampleRate, int channels, FillFunction fill );
    void stop();

    // SDL's callback only.
    void feed( SDL_AudioStream *stream, int bytesWanted );

private:
    SDL_AudioStream *mStream = nullptr;
    FillFunction mFill;
    std::vector<float> mBuffer;  // grows to the largest request, then stays
    int mChannels = 2;
    bool mAudioSubsystemStarted = false;
};

}  // namespace Rhiza
