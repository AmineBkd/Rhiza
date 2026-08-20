cmake_minimum_required(VERSION 3.16)
project(RhizaRoot CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Output all executables to a 'bin' folder at the root of the project
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

find_package(SDL3 CONFIG REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(OgreNext REQUIRED IMPORTED_TARGET OGRE-Next)

add_subdirectory(engine)
add_subdirectory(game)