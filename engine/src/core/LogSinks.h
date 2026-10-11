#pragma once

#include <Rhiza/Log.h>

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

const char *logLevelName( LogLevel level );

}  // namespace Rhiza
