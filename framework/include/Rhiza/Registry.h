#pragma once

#include <cstdint>
#include <memory>
#include <typeindex>
#include <unordered_map>
#include <utility>
#include <vector>

#include "Entity.h"

namespace Rhiza
{

namespace detail
{

// Type-erased base so Registry::destroyEntity can clean up every component
// pool for an entity without knowing the concrete component types involved.
class ComponentPoolBase
{
public:
    virtual ~ComponentPoolBase() = default;
    virtual void remove( Entity entity ) = 0;
};

// A sparse set: O(1) add/remove/lookup, and the components of one type
// packed contiguously for cache-friendly iteration - the two properties
// that make a sparse-set ECS worth using over a plain
// std::unordered_map<Entity, T> per component type.
//
// `mSparse[entity.index]` holds where that entity's component lives in the
// dense arrays, or kInvalidDenseIndex if it has none. Removal is
// swap-and-pop: the last dense element moves into the removed slot, so
// `mSparse` for *that* entity must be patched to point at its new home -
// the one subtlety a sparse-set gets wrong if written carelessly.
template <typename T>
class ComponentPool : public ComponentPoolBase
{
public:
    T &insert( Entity entity, T component )
    {
        if( entity.index >= mSparse.size() )
            mSparse.resize( entity.index + 1, kInvalidDenseIndex );

        const size_t denseIndex = mDense.size();
        mSparse[entity.index] = denseIndex;
        mDenseEntities.push_back( entity );
        mDense.push_back( std::move( component ) );
        return mDense.back();
    }

    void remove( Entity entity ) override
    {
        if( !contains( entity ) )
            return;

        const size_t removedIndex = mSparse[entity.index];
        const size_t lastIndex = mDense.size() - 1;

        mDense[removedIndex] = std::move( mDense[lastIndex] );
        mDenseEntities[removedIndex] = mDenseEntities[lastIndex];
        mSparse[mDenseEntities[removedIndex].index] = removedIndex;

        mDense.pop_back();
        mDenseEntities.pop_back();
        mSparse[entity.index] = kInvalidDenseIndex;
    }

    // Checks the *full* Entity, generation included - not just whether the
    // index slot has an entry. Without that, a stale handle whose index was
    // recycled would alias onto the new occupant's component.
    bool contains( Entity entity ) const
    {
        return entity.index < mSparse.size() && mSparse[entity.index] != kInvalidDenseIndex &&
               mDenseEntities[mSparse[entity.index]] == entity;
    }

    T *get( Entity entity )
    {
        return contains( entity ) ? &mDense[mSparse[entity.index]] : nullptr;
    }

    const T *get( Entity entity ) const
    {
        return contains( entity ) ? &mDense[mSparse[entity.index]] : nullptr;
    }

    size_t size() const { return mDense.size(); }
    Entity entityAt( size_t denseIndex ) const { return mDenseEntities[denseIndex]; }
    T &componentAt( size_t denseIndex ) { return mDense[denseIndex]; }

private:
    static constexpr size_t kInvalidDenseIndex = static_cast<size_t>( -1 );

    std::vector<size_t> mSparse;         // entity.index -> dense index
    std::vector<Entity> mDenseEntities;  // dense index -> owning entity
    std::vector<T> mDense;               // the packed components themselves
};

}  // namespace detail

// Iterates every (Entity, T&) pair currently holding a component of type T,
// in dense-array order - not creation order, since swap-and-pop reorders on
// removal. Do not add or remove components of type T while iterating a
// view over it: that mutates the array underneath the iterator.
template <typename T>
class View
{
public:
    explicit View( detail::ComponentPool<T> &pool ) : mPool( pool ) {}

    class Iterator
    {
    public:
        Iterator( detail::ComponentPool<T> &pool, size_t index ) : mPool( pool ), mIndex( index ) {}

        bool operator!=( const Iterator &other ) const { return mIndex != other.mIndex; }
        Iterator &operator++()
        {
            ++mIndex;
            return *this;
        }
        std::pair<Entity, T &> operator*() const
        {
            return { mPool.entityAt( mIndex ), mPool.componentAt( mIndex ) };
        }

    private:
        detail::ComponentPool<T> &mPool;
        size_t mIndex;
    };

