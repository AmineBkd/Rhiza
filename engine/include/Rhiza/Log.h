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
// Identical repeats collapse into one line with a count. Safe from any thread
// except the audio callback, which must never log.
void logMessage( LogLevel level, std::string_view category, std::string_view message );

// Compiled out of builds that define NDEBUG.
inline void logDebug( [[maybe_unused]] std::string_view category, [[maybe_unused]] std::string_view message )
{
#ifndef NDEBUG
    logMessage( LogLevel::Debug, category, message );
#endif
}

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
