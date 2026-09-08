#pragma once

#include <chrono>

namespace Rhiza
{

// Two clocks, not one: hitstop has to freeze gameplay while input, UI and
// profiling keep running at full speed. Retrofitting that split would mean
// auditing every call site, so it exists from the first frame.
class Clock
{
public:
    Clock();

    // Once per frame, before reading either delta below.
    void tick();

    // Real time - drives input, UI and profiling.
    float realDeltaSeconds() const { return mRealDeltaSeconds; }

    // Scaled time - drives gameplay and in-world animation.
    float scaledDeltaSeconds() const { return mRealDeltaSeconds * mTimeScale; }

    // 1 = normal, 0 = frozen, negative = running backward. Deliberately
    // unclamped so all three stay reachable.
    float timeScale() const { return mTimeScale; }
    void setTimeScale( float scale ) { mTimeScale = scale; }

private:
    // A frame stalled behind a breakpoint, an OS suspend, or Ogre's
    // first-frame shader warm-up would otherwise hand gameplay a
    // multi-second delta and let it step clean through a wall. Unity's
    // equivalent knob:
    // https://docs.unity3d.com/ScriptReference/Time-maximumDeltaTime.html
    static constexpr float kMaxRealDeltaSeconds = 0.25f;

    std::chrono::steady_clock::time_point mLastTick = std::chrono::steady_clock::now();
    float mRealDeltaSeconds = 0.0f;
    float mTimeScale = 1.0f;
};

}  // namespace Rhiza
