#!/usr/bin/env bash
set -euo pipefail
brew install cmake ninja qt@6
export CMAKE_PREFIX_PATH="$(brew --prefix qt@6)"
cmake --preset default
cmake --build --preset default
ctest --preset default
echo "Run: open build/default/apps/desktop/MyDesk.app"
