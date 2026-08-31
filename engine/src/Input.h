#pragma once

#include <bitset>

#include <Rhiza/Types.h>

namespace Rhiza
{

// Pure keyboard state - no SDL type appears here or in Input.cpp. Window
// drains the SDL event queue and reports each transition through
// handleKeyEvent(); Input only ever deals in Rhiza::Key.
//
// Events arrive mid-frame, in a burst, whenever pollEvents() drains the
// queue - but isDown/wasPressed/wasReleased need to give one stable answer
// for the rest of the frame, no matter how many times gameplay code asks.
// beginFrame() is the line that makes that true: call it once, before
// draining events, so "pressed" always means "transitioned since the frame
// before," never "transitioned since the last time someone happened to
// ask."
class Input
{
public:
    // Snapshots the current state as "previous" for this frame's edge
    // detection. Call exactly once per frame, before any handleKeyEvent().
    void beginFrame();

    // Records a key transition. Key::Unknown is silently ignored - it means
    // Window saw a key it doesn't translate, not a real key going down.
    void handleKeyEvent( Key key, bool isDown );

    // Releases every key at once, as if each had a key-up event. For when
    // the window loses focus (alt-tab, task switch): the OS can swallow the
    // matching key-up, and without this a key held at that moment reads as
    // stuck down for the rest of the session.
    void releaseAll();

    bool isKeyDown( Key key ) const;

    // True only on the frame a key went from up to down - the physical
    // press, not the hold. Window filters out SDL's key-repeat events
    // before they ever reach here, so holding a key never re-triggers this.
    bool wasKeyPressed( Key key ) const;

    // True only on the frame a key went from down to up.
    bool wasKeyReleased( Key key ) const;

private:
    static constexpr size_t kKeyCount = static_cast<size_t>( Key::Count );

    std::bitset<kKeyCount> mCurrent;
    std::bitset<kKeyCount> mPrevious;
};

}  // namespace Rhiza
