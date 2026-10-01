#include "AssetPath.h"

#include <algorithm>
#include <filesystem>

namespace Rhiza
{

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

std::string textureCacheKey( const std::string &normalizedPath, const TextureDesc &desc )
{
    return normalizedPath + ( desc.filter == TextureFilter::Nearest ? "|nearest" : "|linear" ) +
           ( desc.wrap == TextureWrap::Repeat ? "|repeat" : "|clamp" );
}

}  // namespace Rhiza
