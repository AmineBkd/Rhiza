#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace Rhiza
{

// SDL rather than std::ifstream: on Android it also reads inside the APK.
bool readFile( const std::string &path, std::vector<uint8_t> &out );

}  // namespace Rhiza
