#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include <Rhiza/Types/Mesh.h>

namespace Rhiza
{

using FileReader = std::function<bool( const std::string &path, std::vector<uint8_t> &out )>;

// Static geometry only: every triangle primitive in the scene, merged into
// one mesh with node transforms baked in. `path` is where `bytes` came from,
// so buffers stored in separate files are found next to it via `readFile`.
bool parseGltfMesh( const std::vector<uint8_t> &bytes, const std::string &path,
                    const FileReader &readFile, MeshDesc &out, std::string &error );

}  // namespace Rhiza
