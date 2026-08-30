project(RhizaEngine VERSION 0.1 LANGUAGES CXX)

add_library(${PROJECT_NAME} STATIC
    src/RhizaEngine.cpp
    include/Rhiza/RhizaEngine.h
)

target_include_directories(${PROJECT_NAME}
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(${PROJECT_NAME} PRIVATE SDL3::SDL3 OgreNext::OgreNext)