#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

namespace Rhiza
{

// Which handle each loaded asset key maps to, and how many loads of it are
// outstanding. Pure bookkeeping: the caller owns the actual resources.
class AssetCache
{
public:
    // The handle cached under `key`, counted as one more load; 0 if none.
    uint32_t acquire( const std::string &key );

    // A freshly loaded asset, with one load outstanding.
    void add( const std::string &key, uint32_t handle );

    // Undoes one load if others still hold the asset. False for its last
    // load - and for handles never cached - meaning the caller should free
    // the asset, then forget() it.
    bool releaseIfShared( uint32_t handle );

    void forget( uint32_t handle );
    void clear();

    uint32_t loadCount( uint32_t handle ) const;

private:
    struct Entry
    {
        std::string key;
        uint32_t loadCount = 1;
    };

    std::unordered_map<std::string, uint32_t> mHandleByKey;
    std::unordered_map<uint32_t, Entry> mEntryByHandle;
};

}  // namespace Rhiza
