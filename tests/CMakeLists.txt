project(RhizaTests LANGUAGES CXX)

# Only links RhizaEngine, never SDL or Ogre: these tests cover the parts of
# the engine that are pure data and need no GPU, window or driver, so they
# run anywhere - including CI containers with no display.
add_executable(ShapesTests ShapesTests.cpp)
target_link_libraries(ShapesTests PRIVATE RhizaEngine)

add_test(NAME ShapesTests COMMAND ShapesTests)

# platform/Input.h is engine-private (Window is the only public-facing way
# to feed them events), but Input itself has no SDL or Ogre dependency, so
# it's compiled directly into this test rather than linked - same
# no-GPU-no-window guarantee as ShapesTests, just reaching one directory
# further in to get at the class instead of going through a public header.
add_executable(InputTests InputTests.cpp ${CMAKE_SOURCE_DIR}/engine/src/platform/Input.cpp)
target_include_directories(InputTests PRIVATE
    ${CMAKE_SOURCE_DIR}/engine/include
    ${CMAKE_SOURCE_DIR}/engine/src
)

add_test(NAME InputTests COMMAND InputTests)

# Same reasoning as InputTests: Clock has no SDL or Ogre dependency (just
# <chrono>), so it's compiled directly rather than linked.
add_executable(ClockTests ClockTests.cpp ${CMAKE_SOURCE_DIR}/engine/src/core/Clock.cpp)
target_include_directories(ClockTests PRIVATE
    ${CMAKE_SOURCE_DIR}/engine/include
    ${CMAKE_SOURCE_DIR}/engine/src
)

add_test(NAME ClockTests COMMAND ClockTests)

# RhizaFramework is header-only and has no SDL/Ogre dependency either, so
# this runs anywhere too - just a link (for the include path) rather than
# compiling an engine/src file directly, since there's no .cpp to compile.
add_executable(RegistryTests RegistryTests.cpp)
target_link_libraries(RegistryTests PRIVATE RhizaFramework)

add_test(NAME RegistryTests COMMAND RegistryTests)
