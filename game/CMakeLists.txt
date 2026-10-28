project(Game VERSION 0.1 LANGUAGES CXX)

if(ANDROID)
    # SDL's Java activity loads the game as libmain.so and calls its SDL_main.
    add_library(${PROJECT_NAME} SHARED src/main.cpp)
    set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME main)
else()
    add_executable(${PROJECT_NAME} src/main.cpp)
endif()
rhiza_enable_warnings(${PROJECT_NAME})

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
#
# On Android, Gradle packs this folder into the APK itself (game/android).
if(NOT ANDROID)
    add_custom_target(GameAssets
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/assets
                ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/assets
    )
    add_dependencies(${PROJECT_NAME} GameAssets)
endif()