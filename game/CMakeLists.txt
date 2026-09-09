project(Expansum VERSION 0.1 LANGUAGES CXX)

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