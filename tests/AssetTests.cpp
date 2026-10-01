// Tests for asset keys and the cache's bookkeeping. Pure data - no GPU,
// window or file system - so they run anywhere, CI included.

#include "core/AssetCache.h"
#include "core/AssetPath.h"

#include <cstdio>
#include <string>

namespace
{

int gFailures = 0;

void check( bool condition, const std::string &what )
{
    if( !condition )
    {
        std::printf( "  FAIL: %s\n", what.c_str() );
        ++gFailures;
    }
}

void testNormalizeAssetPath()
{
    std::printf( "normalizeAssetPath:\n" );
    check( Rhiza::normalizeAssetPath( "ship.png" ) == "ship.png", "plain name unchanged" );
    check( Rhiza::normalizeAssetPath( "./ship.png" ) == "ship.png", "leading ./ removed" );
    check( Rhiza::normalizeAssetPath( "textures//ship.png" ) == "textures/ship.png", "doubled slash collapsed" );
    check( Rhiza::normalizeAssetPath( "textures\\ship.png" ) == "textures/ship.png", "backslash becomes slash" );
    check( Rhiza::normalizeAssetPath( "a/b/../ship.png" ) == "a/ship.png", ".. resolved" );
    check( Rhiza::normalizeAssetPath( "Ship.png" ) == "Ship.png", "case kept" );
}

void testJoinAssetPath()
{
    std::printf( "joinAssetPath:\n" );
    check( Rhiza::joinAssetPath( "", "a.png" ) == "a.png", "empty root leaves path relative" );
    check( Rhiza::joinAssetPath( "root", "a.png" ) == "root/a.png", "separator added" );
    check( Rhiza::joinAssetPath( "root/", "a.png" ) == "root/a.png", "no doubled separator" );
    check( Rhiza::joinAssetPath( "C:\\game\\", "a.png" ) == "C:\\game\\a.png", "Windows root kept" );
}

void testCacheReturnsTheSameHandle()
{
    std::printf( "cache: same key, same handle:\n" );
    Rhiza::AssetCache cache;
    check( cache.acquire( "rock.glb" ) == 0, "nothing cached yet" );

    cache.add( "rock.glb", 7 );
    check( cache.acquire( "rock.glb" ) == 7, "second load gets the first load's handle" );
    check( cache.acquire( "rock.glb" ) == 7, "and so does a third" );
    check( cache.loadCount( 7 ) == 3, "three loads counted" );

    check( cache.acquire( "ship.png" ) == 0, "a different key is not a hit" );
    cache.add( "ship.png", 8 );
    check( cache.acquire( "ship.png" ) == 8 && cache.loadCount( 7 ) == 3, "keys stay independent" );
}

void testCacheFreesOnTheLastRelease()
{
    std::printf( "cache: freed by the last release only:\n" );
    Rhiza::AssetCache cache;
    cache.add( "rock.glb", 7 );
    cache.acquire( "rock.glb" );
    cache.acquire( "rock.glb" );

    check( cache.releaseIfShared( 7 ), "first release: others still hold it" );
    check( cache.releaseIfShared( 7 ), "second release: one still holds it" );
    check( cache.loadCount( 7 ) == 1, "one load left" );
    check( !cache.releaseIfShared( 7 ), "last release tells the caller to free it" );
    check( cache.loadCount( 7 ) == 1, "and does not count below one by itself" );

    cache.forget( 7 );
    check( cache.acquire( "rock.glb" ) == 0, "forgotten: the next load must load again" );
    check( cache.loadCount( 7 ) == 0, "nothing counted for the old handle" );

    cache.add( "rock.glb", 12 );
    check( cache.acquire( "rock.glb" ) == 12, "reloaded under a new handle" );
}

void testCacheIgnoresUnknownHandles()
{
    std::printf( "cache: handles it never saw:\n" );
    Rhiza::AssetCache cache;
    check( !cache.releaseIfShared( 99 ), "uncached handle: caller frees it as usual" );
    cache.forget( 99 );
    cache.add( "a", 1 );
    cache.forget( 2 );
    check( cache.acquire( "a" ) == 1, "forgetting an unknown handle leaves others alone" );

    cache.clear();
    check( cache.acquire( "a" ) == 0, "clear drops everything" );
}

void testSameNameInDifferentFolders()
{
    std::printf( "cache: same file name in different folders:\n" );
    const std::string ships = Rhiza::normalizeAssetPath( "ships/hull.png" );
    const std::string stations = Rhiza::normalizeAssetPath( "stations/hull.png" );
    check( ships != stations, "keys keep the folder" );

    Rhiza::AssetCache cache;
    cache.add( ships, 1 );
    check( cache.acquire( stations ) == 0, "the other folder's hull.png is not a hit" );
    cache.add( stations, 2 );
    check( cache.acquire( ships ) == 1 && cache.acquire( stations ) == 2, "both cached side by side" );

    const Rhiza::TextureDesc linear;
    check( Rhiza::textureCacheKey( ships, linear ) != Rhiza::textureCacheKey( stations, linear ),
           "texture keys differ too" );
}

void testTextureCacheKey()
{
    std::printf( "texture keys: settings are part of identity:\n" );
    const Rhiza::TextureDesc linear;
    Rhiza::TextureDesc nearest;
    nearest.filter = Rhiza::TextureFilter::Nearest;
    Rhiza::TextureDesc repeating;
    repeating.wrap = Rhiza::TextureWrap::Repeat;

    check( Rhiza::textureCacheKey( "ship.png", linear ) == Rhiza::textureCacheKey( "ship.png", linear ),
           "same file, same settings: same key" );
    check( Rhiza::textureCacheKey( "ship.png", linear ) != Rhiza::textureCacheKey( "ship.png", nearest ),
           "different filter: different key" );
    check( Rhiza::textureCacheKey( "ship.png", linear ) != Rhiza::textureCacheKey( "ship.png", repeating ),
           "different wrap: different key" );
    check( Rhiza::textureCacheKey( Rhiza::normalizeAssetPath( "./textures//ship.png" ), linear ) ==
               Rhiza::textureCacheKey( Rhiza::normalizeAssetPath( "textures/ship.png" ), linear ),
           "two spellings of one path: same key" );
}

}  // namespace

int main()
{
    testNormalizeAssetPath();
    testJoinAssetPath();
    testCacheReturnsTheSameHandle();
    testCacheFreesOnTheLastRelease();
    testCacheIgnoresUnknownHandles();
    testSameNameInDifferentFolders();
    testTextureCacheKey();

    if( gFailures == 0 )
    {
        std::printf( "\nAll asset tests passed.\n" );
        return 0;
    }

    std::printf( "\n%d check(s) failed.\n", gFailures );
    return 1;
}
