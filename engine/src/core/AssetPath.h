#pragma once

#include <string>

#include <Rhiza/Types/Texture.h>

namespace Rhiza
{

// The cache key for a path: forward slashes, no "." or ".." segments, no
// doubled separators. Case is kept: Android and Linux paths are
// case-sensitive, so "Ship.png" and "ship.png" are different assets.
std::string normalizeAssetPath( const std::string &path );

std::string joinAssetPath( const std::string &root, const std::string &relativePath );

// One file loaded with different settings is a different GPU texture (mips
// or not), so the settings are part of its key.
std::string textureCacheKey( const std::string &normalizedPath, const TextureDesc &desc );

}  // namespace Rhiza
