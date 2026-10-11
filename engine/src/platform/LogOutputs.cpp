#include "LogOutputs.h"

#include "core/LogSinks.h"

#include <SDL3/SDL.h>

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

void writeToFile( SDL_IOStream *file, LogLevel level, std::string_view category, std::string_view message )
{
    SDL_Time now = 0;
    SDL_DateTime time{};
    SDL_GetCurrentTime( &now );
    SDL_TimeToDateTime( now, &time, true );
    SDL_IOprintf( file, "%02d:%02d:%02d.%03d %-7s [%.*s] %.*s\n", time.hour, time.minute, time.second,
                  time.nanosecond / 1000000, logLevelName( level ), static_cast<int>( category.size() ),
                  category.data(), static_cast<int>( message.size() ), message.data() );
    SDL_FlushIO( file );
}

}  // namespace

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
    mFile = SDL_IOFromFile( path.c_str(), "w" );
    if( !mFile )
    {
        logWarning( "platform", "could not open " + path + ": " + SDL_GetError() );
        return;
    }

    SDL_IOStream *file = mFile;
    mFileSink = addLogSink( [file]( LogLevel level, std::string_view category, std::string_view message ) {
        writeToFile( file, level, category, message );
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
    if( mFile )
    {
        SDL_CloseIO( mFile );
        mFile = nullptr;
    }
}

}  // namespace Rhiza
