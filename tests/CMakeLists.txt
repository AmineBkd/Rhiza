project(RhizaTests LANGUAGES CXX)

find_package(Catch2 3 CONFIG REQUIRED)
include(Catch)

# One executable per layer, linking only that layer, so none of them starts a
# GPU, window or sound card and all of them run in CI containers with no
# display. ctest skips benchmarks, since timings on shared runners are noise:
# run an executable directly to measure.
function(rhiza_add_tests target)
    add_executable(${target} ${ARGN})
    target_link_libraries(${target} PRIVATE Catch2::Catch2WithMain)
    rhiza_enable_warnings(${target})
    catch_discover_tests(${target} EXTRA_ARGS --skip-benchmarks DISCOVERY_MODE PRE_TEST)
endfunction()

rhiza_add_tests(RhizaCoreTests ShapesTests.cpp ClockTests.cpp MathTests.cpp AssetTests.cpp LogTests.cpp)
target_link_libraries(RhizaCoreTests PRIVATE RhizaCore)

rhiza_add_tests(RhizaPlatformTests InputTests.cpp)
target_link_libraries(RhizaPlatformTests PRIVATE RhizaPlatform)

# The demo's music doubles as real-world Ogg Vorbis input.
rhiza_add_tests(RhizaAudioTests AudioTests.cpp)
target_link_libraries(RhizaAudioTests PRIVATE RhizaAudio)
target_compile_definitions(RhizaAudioTests PRIVATE
    RHIZA_TEST_OGG="${CMAKE_CURRENT_SOURCE_DIR}/../game/assets/music.ogg"
)

# Ogre's NULL render system: real Ogre, no GPU or window.
rhiza_add_tests(RhizaRenderTests RenderTests.cpp)
target_link_libraries(RhizaRenderTests PRIVATE RhizaRender)
target_compile_definitions(RhizaRenderTests PRIVATE
    RHIZA_TEST_PNG="${CMAKE_CURRENT_SOURCE_DIR}/../game/assets/ship.png"
)

# The whole engine, headless, in a scene of its own rather than a game's.
rhiza_add_tests(RhizaEngineTests EngineTests.cpp)
target_link_libraries(RhizaEngineTests PRIVATE RhizaEngine)
target_compile_definitions(RhizaEngineTests PRIVATE
    RHIZA_TEST_ASSETS="${CMAKE_CURRENT_SOURCE_DIR}/../game/assets"
)

rhiza_add_tests(RhizaFrameworkTests RegistryTests.cpp)
target_link_libraries(RhizaFrameworkTests PRIVATE RhizaFramework)

rhiza_add_tests(RhizaPhysicsTests PhysicsTests.cpp)
target_link_libraries(RhizaPhysicsTests PRIVATE RhizaPhysics RhizaFramework)
