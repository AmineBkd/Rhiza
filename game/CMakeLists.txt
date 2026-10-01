project(Game VERSION 0.1 LANGUAGES CXX)

# Create game executable
add_executable(${PROJECT_NAME}
    src/main.cpp
)

# Link Engine
# RenderSystem.h includes both, so the game links both. RhizaFramework
# deliberately does not depend on RhizaEngine itself - that would drag SDL and
# Ogre into RegistryTests, which is meant to run anywhere.
target_link_libraries(${PROJECT_NAME} PRIVATE
    RhizaEngine
    RhizaFramework
)

# The engine's default asset root is "assets" beside the executable. A
# custom target rather than a POST_BUILD step: that only runs when the exe
# relinks, so an edited asset alone would never be copied. The folder is
# spelled out because $<TARGET_FILE_DIR> would make this target depend on
# the exe, which already depends on it.
get_property(_isMultiConfig GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(_isMultiConfig)
    set(_exeDir "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>")
else()
    set(_exeDir "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
endif()
add_custom_target(GameAssets
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/assets ${_exeDir}/assets
)
add_dependencies(${PROJECT_NAME} GameAssets)