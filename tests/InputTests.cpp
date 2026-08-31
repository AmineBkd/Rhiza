// Tests for Rhiza::Input.
//
// Input is pure state - no window, no SDL, no GPU - so like Shapes it can be
// checked in a plain console process. It's worth testing properly because
// the bug class it's prone to (a key reads as "just pressed" on the wrong
// frame, or stays stuck down) only shows up as wrong gameplay feel much
// later, at a point where the input code is the last place anyone looks.
//
// Deliberately dependency-free rather than pulling in a test framework;
// there is not yet enough here to justify one.

#include "Input.h"

#include <cstdio>
#include <string>

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

void testPressAndRelease()
{
    std::printf( "press and release:\n" );
    Rhiza::Input input;

    check( !input.isKeyDown( Rhiza::Key::W ), "key starts up" );

    // A key going down mid-frame (as Window reports it) is visible
    // immediately, and counts as freshly pressed for the rest of that frame.
    input.beginFrame();
    input.handleKeyEvent( Rhiza::Key::W, true );
    check( input.isKeyDown( Rhiza::Key::W ), "down after handleKeyEvent(true)" );
    check( input.wasKeyPressed( Rhiza::Key::W ), "wasKeyPressed true the frame it went down" );
    check( !input.wasKeyReleased( Rhiza::Key::W ), "wasKeyReleased false while still down" );

    // Holding the key into the next frame must not re-trigger "just pressed" -
    // that's the entire reason beginFrame() exists.
    input.beginFrame();
    check( input.isKeyDown( Rhiza::Key::W ), "still down next frame" );
    check( !input.wasKeyPressed( Rhiza::Key::W ), "wasKeyPressed false once held past its first frame" );

    input.handleKeyEvent( Rhiza::Key::W, false );
    check( !input.isKeyDown( Rhiza::Key::W ), "up after handleKeyEvent(false)" );
    check( input.wasKeyReleased( Rhiza::Key::W ), "wasKeyReleased true the frame it went up" );
    check( !input.wasKeyPressed( Rhiza::Key::W ), "wasKeyPressed false the frame it went up" );

    input.beginFrame();
    check( !input.wasKeyReleased( Rhiza::Key::W ), "wasKeyReleased false once up past its first frame" );
}

void testKeysAreIndependent()
{
    std::printf( "keys are independent:\n" );
    Rhiza::Input input;

    input.beginFrame();
    input.handleKeyEvent( Rhiza::Key::A, true );

    check( input.isKeyDown( Rhiza::Key::A ), "A is down" );
    check( !input.isKeyDown( Rhiza::Key::D ), "D is unaffected by A" );
    check( !input.wasKeyPressed( Rhiza::Key::D ), "D was not pressed" );
}

void testUnknownKeyIsIgnored()
{
    std::printf( "unknown key is ignored:\n" );
    Rhiza::Input input;

    // Window reports Key::Unknown for any scancode it doesn't translate;
    // Input must not let that alias onto a real key's storage.
    input.beginFrame();
    input.handleKeyEvent( Rhiza::Key::Unknown, true );
    check( !input.isKeyDown( Rhiza::Key::Unknown ), "Unknown never reads as down" );
}

void testReleaseAll()
{
    std::printf( "release all:\n" );
    Rhiza::Input input;

    input.beginFrame();
    input.handleKeyEvent( Rhiza::Key::LeftShift, true );
    input.handleKeyEvent( Rhiza::Key::W, true );

    // Simulates losing window focus mid-hold: both keys must drop even
    // though no key-up event for either was ever seen.
    input.releaseAll();
    check( !input.isKeyDown( Rhiza::Key::LeftShift ), "releaseAll clears LeftShift" );
    check( !input.isKeyDown( Rhiza::Key::W ), "releaseAll clears W" );

    input.beginFrame();
    check( !input.wasKeyPressed( Rhiza::Key::W ),
           "regaining focus does not replay a press for a key released this way" );
}

}  // namespace

int main()
{
    testPressAndRelease();
    testKeysAreIndependent();
    testUnknownKeyIsIgnored();
    testReleaseAll();

    if( gFailures == 0 )
    {
        std::printf( "\nAll input tests passed.\n" );
        return 0;
    }

    std::printf( "\n%d check(s) failed.\n", gFailures );
    return 1;
}
