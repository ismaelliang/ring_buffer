#!/bin/bash

# Build script for ringbuffer project with Google Test integration via vcpkg manifest mode

set -e  # Exit on any error

echo "=== Building RingBuffer Project with Google Test ==="

# Create build directory
mkdir -p build
cd build

# Configure with CMake (vcpkg will automatically install dependencies from vcpkg.json)
echo "Configuring project with CMake..."
if [ -n "$VCPKG_ROOT" ] && [ -f "$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" ]; then
    echo "Using vcpkg toolchain: $VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
    cmake .. -DCMAKE_BUILD_TYPE=Debug -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
else
    echo "Warning: vcpkg toolchain not found, using system packages..."
    cmake .. -DCMAKE_BUILD_TYPE=Debug
fi

# Build the project
echo "Building project..."
cmake --build .

# Run tests
echo "Running tests..."
ctest --output-on-failure

echo "=== Build and Test Complete ==="
