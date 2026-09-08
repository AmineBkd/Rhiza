#pragma once

#include <bitset>

#include <Rhiza/Types.h>

namespace Rhiza
{

// Pure keyboard state; no SDL type appears here or in Input.cpp. Window
// drains the event queue and reports transitions through handleKeyEvent().
class Input
{
public:
    // Snapshots current state as "previous". Exactly once per frame, before
    // any handleKeyEvent - that is what makes "pressed" mean "since last
    // frame" rather than "since someone last asked".
    void beginFrame();

    // Key::Unknown is ignored: it means Window saw a key it doesn't
    // translate, not a real key going down.
    void handleKeyEvent( Key key, bool isDown );

    // Releases every key, as if each got a key-up. For focus loss, where the
    // OS can swallow the real key-up and leave a key stuck down forever.
    void releaseAll();

    bool isKeyDown( Key key ) const;
    bool wasKeyPressed( Key key ) const;
    bool wasKeyReleased( Key key ) const;

private:
    static constexpr size_t kKeyCount = static_cast<size_t>( Key::Count );

    std::bitset<kKeyCount> mCurrent;
    std::bitset<kKeyCount> mPrevious;
};

}  // namespace Rhiza
