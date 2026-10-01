#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace Rhiza
{

// SDL rather than std::ifstream: on Android it also reads inside the APK.
bool readFile( const std::string &path, std::vector<uint8_t> &out );

// "assets" under SDL's base path: beside the executable on desktop, inside
// the bundle's Resources on macOS. On Android the base path is "./", which
// SDL resolves through the APK.
std::string defaultAssetRoot();

}  // namespace Rhiza
