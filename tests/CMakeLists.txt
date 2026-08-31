project(RhizaTests LANGUAGES CXX)

# Only links RhizaEngine, never SDL or Ogre: these tests cover the parts of
# the engine that are pure data and need no GPU, window or driver, so they
# run anywhere - including CI containers with no display.
add_executable(ShapesTests ShapesTests.cpp)
target_link_libraries(ShapesTests PRIVATE RhizaEngine)

add_test(NAME ShapesTests COMMAND ShapesTests)
