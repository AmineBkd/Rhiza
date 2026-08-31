project(RhizaFramework LANGUAGES CXX)

# Header-only: Entity and Registry are templates, so there is nothing to
# compile into a .cpp yet. INTERFACE just propagates the include path.
add_library(${PROJECT_NAME} INTERFACE)

target_include_directories(${PROJECT_NAME}
    INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include
)
