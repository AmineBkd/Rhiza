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

find_package_handle_standard_args(OgreNext
    REQUIRED_VARS OGRENEXT_LIBRARY OGRENEXT_INCLUDE_DIR
)

if(OgreNext_FOUND AND NOT TARGET OgreNext::OgreNext)
    add_library(OgreNext::OgreNext UNKNOWN IMPORTED)
    set_target_properties(OgreNext::OgreNext PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${OGRENEXT_INCLUDE_DIR}"
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

    # Static triplets need OGRE_STATIC_LIB defined (skips dllexport/import
    # decoration) and, once you add RenderSystems, will also need their
    # transitive deps (freetype/freeimage/zziplib/zlib/etc.) linked explicitly.
    if(OGRENEXT_LIBRARY MATCHES "Static")
        target_compile_definitions(OgreNext::OgreNext INTERFACE OGRE_STATIC_LIB)
    endif()
endif()

mark_as_advanced(OGRENEXT_INCLUDE_DIR OGRENEXT_LIBRARY_RELEASE OGRENEXT_LIBRARY_DEBUG)