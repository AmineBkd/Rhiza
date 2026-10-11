#include "LogOutputs.h"

#include "core/LogSinks.h"

#include <SDL3/SDL.h>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

namespace Rhiza
{

namespace
{

SDL_LogPriority toSdlPriority( LogLevel level )
{
    switch( level )
    {
    case LogLevel::Debug:
        return SDL_LOG_PRIORITY_DEBUG;
    case LogLevel::Info:
        return SDL_LOG_PRIORITY_INFO;
    case LogLevel::Warning:
        return SDL_LOG_PRIORITY_WARN;
    case LogLevel::Error:
        return SDL_LOG_PRIORITY_ERROR;
    }
    return SDL_LOG_PRIORITY_INFO;
}

void writeToConsole( LogLevel level, std::string_view category, std::string_view message )
{
    SDL_LogMessage( SDL_LOG_CATEGORY_APPLICATION, toSdlPriority( level ), "[%.*s] %.*s",
                    static_cast<int>( category.size() ), category.data(), static_cast<int>( message.size() ),
                    message.data() );
}

// Flushed to the OS, not synced to the device: the OS still writes the line
// out if the game crashes, and a sync per line cost ~1.5 ms (SDL_FlushIO).
void writeToFile( std::ofstream &file, LogLevel level, std::string_view category, std::string_view message )
{
    SDL_Time now = 0;
    SDL_DateTime time{};
    SDL_GetCurrentTime( &now );
    SDL_TimeToDateTime( now, &time, true );

    char prefix[32];
    std::snprintf( prefix, sizeof( prefix ), "%02d:%02d:%02d.%03d %-7s [", time.hour, time.minute,
                   time.second, time.nanosecond / 1000000, logLevelName( level ) );
    file << prefix << category << "] " << message << '\n';
    file.flush();
}

}  // namespace

LogOutputs::LogOutputs() = default;

LogOutputs::~LogOutputs()
{
    detach();
}

void LogOutputs::attach( const char *organization, const char *application )
{
#ifndef NDEBUG
    SDL_SetLogPriority( SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_DEBUG );
#endif
    mConsoleSink = addLogSink( &writeToConsole );

    char *folder = SDL_GetPrefPath( organization, application );
    if( !folder )
    {
        logWarning( "platform", std::string( "no writable folder for the log file: " ) + SDL_GetError() );
        return;
    }
    const std::string path = std::string( folder ) + "Rhiza.log";
    const std::string previous = std::string( folder ) + "Rhiza.previous.log";
    SDL_free( folder );

    // Fails harmlessly on the first run, when there is nothing to keep.
    SDL_RenamePath( path.c_str(), previous.c_str() );

    // SDL's paths are UTF-8; going through u8string keeps a non-ASCII user
    // folder working on Windows, where a narrow path would be read as ANSI.
    mFile =
        std::make_unique<std::ofstream>( std::filesystem::path( std::u8string( path.begin(), path.end() ) ) );
    if( !mFile->is_open() )
    {
        mFile.reset();
        logWarning( "platform", "could not open " + path );
        return;
    }

    std::ofstream *file = mFile.get();
    mFileSink = addLogSink( [file]( LogLevel level, std::string_view category, std::string_view message ) {
        writeToFile( *file, level, category, message );
    } );
    logInfo( "platform", "logging to " + path );
}

void LogOutputs::detach()
{
    flushLog();
    if( mFileSink )
    {
        removeLogSink( mFileSink );
        mFileSink = 0;
    }
    if( mConsoleSink )
    {
        removeLogSink( mConsoleSink );
        mConsoleSink = 0;
    }
    mFile.reset();
}

}  // namespace Rhiza
