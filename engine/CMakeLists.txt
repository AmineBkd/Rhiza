project(RhizaEngine VERSION 0.1 LANGUAGES CXX)

add_library(${PROJECT_NAME} STATIC
    src/RhizaEngine.cpp
    src/Window.cpp
    src/Renderer.cpp
    include/Rhiza/RhizaEngine.h
    include/Rhiza/Types.h
    src/NativeWindowHandle.h
    src/Window.h
    src/Renderer.h
)

target_include_directories(${PROJECT_NAME}
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(${PROJECT_NAME} PRIVATE SDL3::SDL3 OgreNext::OgreNext)

# Consumed by Renderer.cpp: where this engine's own copy of the Hlms shader
# templates lives, and where the vcpkg-built OGRE-Next plugin DLLs
# (RenderSystem_Direct3D11.dll) live. See engine/media/Hlms/ATTRIBUTION.txt
# and cmake/FindOgreNext.cmake for why both are needed.
target_compile_definitions(${PROJECT_NAME} PRIVATE
    RHIZA_MEDIA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/media"
    RHIZA_OGRE_INSTALL_ROOT="${OGRENEXT_INSTALL_ROOT}"
    $<$<CONFIG:Debug>:RHIZA_DEBUG_BUILD>
)