#pragma once

#include <string_view>

namespace Rhiza
{

enum class LogLevel
{
    Debug,
    Info,
    Warning,
    Error,
};

// One log for the engine and the game, written to the console and to a file
// in the per-user folder named by EngineSettings::organization/application.
// The category says who is speaking: "render", "audio", "ship-ai".
// Identical repeats collapse into a count, written every 10 repeats while
// they continue, then as a total once a second passes without one. Safe
// from any thread except the audio callback, which must never log.
void logMessage( LogLevel level, std::string_view category, std::string_view message );

inline void logInfo( std::string_view category, std::string_view message )
{
    logMessage( LogLevel::Info, category, message );
}

inline void logWarning( std::string_view category, std::string_view message )
{
    logMessage( LogLevel::Warning, category, message );
}

inline void logError( std::string_view category, std::string_view message )
{
    logMessage( LogLevel::Error, category, message );
}

}  // namespace Rhiza

// Removed whole from builds that define NDEBUG, arguments included, so
// building a Debug message costs nothing in Release.
#ifdef NDEBUG
#    define RHIZA_LOG_DEBUG( category, message ) static_cast<void>( 0 )
#else
#    define RHIZA_LOG_DEBUG( category, message ) \
        ::Rhiza::logMessage( ::Rhiza::LogLevel::Debug, category, message )
#endif
