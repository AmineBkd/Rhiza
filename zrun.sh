#!/usr/bin/env bash
set -e

# Runs the last build of the game. Build it first with ./zbuild.sh.
#
#   ./zrun.sh                  -> Debug
#   ./zrun.sh RelWithDebInfo   -> RelWithDebInfo
config="${1:-Debug}"

# Visual Studio (Git Bash on Windows) builds into bin/<config>/; Makefiles straight into bin/.
for game in "./bin/$config/Game.exe" "./bin/Game"; do
    if [ -x "$game" ]; then
        exec "$game"
    fi
done

echo "zrun.sh: no Game in bin/$config/ or bin/ - run ./zbuild.sh $config first" >&2
exit 1
