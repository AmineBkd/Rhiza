project(RhizaTests LANGUAGES CXX)

# Only links RhizaEngine, never SDL or Ogre: these tests cover the parts of
# the engine that are pure data and need no GPU, window or driver, so they
# run anywhere - including CI containers with no display.
add_executable(ShapesTests ShapesTests.cpp)
target_link_libraries(ShapesTests PRIVATE RhizaEngine)

add_test(NAME ShapesTests COMMAND ShapesTests)

# Input.h/.cpp are engine/src-private (Window is the only public-facing way
# to feed them events), but Input itself has no SDL or Ogre dependency, so
# it's compiled directly into this test rather than linked - same
# no-GPU-no-window guarantee as ShapesTests, just reaching one directory
# further in to get at the class instead of going through a public header.
add_executable(InputTests InputTests.cpp ${CMAKE_SOURCE_DIR}/engine/src/Input.cpp)
target_include_directories(InputTests PRIVATE
    ${CMAKE_SOURCE_DIR}/engine/include
    ${CMAKE_SOURCE_DIR}/engine/src
)

add_test(NAME InputTests COMMAND InputTests)