    Iterator begin() const { return Iterator( mPool, 0 ); }
    Iterator end() const { return Iterator( mPool, mPool.size() ); }

private:
    detail::ComponentPool<T> &mPool;
};

// The ECS core: entity lifecycle plus per-type component storage. Nothing
// here knows what a Transform or a MeshRenderer is - concrete components
// are for game/engine code built on top of this, once the mesh/asset split
// they depend on has a stable shape (see rhiza-design/TODO.md).
//
// Deliberately a sparse-set, not an archetype/chunk design: the MVP's real
// entity count is in the hundreds, and archetype iteration solves a
// cache-locality problem this scale doesn't have. Small enough to replace
// with EnTT later if that ever becomes worthwhile - see TODO.md.
class Registry
{
public:
    Entity createEntity()
    {
        uint32_t index;
        if( !mFreeIndices.empty() )
        {
            index = mFreeIndices.back();
            mFreeIndices.pop_back();
        }
        else
        {
            index = static_cast<uint32_t>( mGenerations.size() );
            mGenerations.push_back( 0 );
        }
        return Entity{ index, mGenerations[index] };
    }

    // No-op if `entity` is already dead - double-destroying is a caller
    // mistake that shouldn't need guarding at every call site.
    void destroyEntity( Entity entity )
    {
        if( !isAlive( entity ) )
            return;

        for( auto &pool : mPools )
            pool.second->remove( entity );

        ++mGenerations[entity.index];
        mFreeIndices.push_back( entity.index );
    }

    // False for a default-constructed Entity, one already destroyed, or a
    // stale handle whose index has since been recycled into a new entity -
    // the three ways an Entity can fail to name something real right now.
    bool isAlive( Entity entity ) const
    {
        return entity.isValid() && entity.index < mGenerations.size() &&
               mGenerations[entity.index] == entity.generation;
    }

    template <typename T>
    T &addComponent( Entity entity, T component )
    {
        return poolFor<T>().insert( entity, std::move( component ) );
    }

    template <typename T>
    void removeComponent( Entity entity )
    {
        if( detail::ComponentPool<T> *pool = tryGetPool<T>() )
            pool->remove( entity );
    }

    template <typename T>
    bool hasComponent( Entity entity ) const
    {
        const detail::ComponentPool<T> *pool = tryGetPool<T>();
        return pool && pool->contains( entity );
    }

    template <typename T>
    T *getComponent( Entity entity )
    {
        detail::ComponentPool<T> *pool = tryGetPool<T>();
        return pool ? pool->get( entity ) : nullptr;
    }

    template <typename T>
    const T *getComponent( Entity entity ) const
    {
        const detail::ComponentPool<T> *pool = tryGetPool<T>();
        return pool ? pool->get( entity ) : nullptr;
    }

    // Only one component type per view - no query intersection (e.g.
    // "entities with both A and B") yet. Everything the MVP needs so far is
    // single-type; add intersection when a real caller needs it rather than
    // speculatively.
    template <typename T>
    View<T> view()
    {
        return View<T>( poolFor<T>() );
    }

private:
    template <typename T>
    detail::ComponentPool<T> *tryGetPool()
    {
        auto it = mPools.find( std::type_index( typeid( T ) ) );
        return it == mPools.end() ? nullptr : static_cast<detail::ComponentPool<T> *>( it->second.get() );
    }

    template <typename T>
    const detail::ComponentPool<T> *tryGetPool() const
    {
        auto it = mPools.find( std::type_index( typeid( T ) ) );
        return it == mPools.end() ? nullptr
                                   : static_cast<const detail::ComponentPool<T> *>( it->second.get() );
    }

    template <typename T>
    detail::ComponentPool<T> &poolFor()
    {
        if( detail::ComponentPool<T> *pool = tryGetPool<T>() )
            return *pool;

        auto pool = std::make_unique<detail::ComponentPool<T>>();
        detail::ComponentPool<T> &ref = *pool;
        mPools.emplace( std::type_index( typeid( T ) ), std::move( pool ) );
        return ref;
    }

    std::vector<uint32_t> mGenerations;  // entity.index -> current generation
    std::vector<uint32_t> mFreeIndices;  // recycled entity.index slots
    std::unordered_map<std::type_index, std::unique_ptr<detail::ComponentPoolBase>> mPools;
};

}  // namespace Rhiza
