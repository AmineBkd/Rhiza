cmake_minimum_required(VERSION 3.25)
project(RhizaRoot CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Output all executables to a 'bin' folder at the root of the project
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

# Git never turns on a repository's own hooks, so each clone does it here on first configure.
option(RHIZA_GIT_HOOKS "Run tools/hooks (the format check before a push)" ON)
find_package(Git QUIET)
if(RHIZA_GIT_HOOKS AND GIT_FOUND AND EXISTS ${CMAKE_SOURCE_DIR}/.git)
    execute_process(COMMAND ${GIT_EXECUTABLE} config core.hooksPath
                    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} OUTPUT_VARIABLE hooksPath OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(hooksPath STREQUAL "")
        execute_process(COMMAND ${GIT_EXECUTABLE} config core.hooksPath tools/hooks WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    endif()
endif()

list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)
include(RhizaWarnings)

find_package(SDL3 CONFIG REQUIRED)
find_package(OgreNext REQUIRED)
find_package(box2d CONFIG REQUIRED)

# Header-only, and vcpkg ships no CMake package for it.
find_path(CGLTF_INCLUDE_DIR cgltf.h REQUIRED)
find_path(MINIAUDIO_INCLUDE_DIR miniaudio.h REQUIRED)
find_path(STB_INCLUDE_DIR stb_vorbis.c REQUIRED)

enable_testing()

add_subdirectory(engine)
add_subdirectory(framework)
add_subdirectory(game)
add_subdirectory(tests)