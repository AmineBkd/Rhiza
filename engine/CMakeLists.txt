project(RhizaEngine VERSION 0.1 LANGUAGES CXX)

# src/ is split by layer, and dependencies point one way: toward core.
# Nothing in platform/ may include render/, or vice versa - they meet only
# through core/NativeWindowHandle.h. SDL appears only under platform/, Ogre
# only under render/.
add_library(${PROJECT_NAME} STATIC
    src/Engine.cpp

    src/core/Clock.cpp
    src/core/Shapes.cpp
    src/core/Clock.h
    src/core/NativeWindowHandle.h

    src/platform/Window.cpp
    src/platform/Input.cpp
    src/platform/Window.h
    src/platform/Input.h

    src/render/Renderer.cpp
    src/render/Renderer.h

    include/Rhiza/Engine.h
    include/Rhiza/Types.h
    include/Rhiza/Shapes.h
)

target_include_directories(${PROJECT_NAME}
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(${PROJECT_NAME} PRIVATE SDL3::SDL3 OgreNext::OgreNext)

# Consumed by render/Renderer.cpp: where this engine's own copy of the Hlms
# shader templates lives, and the vcpkg triplet install root holding the
# OGRE-Next render system plugin. See engine/media/Hlms/ATTRIBUTION.txt and
# cmake/FindOgreNext.cmake.
target_compile_definitions(${PROJECT_NAME} PRIVATE
    RHIZA_MEDIA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/media"
    RHIZA_OGRE_INSTALL_ROOT="${OGRENEXT_INSTALL_ROOT}"
)

# Selects the _d render system plugin. Only Windows pairs a debug app with
# debug Ogre - see cmake/FindOgreNext.cmake for why everywhere else uses the
# release libraries regardless of build type.
if(WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE $<$<CONFIG:Debug>:RHIZA_DEBUG_BUILD>)
endif()
