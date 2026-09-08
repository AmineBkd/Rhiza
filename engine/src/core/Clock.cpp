#include "Clock.h"
#include <algorithm>

namespace Rhiza
{

Clock::Clock() = default;

void Clock::tick()
{
    const auto now = std::chrono::steady_clock::now();
    const float elapsed = std::chrono::duration<float>( now - mLastTick ).count();
    mLastTick = now;
    mRealDeltaSeconds = std::min( elapsed, kMaxRealDeltaSeconds );
}

}  // namespace Rhiza
