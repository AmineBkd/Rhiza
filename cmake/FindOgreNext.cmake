# Locates OGRE-Next in the vcpkg install tree.
# vcpkg's toolchain already appends the triplet dir to CMAKE_PREFIX_PATH,
# so plain find_path/find_library see it without extra hints.
# Creates target: OgreNext::OgreNext

include(SelectLibraryConfigurations)
include(FindPackageHandleStandardArgs)

find_path(OGRENEXT_INCLUDE_DIR
    NAMES OgreRoot.h
    PATH_SUFFIXES OGRE OGRE-Next
)

# OGRE_USE_NEW_PROJECT_NAME=ON in vcpkg's port -> OgreNextMain(Static).
# OgreMain(Static) kept as a fallback in case a future port revision changes this.
find_library(OGRENEXT_LIBRARY_RELEASE
    NAMES OgreNextMain OgreNextMainStatic OgreMain OgreMainStatic
)
find_library(OGRENEXT_LIBRARY_DEBUG
    NAMES OgreNextMain_d OgreNextMainStatic_d OgreMain_d OgreMainStatic_d
)
select_library_configurations(OGRENEXT)

# Each Hlms implementation lives in its own component library (OgreMain does
# not contain them). Anything that #includes Hlms/Unlit/OgreHlmsUnlit.h or
# Hlms/Pbs/OgreHlmsPbs.h must link the matching library too, or every method
# comes back as an unresolved external at link time despite compiling fine.
find_library(OGRENEXT_HLMSUNLIT_LIBRARY_RELEASE NAMES OgreNextHlmsUnlit)
find_library(OGRENEXT_HLMSUNLIT_LIBRARY_DEBUG NAMES OgreNextHlmsUnlit_d)
select_library_configurations(OGRENEXT_HLMSUNLIT)

find_library(OGRENEXT_HLMSPBS_LIBRARY_RELEASE NAMES OgreNextHlmsPbs)
find_library(OGRENEXT_HLMSPBS_LIBRARY_DEBUG NAMES OgreNextHlmsPbs_d)
select_library_configurations(OGRENEXT_HLMSPBS)

# vcpkg installs a single OgreBuildSettings.h, generated from the release build
# and hardcoding OGRE_DEBUG_MODE to OGRE_DEBUG_LEVEL_RELEASE. An app compiled
# against it therefore always identifies as release, and linking the _d
# libraries fails Ogre's generateAbiCookie/testAbiCookie check at startup -
# the plugin is rejected and no render system is found.
#
# Off Windows there is no separate debug C++ runtime, so pointing the debug
# configuration at the release libraries is safe and lets a real -O0 -g build
# of Rhiza itself run. On Windows a debug app links the debug CRT, and mixing
# that with release libraries breaks on _ITERATOR_DEBUG_LEVEL, so the normal
# debug/release pairing is kept there and Debug stays unusable until the port
# ships per-config headers.
if(NOT WIN32)
    set(OGRENEXT_LIBRARY_DEBUG "${OGRENEXT_LIBRARY_RELEASE}")
    set(OGRENEXT_HLMSUNLIT_LIBRARY_DEBUG "${OGRENEXT_HLMSUNLIT_LIBRARY_RELEASE}")
    set(OGRENEXT_HLMSPBS_LIBRARY_DEBUG "${OGRENEXT_HLMSPBS_LIBRARY_RELEASE}")
endif()

find_package_handle_standard_args(OgreNext
    REQUIRED_VARS OGRENEXT_LIBRARY OGRENEXT_INCLUDE_DIR
)

