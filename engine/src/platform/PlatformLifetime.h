#pragma once

namespace Rhiza
{

// Owns SDL's process-wide state. Window and AudioOutput start and stop only
// their own subsystems; SDL's final cleanup waits until both are done.
class PlatformLifetime
{
public:
    PlatformLifetime() = default;
    ~PlatformLifetime();

    PlatformLifetime( const PlatformLifetime & ) = delete;
    PlatformLifetime &operator=( const PlatformLifetime & ) = delete;
};

}  // namespace Rhiza
