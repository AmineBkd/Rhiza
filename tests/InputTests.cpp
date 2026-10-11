// Tests for Rhiza::Input.
//
// Input is pure state - no window, no SDL, no GPU - so like Shapes it can be
// checked in a plain console process. It's worth testing properly because
// the bug class it's prone to (a key reads as "just pressed" on the wrong
// frame, or stays stuck down) only shows up as wrong gameplay feel much
// later, at a point where the input code is the last place anyone looks.

#include "platform/Input.h"

#include <catch2/catch_test_macros.hpp>

#include <string>

namespace
{

TEST_CASE( "Input: press and release" )
{
    Rhiza::Input input;

    CHECK( !input.isKeyDown( Rhiza::Key::W ) );

    // A key going down mid-frame (as Window reports it) is visible
    // immediately, and counts as freshly pressed for the rest of that frame.
    input.beginFrame();
    input.handleKeyEvent( Rhiza::Key::W, true );
    CHECK( input.isKeyDown( Rhiza::Key::W ) );
    CHECK( input.wasKeyPressed( Rhiza::Key::W ) );
    CHECK( !input.wasKeyReleased( Rhiza::Key::W ) );

    // Holding the key into the next frame must not re-trigger "just pressed" -
    // that's the entire reason beginFrame() exists.
    input.beginFrame();
    CHECK( input.isKeyDown( Rhiza::Key::W ) );
    CHECK( !input.wasKeyPressed( Rhiza::Key::W ) );

    input.handleKeyEvent( Rhiza::Key::W, false );
    CHECK( !input.isKeyDown( Rhiza::Key::W ) );
    CHECK( input.wasKeyReleased( Rhiza::Key::W ) );
    CHECK( !input.wasKeyPressed( Rhiza::Key::W ) );

    input.beginFrame();
    CHECK( !input.wasKeyReleased( Rhiza::Key::W ) );
}

TEST_CASE( "Input: keys are independent" )
{
    Rhiza::Input input;

    input.beginFrame();
    input.handleKeyEvent( Rhiza::Key::A, true );

    CHECK( input.isKeyDown( Rhiza::Key::A ) );
    CHECK( !input.isKeyDown( Rhiza::Key::D ) );
    CHECK( !input.wasKeyPressed( Rhiza::Key::D ) );
}

TEST_CASE( "Input: unknown key is ignored" )
{
    Rhiza::Input input;

    // Window reports Key::Unknown for any scancode it doesn't translate;
    // Input must not let that alias onto a real key's storage.
    input.beginFrame();
    input.handleKeyEvent( Rhiza::Key::Unknown, true );
    CHECK( !input.isKeyDown( Rhiza::Key::Unknown ) );
}

TEST_CASE( "Input: release all" )
{
    Rhiza::Input input;

    input.beginFrame();
    input.handleKeyEvent( Rhiza::Key::LeftShift, true );
    input.handleKeyEvent( Rhiza::Key::W, true );

    // Simulates losing window focus mid-hold: both keys must drop even
    // though no key-up event for either was ever seen.
    input.releaseAll();
    CHECK( !input.isKeyDown( Rhiza::Key::LeftShift ) );
    CHECK( !input.isKeyDown( Rhiza::Key::W ) );

    input.beginFrame();
    CHECK( !input.wasKeyPressed( Rhiza::Key::W ) );
}

}  // namespace
