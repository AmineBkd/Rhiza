#pragma once

#include <cstdint>

namespace Rhiza
{

// An opaque reference to a row in a Registry - "an entity" in ECS terms.
// Carries no data of its own; every property of what it "is" comes from the
// components attached to it.
//
// Two fields, not one: `index` names the storage slot, `generation` is
// bumped every time that slot is recycled after a destroyEntity(). A stale
// Entity - one saved before its owner was destroyed and the slot handed to
// something new - keeps the old generation, so Registry can tell "this
// handle is dead" from "this handle points at a live entity" instead of
// silently aliasing onto whatever now occupies that slot. This is the same
// problem SceneNodeHandle/MeshHandle/etc. in engine/ don't yet solve (see
// rhiza-design/TODO.md's "Known debt") - solved here because an ECS
// recycles slots constantly, so skipping it wouldn't survive first contact.
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
