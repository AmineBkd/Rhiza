// Tests for the core log: delivery to sinks, collapsing identical repeats,
// and Debug messages disappearing from release builds.

#include "core/LogSinks.h"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

namespace
{

struct Line
{
    Rhiza::LogLevel level;
    std::string category;
    std::string message;
};

// A sink for the length of one test, starting from a clean repeat state.
class Capture
{
public:
    Capture()
    {
        Rhiza::flushLog();
        mId = Rhiza::addLogSink(
            [this]( Rhiza::LogLevel level, std::string_view category, std::string_view message ) {
                lines.push_back( { level, std::string( category ), std::string( message ) } );
            } );
    }
    ~Capture() { Rhiza::removeLogSink( mId ); }

    Capture( const Capture & ) = delete;
    Capture &operator=( const Capture & ) = delete;

    std::vector<Line> lines;

private:
    int mId = 0;
};

TEST_CASE( "Log: a message reaches the sink with its level and category" )
{
    Capture capture;
    Rhiza::logWarning( "audio", "no output device" );

    REQUIRE( capture.lines.size() == 1 );
    CHECK( capture.lines[0].level == Rhiza::LogLevel::Warning );
    CHECK( capture.lines[0].category == "audio" );
    CHECK( capture.lines[0].message == "no output device" );
}

TEST_CASE( "Log: identical repeats collapse into a count" )
{
    Capture capture;
    for( int i = 0; i < 3; ++i )
        Rhiza::logError( "render", "bad near clip" );
    Rhiza::logError( "render", "something else" );

    REQUIRE( capture.lines.size() == 3 );
    CHECK( capture.lines[0].message == "bad near clip" );
    CHECK( capture.lines[1].message == "(previous message repeated 2 more times)" );
    CHECK( capture.lines[1].level == Rhiza::LogLevel::Error );
    CHECK( capture.lines[1].category == "render" );
    CHECK( capture.lines[2].message == "something else" );
}

TEST_CASE( "Log: the same text at another level or category is not a repeat" )
{
    Capture capture;
    Rhiza::logInfo( "a", "same" );
    Rhiza::logWarning( "a", "same" );
    Rhiza::logWarning( "b", "same" );

    CHECK( capture.lines.size() == 3 );
}

TEST_CASE( "Log: flushLog writes a pending count and forgets the last message" )
{
    Capture capture;
    Rhiza::logInfo( "assets", "loaded" );
    Rhiza::logInfo( "assets", "loaded" );
    Rhiza::flushLog();
    Rhiza::logInfo( "assets", "loaded" );

    REQUIRE( capture.lines.size() == 3 );
    CHECK( capture.lines[1].message == "(previous message repeated 1 more times)" );
    CHECK( capture.lines[2].message == "loaded" );
}

TEST_CASE( "Log: a removed sink receives nothing" )
{
    Capture capture;
    std::vector<std::string> removed;
    const int id =
        Rhiza::addLogSink( [&removed]( Rhiza::LogLevel, std::string_view, std::string_view message ) {
            removed.emplace_back( message );
        } );
    Rhiza::removeLogSink( id );
    Rhiza::logInfo( "test", "after removal" );

    CHECK( removed.empty() );
    CHECK( capture.lines.size() == 1 );
}

TEST_CASE( "Log: debug messages exist only in debug builds" )
{
    Capture capture;
    Rhiza::logDebug( "test", "detail" );

#ifdef NDEBUG
    CHECK( capture.lines.empty() );
#else
    REQUIRE( capture.lines.size() == 1 );
    CHECK( capture.lines[0].level == Rhiza::LogLevel::Debug );
#endif
}

}  // namespace
