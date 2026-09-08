#!/usr/bin/env bash
set -e

# Runs the last build. Use ./zbuild.sh to configure and build first - keeping
# the two apart means re-running is instant instead of paying ~3s of CMake
# re-checking, and a failed build can never be masked by an old binary.
if [ ! -x ./bin/Expansum ]; then
    echo "zrun.sh: ./bin/Expansum not found - run ./zbuild.sh first" >&2
    exit 1
fi

./bin/Expansum
