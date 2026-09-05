#!/usr/bin/env bash
set -euo pipefail

# If the project directory has moved or the CMake cache was generated elsewhere,
# CMake will complain about mismatched cache entries.  Remove the build
# directory when that happens to force a clean configuration.
if [ -f build/CMakeCache.txt ]; then
    # extract the recorded source directory from the cache
    old_src=$(grep "CMAKE_HOME_DIRECTORY:INTERNAL=" build/CMakeCache.txt | cut -d'=' -f2- || true)
    if [ -n "$old_src" ] && [ "$old_src" != "$(pwd)" ]; then
        echo "Detected CMake cache from $old_src, removing old build/ to avoid errors"
        rm -rf build
    fi
fi

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j

echo "Built: ./build/zed_apriltag"
