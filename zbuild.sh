#!/usr/bin/env bash
set -e

# RelWithDebInfo by default: -O2 -g, and it works on every platform.
#
#   ./zbuild.sh              -> RelWithDebInfo  (-O2 -g)
#   ./zbuild.sh Debug        -> -g, no optimisation. Linux and macOS only;
#                               still fails Ogre's ABI check on Windows.
#   ./zbuild.sh Release      -> -O3
config="${1:-RelWithDebInfo}"

case "$(uname -s)" in
    Linux*)
        preset=linux
        ;;
    Darwin*)
        if [ "$(uname -m)" = "arm64" ]; then
            preset=macos-arm64
        else
            preset=macos-x64
        fi
        ;;
    MINGW*|MSYS*|CYGWIN*)
        preset=windows
        ;;
    *)
        echo "zbuild.sh: unrecognized platform '$(uname -s)', see CMakePresets.json for available presets" >&2
        exit 1
        ;;
esac

# CMAKE_BUILD_TYPE is what single-config generators (Makefiles, on Linux and
# macOS) read at configure time; --config is what multi-config generators
# (Visual Studio) read at build time. Each ignores the other, so one pair of
# lines covers all three platforms. Passing neither leaves the build type
# empty, which is not a middle ground - it means no -O2 and no -g at once.
cmake --preset "$preset" -DCMAKE_BUILD_TYPE="$config"
cmake --build --preset "$preset" --config "$config"

echo "zbuild.sh: built $config"
