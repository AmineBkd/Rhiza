cmake_minimum_required(VERSION 3.16)
project(RhizaRoot CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Output all executables to a 'bin' folder at the root of the project
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

find_package(SDL3 CONFIG REQUIRED)
find_package(OgreNext REQUIRED)

enable_testing()

add_subdirectory(engine)
add_subdirectory(game)
add_subdirectory(tests)