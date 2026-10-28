# Everything links statically into the one libmain.so that SDL's Java side loads.
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)
set(VCPKG_CMAKE_SYSTEM_NAME Android)
set(VCPKG_CMAKE_SYSTEM_VERSION 28)

# zziplib writes its pkg-config file (which Ogre's requires) with bash and sed. A Windows host has
# them only through Git, outside vcpkg's clean PATH; Git's bin/bash.exe sets up its own PATH.
if(PORT STREQUAL "zziplib" AND CMAKE_HOST_WIN32)
    find_path(RHIZA_GIT_ROOT bin/bash.exe REQUIRED NO_DEFAULT_PATH
              PATHS "$ENV{ProgramW6432}/Git" "$ENV{ProgramFiles}/Git" "$ENV{LOCALAPPDATA}/Programs/Git")
    list(APPEND VCPKG_CMAKE_CONFIGURE_OPTIONS "-DBASH=${RHIZA_GIT_ROOT}/bin/bash.exe")
    foreach(tool CP GZIP MV RM)
        string(TOLOWER ${tool} exe)
        list(APPEND VCPKG_CMAKE_CONFIGURE_OPTIONS "-D${tool}=${RHIZA_GIT_ROOT}/usr/bin/${exe}.exe")
    endforeach()
endif()
