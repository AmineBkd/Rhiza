#!/usr/bin/env bash
set -e

# Runs the last build's tests. Use ./zbuild.sh with the same config first.
#
#   ./ztest.sh              -> RelWithDebInfo
#   ./ztest.sh Release      -> Release
config="${1:-RelWithDebInfo}"

# Without a build, ctest finds no tests and would call that a pass.
ctest --test-dir build -C "$config" --output-on-failure --no-tests=error
