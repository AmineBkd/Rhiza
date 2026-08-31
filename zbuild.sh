#!/usr/bin/env bash
set -e

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

cmake --preset "$preset"
cmake --build --preset "$preset"
