#pragma once

#include <Rhiza/Log.h>

#include <chrono>
#include <functional>
#include <string_view>

namespace Rhiza
{

// Where log messages go. The engine plugs these in at startup, since core/
// cannot reach SDL; with none attached, messages go to stderr. Sinks run
// under the log's lock, so a sink must not log.
using LogSink = std::function<void( LogLevel level, std::string_view category, std::string_view message )>;

int addLogSink( LogSink sink );
void removeLogSink( int id );

// Writes any pending "repeated" count now, and forgets the last message.
void flushLog();

// The engine calls this once per frame. A repeat that has gone a second
// without a copy is over: its total is written now, and the next copy of
// that message counts as new.
void tickLog( std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now() );

const char *logLevelName( LogLevel level );

}  // namespace Rhiza
