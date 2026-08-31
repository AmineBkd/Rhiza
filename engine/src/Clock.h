#pragma once

#include <chrono>

namespace Rhiza
{

// Two values, not one - see rhiza-design/TODO.md's "Frame timing + input"
// entry. Act 1 needs hitstop (freezing or slowing gameplay for a few frames
// on impact), which has to leave input, UI and profiling running at full
// speed while gameplay itself stops. A single
// deltaTime can't represent both states at once, and every call site that
// ever reads it would need auditing if the split were bolted on later, so it
// exists from the first frame that has any notion of time at all.
class Clock
{
public:
    Clock();

    // Call once per frame, before reading either delta below. Measures real
    // time elapsed since the previous call (or since construction, on the
    // first call).
    void tick();

    // Seconds since the previous tick(), unscaled. Always advances at the
    // rate the wall clock does - drives input, UI and profiling, none of
    // which should freeze during a hitstop.
    float realDeltaSeconds() const { return mRealDeltaSeconds; }

    // realDeltaSeconds() * timeScale(). Drives gameplay and in-world
    // animation - the value hitstop freezes (scale 0) or slows (0 < scale < 1).
    float scaledDeltaSeconds() const { return mRealDeltaSeconds * mTimeScale; }

    // Multiplier applied to real time to produce scaled time. 1 = normal,
    // 0 = frozen, negative = time running backward (a rewind effect) -
    // deliberately unclamped so any of those stay reachable.
    float timeScale() const { return mTimeScale; }
    void setTimeScale( float scale ) { mTimeScale = scale; }

private:
    // A frame stalled behind a breakpoint, an OS suspend, or (on the very
    // first tick()) Ogre's shader/Hlms warm-up would otherwise hand gameplay
    // a multi-second delta and let it step clean through a wall or a
    // collision in one frame. Capping it trades perfect timing accuracy on
    // that one frame for never handing simulation code a delta it can't
    // survive - the standard fix (Unity calls the equivalent knob
    // maximumDeltaTime).
    static constexpr float kMaxRealDeltaSeconds = 0.25f;

    std::chrono::steady_clock::time_point mLastTick = std::chrono::steady_clock::now();
    float mRealDeltaSeconds = 0.0f;
    float mTimeScale = 1.0f;
};

}  // namespace Rhiza
