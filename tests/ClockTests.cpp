// Tests for Rhiza::Clock.
//
// Unlike Shapes and Input, Clock's correctness depends on actual elapsed
// wall time, not just state transitions - so unlike those, this test
// deliberately sleeps. That costs real time in the suite, but the
// alternative is trusting the max-delta clamp works without ever having
// measured it, which defeats the point of having it.

#include "core/Clock.h"

#include <chrono>
#include <cstdio>
#include <string>
#include <thread>

namespace
{

int gFailures = 0;

void check( bool condition, const std::string &what )
{
    if( !condition )
    {
        std::printf( "  FAIL: %s\n", what.c_str() );
        ++gFailures;
    }
}

void testFirstTickIsSmall()
{
    std::printf( "first tick:\n" );
    Rhiza::Clock clock;
    clock.tick();

    // No sleep between construction and this tick(), so elapsed time should
    // be small - not exactly zero (constructing the object and calling
    // tick() both take some real time), but nowhere near a full second.
    check( clock.realDeltaSeconds() >= 0.0f, "realDeltaSeconds is never negative" );
    check( clock.realDeltaSeconds() < 1.0f, "first tick with no sleep is well under a second" );
}

void testTimeScaleDefaultsToNormalSpeed()
{
    std::printf( "default time scale:\n" );
    Rhiza::Clock clock;
    clock.tick();

    check( clock.timeScale() == 1.0f, "timeScale defaults to 1" );
    check( clock.scaledDeltaSeconds() == clock.realDeltaSeconds(),
           "scaled equals real at the default scale" );
}

void testTimeScaleFreezesAndReverses()
{
    std::printf( "time scale freeze and reverse:\n" );
    Rhiza::Clock clock;
    clock.tick();

    // Frozen: hitstop's core case. Real time must keep moving even though
    // scaled time reports nothing happened.
    clock.setTimeScale( 0.0f );
    check( clock.scaledDeltaSeconds() == 0.0f, "scale 0 freezes scaled time" );
    check( clock.realDeltaSeconds() > 0.0f, "real time is unaffected by scale 0" );

    // Doubled: gameplay running at 2x.
    clock.setTimeScale( 2.0f );
    check( clock.scaledDeltaSeconds() == clock.realDeltaSeconds() * 2.0f,
           "scale 2 doubles scaled time" );

    // Negative scale is intentional - time running backward - so it must
    // not be clamped away as an accidental sign error.
    clock.setTimeScale( -1.0f );
    check( clock.scaledDeltaSeconds() < 0.0f, "negative scale reverses scaled time" );
}

void testLongStallIsClamped()
{
    std::printf( "long stall is clamped:\n" );
    Rhiza::Clock clock;
    clock.tick();

    // Simulates the failure case the clamp exists for: a frame stalled
    // behind a breakpoint, an OS suspend, or a slow first-frame shader
    // warm-up. Without the clamp this would report ~0.4s and hand
    // simulation code a delta large enough to step clean through a wall.
    std::this_thread::sleep_for( std::chrono::milliseconds( 400 ) );
    clock.tick();

    check( clock.realDeltaSeconds() > 0.05f, "tick still measured real elapsed time" );
    check( clock.realDeltaSeconds() < 0.3f,
           "a 400ms stall is capped well below the true elapsed time" );
}

}  // namespace

int main()
{
    testFirstTickIsSmall();
    testTimeScaleDefaultsToNormalSpeed();
    testTimeScaleFreezesAndReverses();
    testLongStallIsClamped();

    if( gFailures == 0 )
    {
        std::printf( "\nAll clock tests passed.\n" );
        return 0;
    }

    std::printf( "\n%d check(s) failed.\n", gFailures );
    return 1;
}
