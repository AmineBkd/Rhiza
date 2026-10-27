#!/usr/bin/env bash
set -e

# Runs the last build's tests. Build it first with ./zbuild.sh.
#
#   ./ztest.sh                  -> Debug
#   ./ztest.sh RelWithDebInfo   -> RelWithDebInfo
config="${1:-Debug}"

# Without a build, ctest finds no tests and would call that a pass.
ctest --test-dir build -C "$config" --output-on-failure --no-tests=error
