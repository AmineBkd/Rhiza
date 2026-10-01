#include "AssetPath.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <system_error>

namespace Rhiza
{

namespace
{

bool equalsIgnoringCase( const std::string &a, const std::string &b )
{
    return a.size() == b.size() &&
           std::equal( a.begin(), a.end(), b.begin(), []( char x, char y ) {
               return std::tolower( static_cast<unsigned char>( x ) ) ==
                      std::tolower( static_cast<unsigned char>( y ) );
           } );
}

// An exact match wins even if a case-only match was listed first: on a
// case-sensitive file system both can exist side by side.
std::string findEntrySpelling( const std::filesystem::path &directory, const std::string &wanted )
{
    std::error_code error;
    std::filesystem::directory_iterator it( directory, error );
    std::string caseOnlyMatch;
    for( ; !error && it != std::filesystem::directory_iterator(); it.increment( error ) )
    {
        const std::string name = it->path().filename().generic_string();
        if( name == wanted )
            return name;
        if( caseOnlyMatch.empty() && equalsIgnoringCase( name, wanted ) )
            caseOnlyMatch = name;
    }
    return caseOnlyMatch;
}

}  // namespace

std::string normalizeAssetPath( const std::string &path )
{
    // Converted first so Linux, where a backslash is a legal filename
    // character, normalises "a\b" the same way Windows does.
    std::string forwardSlashed = path;
    std::replace( forwardSlashed.begin(), forwardSlashed.end(), '\\', '/' );
    return std::filesystem::path( forwardSlashed ).lexically_normal().generic_string();
}

std::string joinAssetPath( const std::string &root, const std::string &relativePath )
{
    if( root.empty() )
        return relativePath;
    if( root.back() == '/' || root.back() == '\\' )
        return root + relativePath;
    return root + "/" + relativePath;
}

std::string findCaseMismatch( const std::string &root, const std::string &relativePath )
{
    std::filesystem::path directory( root );
    std::string spelledOnDisk;
    bool differs = false;
    for( const std::filesystem::path &component : std::filesystem::path( relativePath ) )
    {
        const std::string wanted = component.generic_string();
        const std::string found = findEntrySpelling( directory, wanted );
        if( found.empty() )
            return {};

        differs = differs || found != wanted;
        spelledOnDisk = spelledOnDisk.empty() ? found : spelledOnDisk + "/" + found;
        directory /= found;
    }
    return differs ? spelledOnDisk : std::string();
}

std::string textureCacheKey( const std::string &normalizedPath, const TextureDesc &desc )
{
    return normalizedPath + ( desc.filter == TextureFilter::Nearest ? "|nearest" : "|linear" ) +
           ( desc.wrap == TextureWrap::Repeat ? "|repeat" : "|clamp" );
}

}  // namespace Rhiza
