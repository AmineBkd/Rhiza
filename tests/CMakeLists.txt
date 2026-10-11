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

rhiza_add_tests(RhizaCoreTests ShapesTests.cpp ClockTests.cpp MathTests.cpp AssetTests.cpp)
target_link_libraries(RhizaCoreTests PRIVATE RhizaCore)

rhiza_add_tests(RhizaPlatformTests InputTests.cpp)
target_link_libraries(RhizaPlatformTests PRIVATE RhizaPlatform)

# The demo's music doubles as real-world Ogg Vorbis input.
rhiza_add_tests(RhizaAudioTests AudioTests.cpp)
target_link_libraries(RhizaAudioTests PRIVATE RhizaAudio)
target_compile_definitions(RhizaAudioTests PRIVATE
    RHIZA_TEST_OGG="${CMAKE_CURRENT_SOURCE_DIR}/../game/assets/music.ogg"
)

rhiza_add_tests(RhizaFrameworkTests RegistryTests.cpp)
target_link_libraries(RhizaFrameworkTests PRIVATE RhizaFramework)
