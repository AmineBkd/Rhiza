#include "LogSinks.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace Rhiza
{

namespace
{

constexpr uint32_t kRepeatReportInterval = 10;
constexpr std::chrono::seconds kRepeatQuietTime( 1 );

struct LogState
{
    std::mutex mutex;
    std::vector<std::pair<int, LogSink>> sinks;
    int nextSinkId = 1;

    bool hasLast = false;
    LogLevel lastLevel = LogLevel::Info;
    std::string lastCategory;
    std::string lastMessage;
    std::chrono::steady_clock::time_point lastCopyTime;
    uint32_t repeats = 0;
};

LogState &state()
{
    static LogState s;
    return s;
}

void emit( const LogState &s, LogLevel level, std::string_view category, std::string_view message )
{
    if( s.sinks.empty() )
    {
        std::fprintf( stderr, "%s [%.*s] %.*s\n", logLevelName( level ), static_cast<int>( category.size() ),
                      category.data(), static_cast<int>( message.size() ), message.data() );
        return;
    }
    for( const auto &[id, sink] : s.sinks )
        sink( level, category, message );
}

// A repeat's last copy looks like every other, so it is known to have
// stopped only when a different message arrives, a second passes without a
// copy (tickLog), or the log is flushed.
void emitRepeats( LogState &s )
{
    if( s.repeats == 0 )
        return;
    emit( s, s.lastLevel, s.lastCategory,
          "(previous message logged " + std::to_string( s.repeats + 1 ) + " times in total)" );
    s.repeats = 0;
}

}  // namespace

void logMessage( LogLevel level, std::string_view category, std::string_view message )
{
    LogState &s = state();
    std::lock_guard<std::mutex> lock( s.mutex );
    if( s.hasLast && level == s.lastLevel && category == s.lastCategory && message == s.lastMessage )
    {
        s.lastCopyTime = std::chrono::steady_clock::now();
        // Reported as it goes, so a message that never stops still shows.
        if( ++s.repeats % kRepeatReportInterval == 0 )
            emit( s, level, category,
                  "(previous message repeated " + std::to_string( s.repeats ) + " more times so far)" );
        return;
    }

    emitRepeats( s );
    s.hasLast = true;
    s.lastLevel = level;
    s.lastCategory.assign( category );
    s.lastMessage.assign( message );
    s.lastCopyTime = std::chrono::steady_clock::now();
    emit( s, level, category, message );
}

int addLogSink( LogSink sink )
{
    LogState &s = state();
    std::lock_guard<std::mutex> lock( s.mutex );
    const int id = s.nextSinkId++;
    s.sinks.emplace_back( id, std::move( sink ) );
    return id;
}

void removeLogSink( int id )
{
    LogState &s = state();
    std::lock_guard<std::mutex> lock( s.mutex );
    std::erase_if( s.sinks, [id]( const auto &entry ) { return entry.first == id; } );
}

void flushLog()
{
    LogState &s = state();
    std::lock_guard<std::mutex> lock( s.mutex );
    emitRepeats( s );
    s.hasLast = false;
}

void tickLog( std::chrono::steady_clock::time_point now )
{
    LogState &s = state();
    std::lock_guard<std::mutex> lock( s.mutex );
    if( !s.hasLast || now - s.lastCopyTime < kRepeatQuietTime )
        return;
    emitRepeats( s );
    s.hasLast = false;
}

const char *logLevelName( LogLevel level )
{
    switch( level )
    {
    case LogLevel::Debug:
        return "DEBUG";
    case LogLevel::Info:
        return "INFO";
    case LogLevel::Warning:
        return "WARNING";
    case LogLevel::Error:
        return "ERROR";
    }
    return "?";
}

}  // namespace Rhiza
