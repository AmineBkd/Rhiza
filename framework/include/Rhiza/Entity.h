#pragma once

#include <cstdint>

namespace Rhiza
{

// An opaque reference to a row in a Registry. Carries no data of its own;
// everything it "is" comes from its components.
//
// `generation` is bumped every time a slot is recycled, so a handle saved
// before its owner was destroyed reads as dead instead of silently aliasing
// onto whatever now occupies that slot.
struct Entity
{
    static constexpr uint32_t kInvalidIndex = 0xFFFFFFFFu;

    uint32_t index = kInvalidIndex;
    uint32_t generation = 0;

    bool isValid() const { return index != kInvalidIndex; }

    bool operator==( const Entity &other ) const
    {
        return index == other.index && generation == other.generation;
    }
    bool operator!=( const Entity &other ) const { return !( *this == other ); }
};

}  // namespace Rhiza
