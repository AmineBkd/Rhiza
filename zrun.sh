#!/usr/bin/env bash
set -e

# Runs the last build of the game. Build it first with ./zbuild.sh.
#
#   ./zrun.sh                  -> Debug
#   ./zrun.sh RelWithDebInfo   -> RelWithDebInfo
config="${1:-Debug}"

# Game.exe from Git Bash on Windows, Game everywhere else.
for game in "./bin/$config/Game.exe" "./bin/$config/Game"; do
    if [ -x "$game" ]; then
        exec "$game"
    fi
done

echo "zrun.sh: no Game in bin/$config/ - run ./zbuild.sh $config first" >&2
exit 1
