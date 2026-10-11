project(RhizaEngine VERSION 0.1 LANGUAGES CXX)

# Third-party implementation files we compile ourselves, apart from our own
# code so they build with warnings off. SYSTEM keeps their headers quiet in
# every target that links this.
add_library(RhizaThirdParty STATIC
    src/audio/MiniaudioImplementation.cpp
    src/core/CgltfImplementation.cpp
)
target_include_directories(RhizaThirdParty SYSTEM PUBLIC
    ${CGLTF_INCLUDE_DIR} ${MINIAUDIO_INCLUDE_DIR} ${STB_INCLUDE_DIR}
)
target_compile_options(RhizaThirdParty PRIVATE $<IF:$<CXX_COMPILER_ID:MSVC>,/W0,-w>)

# src/ is split by layer, and dependencies point one way: toward core.
# platform/, render/ and audio/ never include one another - they meet only
# through core/ types or plain callbacks wired up in Engine.cpp. SDL appears
# only under platform/, Ogre only under render/, miniaudio only under audio/.
# Each layer is its own library so a test links only the layer it tests.
add_library(RhizaCore STATIC
    src/core/Clock.cpp
    src/core/Shapes.cpp
    src/core/Math.cpp
    src/core/AssetPath.cpp
    src/core/AssetCache.cpp
    src/core/Gltf.cpp
    src/core/Clock.h
    src/core/AssetPath.h
    src/core/AssetCache.h
    src/core/Gltf.h
    src/core/NativeWindowHandle.h

    include/Rhiza/Types/Audio.h
    include/Rhiza/Types/Camera.h
    include/Rhiza/Types/EngineSettings.h
    include/Rhiza/Types/Handles.h
    include/Rhiza/Types/Input.h
    include/Rhiza/Types/Light.h
    include/Rhiza/Types/Material.h
    include/Rhiza/Types/Math.h
    include/Rhiza/Types/Mesh.h
    include/Rhiza/Types/Texture.h
    include/Rhiza/Shapes.h
)
target_include_directories(RhizaCore PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(RhizaCore PRIVATE RhizaThirdParty)
rhiza_enable_warnings(RhizaCore)

add_library(RhizaPlatform STATIC
    src/platform/Window.cpp
    src/platform/Input.cpp
    src/platform/FileSystem.cpp
    src/platform/AudioOutput.cpp
    src/platform/PlatformLifetime.cpp
    src/platform/Window.h
    src/platform/Input.h
    src/platform/FileSystem.h
    src/platform/AudioOutput.h
    src/platform/PlatformLifetime.h
)
target_link_libraries(RhizaPlatform PUBLIC RhizaCore PRIVATE SDL3::SDL3)
rhiza_enable_warnings(RhizaPlatform)

add_library(RhizaAudio STATIC
    src/audio/Audio.cpp
    src/audio/Audio.h
    src/audio/Miniaudio.h
)
target_link_libraries(RhizaAudio PUBLIC RhizaCore PRIVATE RhizaThirdParty)
rhiza_enable_warnings(RhizaAudio)

add_library(RhizaRender STATIC
    src/render/Renderer.cpp
    src/render/Renderer.h
)
target_link_libraries(RhizaRender PUBLIC RhizaCore PRIVATE OgreNext::OgreNext)
rhiza_enable_warnings(RhizaRender)

# Consumed by render/Renderer.cpp: where this engine's own copy of the Hlms
# shader templates lives, and the vcpkg triplet install root holding the
# OGRE-Next render system plugin. See engine/media/Hlms/ATTRIBUTION.txt and
# cmake/FindOgreNext.cmake.
target_compile_definitions(RhizaRender PRIVATE
    RHIZA_MEDIA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/media"
    RHIZA_OGRE_INSTALL_ROOT="${OGRENEXT_INSTALL_ROOT}"
)

# Selects the _d render system plugin. Only Windows pairs a debug app with
# debug Ogre - see cmake/FindOgreNext.cmake for why everywhere else uses the
# release libraries regardless of build type.
if(WIN32)
    target_compile_definitions(RhizaRender PRIVATE $<$<CONFIG:Debug>:RHIZA_DEBUG_BUILD>)
endif()

# The facade, and the only target a game links. The layers stay private, so
# their headers and src/ never reach game code.
add_library(${PROJECT_NAME} STATIC
    src/Engine.cpp
    include/Rhiza/Engine.h
)
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(${PROJECT_NAME} PRIVATE RhizaCore RhizaPlatform RhizaAudio RhizaRender)
rhiza_enable_warnings(${PROJECT_NAME})
