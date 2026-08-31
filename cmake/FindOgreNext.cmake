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

# HlmsUnlit lives in its own component library (OgreMain does not contain
# it), same as HlmsPbs would if we needed it. Anything that #includes
# Hlms/Unlit/OgreHlmsUnlit.h must link this too, or every HlmsUnlit method
# comes back as an unresolved external at link time despite compiling fine.
find_library(OGRENEXT_HLMSUNLIT_LIBRARY_RELEASE NAMES OgreNextHlmsUnlit)
find_library(OGRENEXT_HLMSUNLIT_LIBRARY_DEBUG NAMES OgreNextHlmsUnlit_d)
select_library_configurations(OGRENEXT_HLMSUNLIT)

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

    if(OGRENEXT_HLMSUNLIT_LIBRARY)
        # Must go through the target_link_libraries() command (not a direct
        # set_property on INTERFACE_LINK_LIBRARIES): only the command form
        # understands the debug/optimized keywords that
        # select_library_configurations() produces.
        target_link_libraries(OgreNext::OgreNext INTERFACE
            optimized "${OGRENEXT_HLMSUNLIT_LIBRARY_RELEASE}"
            debug "${OGRENEXT_HLMSUNLIT_LIBRARY_DEBUG}"
        )
    endif()

    # Static triplets need OGRE_STATIC_LIB defined (skips dllexport/import
    # decoration) and, once you add RenderSystems, will also need their
    # transitive deps (freetype/freeimage/zziplib/zlib/etc.) linked explicitly.
    if(OGRENEXT_LIBRARY MATCHES "Static")
        target_compile_definitions(OgreNext::OgreNext INTERFACE OGRE_STATIC_LIB)
    endif()

    # This build is NOT static: OgreNextMain is a DLL, and RenderSystem_Direct3D11
    # is a separate plugin DLL that vcpkg installs to <root>/bin (and <root>/debug/bin)
    # but does NOT expose as a linkable CMake target (it's meant to be loaded at
    # runtime via Ogre::Root::loadPlugin(), the same way a classic plugins.cfg would).
    # We derive the triplet's install root here so callers can find that DLL
    # themselves instead of silently failing to locate it.
    get_filename_component(OGRENEXT_INSTALL_ROOT "${OGRENEXT_INCLUDE_DIR}" DIRECTORY)
    get_filename_component(OGRENEXT_INSTALL_ROOT "${OGRENEXT_INSTALL_ROOT}" DIRECTORY)
endif()

mark_as_advanced(OGRENEXT_INCLUDE_DIR OGRENEXT_LIBRARY_RELEASE OGRENEXT_LIBRARY_DEBUG
    OGRENEXT_HLMSUNLIT_LIBRARY_RELEASE OGRENEXT_HLMSUNLIT_LIBRARY_DEBUG)