if(OgreNext_FOUND AND NOT TARGET OgreNext::OgreNext)
    add_library(OgreNext::OgreNext UNKNOWN IMPORTED)

    # Ogre-Next's own headers use bare includes (e.g. OgreHlmsUnlit.h does
    # #include "OgreHlmsBufferManager.h", which actually lives in
    # Hlms/Common/, not next to it) on the assumption that every component's
    # include directory is simultaneously on the include path - which is how
    # Ogre-Next's own CMake build sets things up while compiling itself. The
    # vcpkg port installs the headers preserving that same subdirectory
    # layout (Hlms/Common, Hlms/Pbs, Hlms/Unlit, Compositor, Vao, Math/...)
    # but doesn't ship a CMake package that reproduces the flattened include
    # path, so consumers hit "cannot open include file" for headers that are
    # very much present, just one directory over. Rather than hand-list every
    # subdirectory (and re-break the next time a deeper header is pulled in),
    # add every directory that actually contains a header.
    file(GLOB_RECURSE _ogrenext_headers "${OGRENEXT_INCLUDE_DIR}/*.h" "${OGRENEXT_INCLUDE_DIR}/*.inl")
    set(_ogrenext_include_dirs "${OGRENEXT_INCLUDE_DIR}")
    foreach(_ogrenext_header ${_ogrenext_headers})
        get_filename_component(_ogrenext_header_dir "${_ogrenext_header}" DIRECTORY)
        list(APPEND _ogrenext_include_dirs "${_ogrenext_header_dir}")
    endforeach()
    list(REMOVE_DUPLICATES _ogrenext_include_dirs)

    set_target_properties(OgreNext::OgreNext PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${_ogrenext_include_dirs}"
    )
    if(OGRENEXT_LIBRARY_RELEASE)
        set_property(TARGET OgreNext::OgreNext APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
        set_target_properties(OgreNext::OgreNext PROPERTIES IMPORTED_LOCATION_RELEASE "${OGRENEXT_LIBRARY_RELEASE}")
    endif()
    if(OGRENEXT_LIBRARY_DEBUG)
        set_property(TARGET OgreNext::OgreNext APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
        set_target_properties(OgreNext::OgreNext PROPERTIES IMPORTED_LOCATION_DEBUG "${OGRENEXT_LIBRARY_DEBUG}")
    endif()
    if(NOT OGRENEXT_LIBRARY_RELEASE AND NOT OGRENEXT_LIBRARY_DEBUG)
        set_target_properties(OgreNext::OgreNext PROPERTIES IMPORTED_LOCATION "${OGRENEXT_LIBRARY}")
    endif()

    # Must go through the target_link_libraries() command (not a direct
    # set_property on INTERFACE_LINK_LIBRARIES): only the command form
    # understands the debug/optimized keywords that
    # select_library_configurations() produces.
    if(OGRENEXT_HLMSUNLIT_LIBRARY)
        target_link_libraries(OgreNext::OgreNext INTERFACE
            optimized "${OGRENEXT_HLMSUNLIT_LIBRARY_RELEASE}"
            debug "${OGRENEXT_HLMSUNLIT_LIBRARY_DEBUG}"
        )
    endif()

    if(OGRENEXT_HLMSPBS_LIBRARY)
        target_link_libraries(OgreNext::OgreNext INTERFACE
            optimized "${OGRENEXT_HLMSPBS_LIBRARY_RELEASE}"
            debug "${OGRENEXT_HLMSPBS_LIBRARY_DEBUG}"
        )
    endif()

    # Static triplets need OGRE_STATIC_LIB defined (skips dllexport/import
    # decoration) and, once you add RenderSystems, will also need their
    # transitive deps (freetype/freeimage/zziplib/zlib/etc.) linked explicitly.
    if(OGRENEXT_LIBRARY MATCHES "Static")
        target_compile_definitions(OgreNext::OgreNext INTERFACE OGRE_STATIC_LIB)
    endif()

    # This build is NOT static: OgreNextMain is a shared library, and the render
    # system (Direct3D11 / GL3Plus / Metal, depending on platform) is a separate
    # plugin that vcpkg installs - to <root>/bin on Windows, to
    # <root>/lib/OGRE-Next on Unix - but does NOT expose as a linkable CMake
    # target (it's meant to be loaded at runtime via Ogre::Root::loadPlugin(),
    # the same way a classic plugins.cfg would). We derive the triplet's install
    # root here so callers can find that plugin themselves instead of silently
    # failing to locate it; Renderer.cpp turns the root into the full path.
    get_filename_component(OGRENEXT_INSTALL_ROOT "${OGRENEXT_INCLUDE_DIR}" DIRECTORY)
    get_filename_component(OGRENEXT_INSTALL_ROOT "${OGRENEXT_INSTALL_ROOT}" DIRECTORY)

    # On dynamic Unix triplets, libFreeImage.so (an OgreNext dependency) is
    # linked against libraw.so, but vcpkg's libraw port deliberately installs
    # that non-thread-safe variant to lib/manual-link (see
    # third_party/vcpkg/ports/libraw/fix-install.patch) instead of lib/, so
    # it's on neither the linker's nor the runtime loader's search path.
    # MSVC-generator app-local DLL deployment papers over the Windows
    # equivalent of this, which is why the build only breaks here on Linux.
    # Mirror it next to the rest of the triplet's shared libs to fix both.
    if(UNIX)
        foreach(_ogrenext_libdir "${OGRENEXT_INSTALL_ROOT}/lib" "${OGRENEXT_INSTALL_ROOT}/debug/lib")
            file(GLOB _ogrenext_manual_libraw "${_ogrenext_libdir}/manual-link/libraw${CMAKE_SHARED_LIBRARY_SUFFIX}*")
            if(_ogrenext_manual_libraw)
                file(COPY ${_ogrenext_manual_libraw} DESTINATION "${_ogrenext_libdir}")
            endif()
        endforeach()
    endif()
endif()

mark_as_advanced(OGRENEXT_INCLUDE_DIR OGRENEXT_LIBRARY_RELEASE OGRENEXT_LIBRARY_DEBUG
    OGRENEXT_HLMSUNLIT_LIBRARY_RELEASE OGRENEXT_HLMSUNLIT_LIBRARY_DEBUG
    OGRENEXT_HLMSPBS_LIBRARY_RELEASE OGRENEXT_HLMSPBS_LIBRARY_DEBUG)