#include "AssetCache.h"

namespace Rhiza
{

uint32_t AssetCache::acquire( const std::string &key )
{
    auto it = mHandleByKey.find( key );
    if( it == mHandleByKey.end() )
        return 0;

    ++mEntryByHandle[it->second].loadCount;
    return it->second;
}

void AssetCache::add( const std::string &key, uint32_t handle )
{
    mHandleByKey[key] = handle;
    mEntryByHandle[handle] = Entry{ key, 1 };
}

bool AssetCache::releaseIfShared( uint32_t handle )
{
    auto it = mEntryByHandle.find( handle );
    if( it == mEntryByHandle.end() || it->second.loadCount <= 1 )
        return false;

    --it->second.loadCount;
    return true;
}

void AssetCache::forget( uint32_t handle )
{
    auto it = mEntryByHandle.find( handle );
    if( it == mEntryByHandle.end() )
        return;

    mHandleByKey.erase( it->second.key );
    mEntryByHandle.erase( it );
}

void AssetCache::clear()
{
    mHandleByKey.clear();
    mEntryByHandle.clear();
}

uint32_t AssetCache::loadCount( uint32_t handle ) const
{
    auto it = mEntryByHandle.find( handle );
    return it == mEntryByHandle.end() ? 0 : it->second.loadCount;
}

}  // namespace Rhiza
