cmake_minimum_required(VERSION 3.16)
project(RhizaRoot CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Output all executables to a 'bin' folder at the root of the project
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
#set(SDL_RENDER OFF CACHE BOOL "" FORCE)

add_subdirectory(third_party/sdl3 EXCLUDE_FROM_ALL)

add_subdirectory(engine)
add_subdirectory(game)