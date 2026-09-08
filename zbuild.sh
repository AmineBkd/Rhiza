#!/usr/bin/env bash
set -e

# Debug by default; Release is deliberate.
#
#   ./zbuild.sh            -> Debug
#   ./zbuild.sh Release    -> Release
config="${1:-Debug}"

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
