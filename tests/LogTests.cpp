// Tests for the core log: delivery to sinks, collapsing identical repeats,
// and Debug messages disappearing from release builds.

#include "core/LogSinks.h"

#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/catch_test_macros.hpp>

#include <chrono>
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
    CHECK( capture.lines[1].message == "(previous message logged 3 times in total)" );
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
    CHECK( capture.lines[1].message == "(previous message logged 2 times in total)" );
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

TEST_CASE( "Log: a repeat that keeps going is reported every 10 times" )
{
    Capture capture;
    for( int i = 0; i < 25; ++i )
        Rhiza::logError( "render", "bad near clip" );
    Rhiza::flushLog();

    REQUIRE( capture.lines.size() == 4 );
    CHECK( capture.lines[0].message == "bad near clip" );
    CHECK( capture.lines[1].message == "(previous message repeated 10 more times so far)" );
    CHECK( capture.lines[2].message == "(previous message repeated 20 more times so far)" );
    CHECK( capture.lines[3].message == "(previous message logged 25 times in total)" );
}

TEST_CASE( "Log: a repeat always ends with its total, even right after a report" )
{
    Capture capture;
    for( int i = 0; i < 11; ++i )
        Rhiza::logError( "render", "bad near clip" );
    Rhiza::flushLog();

    REQUIRE( capture.lines.size() == 3 );
    CHECK( capture.lines[1].message == "(previous message repeated 10 more times so far)" );
    CHECK( capture.lines[2].message == "(previous message logged 11 times in total)" );
}

TEST_CASE( "Log: a second without a copy ends the repeat" )
{
    const auto aSecondLater = [] { return std::chrono::steady_clock::now() + std::chrono::seconds( 2 ); };
    Capture capture;
    for( int i = 0; i < 3; ++i )
        Rhiza::logError( "render", "bad near clip" );

    Rhiza::tickLog();
    CHECK( capture.lines.size() == 1 );

    Rhiza::tickLog( aSecondLater() );
    REQUIRE( capture.lines.size() == 2 );
    CHECK( capture.lines[1].message == "(previous message logged 3 times in total)" );

    // Once over, the same message is a new occurrence, written in full.
    Rhiza::logError( "render", "bad near clip" );
    REQUIRE( capture.lines.size() == 3 );
    CHECK( capture.lines[2].message == "bad near clip" );
}

TEST_CASE( "Log: a message seen once, then quiet, is written again when it returns" )
{
    Capture capture;
    Rhiza::logInfo( "assets", "loaded" );
    Rhiza::tickLog( std::chrono::steady_clock::now() + std::chrono::seconds( 2 ) );
    Rhiza::logInfo( "assets", "loaded" );

    REQUIRE( capture.lines.size() == 2 );
    CHECK( capture.lines[1].message == "loaded" );
}

TEST_CASE( "Log: RHIZA_LOG_DEBUG vanishes from release builds, arguments included" )
{
    Capture capture;
    int built = 0;
    [[maybe_unused]] auto message = [&built] {
        ++built;
        return std::string( "detail" );
    };
    RHIZA_LOG_DEBUG( "test", message() );

#ifdef NDEBUG
    CHECK( built == 0 );
    CHECK( capture.lines.empty() );
#else
    CHECK( built == 1 );
    REQUIRE( capture.lines.size() == 1 );
    CHECK( capture.lines[0].level == Rhiza::LogLevel::Debug );
#endif
}

// Hidden from ctest; run with: RhizaCoreTests [benchmark]
TEST_CASE( "Log: repeat-tracking benchmark", "[.benchmark]" )
{
    Rhiza::flushLog();
    const int sink = Rhiza::addLogSink( []( Rhiza::LogLevel, std::string_view, std::string_view ) {} );

    BENCHMARK( "tickLog, once per frame, nothing repeating" )
    {
        Rhiza::tickLog();
    };

    Rhiza::logWarning( "bench", "spam" );
    BENCHMARK( "a repeated copy, counted instead of written" )
    {
        Rhiza::logWarning( "bench", "spam" );
    };

    BENCHMARK( "steady_clock::now() alone" )
    {
        return std::chrono::steady_clock::now();
    };

    Rhiza::flushLog();
    Rhiza::removeLogSink( sink );
}

}  // namespace
