project(RhizaEngine VERSION 0.1 LANGUAGES CXX)

add_library(${PROJECT_NAME} STATIC
    src/RhizaEngine.cpp
    include/Rhiza/RhizaEngine.h
)

# Set header search paths
target_include_directories(${PROJECT_NAME}
    PUBLIC  include
    PRIVATE src
)

# PRIVATE link so SDL3 is strictly internal to the engine
target_link_libraries(${PROJECT_NAME} PRIVATE SDL3::SDL3-static)