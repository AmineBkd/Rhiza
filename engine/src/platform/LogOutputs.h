#pragma once

struct SDL_IOStream;

namespace Rhiza
{

// Plugs two outputs into the core log while attached: the console through
// SDL_Log, which also reaches Android's logcat and Xcode, and Rhiza.log in
// the per-user writable folder. The previous run's file is kept as
// Rhiza.previous.log, since a restart after a crash would otherwise
// overwrite the log that explains it.
class LogOutputs
{
public:
    ~LogOutputs();

    void attach( const char *organization, const char *application );
    void detach();

private:
    int mConsoleSink = 0;
    int mFileSink = 0;
    SDL_IOStream *mFile = nullptr;
};

}  // namespace Rhiza
