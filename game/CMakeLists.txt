project(Expansum VERSION 0.1 LANGUAGES CXX)

# Create game executable
add_executable(${PROJECT_NAME}
    src/main.cpp
)

# Link Engine
target_link_libraries(${PROJECT_NAME} PRIVATE 
    RhizaEngine
)