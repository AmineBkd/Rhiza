cmake_minimum_required(VERSION 3.15)

# Specify your project name here
project(Rhiza VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Uses the project name variable to name the output executable
add_executable(${PROJECT_NAME} src/main.cpp)
