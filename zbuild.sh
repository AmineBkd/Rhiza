#!/usr/bin/env bash
set -e

# Configures and builds.
#
#   ./zbuild.sh                  -> Debug
#   ./zbuild.sh RelWithDebInfo   -> optimised, with symbols
#   ./zbuild.sh Release          -> optimised
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

# Makefiles read the config at configure time, Visual Studio at build time; each ignores the other.
cmake --preset "$preset" -DCMAKE_BUILD_TYPE="$config"
cmake --build --preset "$preset" --config "$config"

echo "zbuild.sh: built $config"
