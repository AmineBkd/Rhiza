cmake_minimum_required(VERSION 3.25)
project(RhizaRoot CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Output all executables to a 'bin' folder at the root of the project
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

find_package(SDL3 CONFIG REQUIRED)
find_package(OgreNext REQUIRED)

# Header-only, and vcpkg ships no CMake package for it.
find_path(CGLTF_INCLUDE_DIR cgltf.h REQUIRED)
find_path(MINIAUDIO_INCLUDE_DIR miniaudio.h REQUIRED)
find_path(STB_INCLUDE_DIR stb_vorbis.c REQUIRED)

enable_testing()

add_subdirectory(engine)
add_subdirectory(framework)
add_subdirectory(game)
add_subdirectory(tests)