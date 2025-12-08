#!/bin/bash

# DwarFS Clean Build Script
# Safely removes build directory and reconfigures with current Homebrew paths

set -e

PROJECT_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$PROJECT_ROOT/build-test}"

# Parse command-line options
SKIP_CONFIRM=false
while getopts "y" opt; do
    case $opt in
        y)
            SKIP_CONFIRM=true
            ;;
        \?)
            echo "Usage: $0 [-y]"
            echo "  -y: Skip confirmation prompt"
            exit 1
            ;;
    esac
done

echo "=== DwarFS Clean Build Script ==="
echo "Project root: $PROJECT_ROOT"
echo "Build directory: $BUILD_DIR"
echo

# Confirm action
if [ -d "$BUILD_DIR" ]; then
    echo "Warning: This will delete the existing build directory:"
    echo "  $BUILD_DIR"

    if [ "$SKIP_CONFIRM" = false ]; then
        read -p "Continue? (y/N) " -n 1 -r
        echo
        if [[ ! $REPLY =~ ^[Yy]$ ]]; then
            echo "Aborted."
            exit 1
        fi
    else
        echo "Proceeding with -y flag..."
    fi

    echo "Removing old build directory..."
    rm -rf "$BUILD_DIR"
fi

# Create fresh build directory
echo "Creating fresh build directory..."
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Default build configuration
BUILD_TYPE="${BUILD_TYPE:-Release}"
WITH_TESTS="${WITH_TESTS:-ON}"
WITH_TOOLS="${WITH_TOOLS:-ON}"
WITH_LIBDWARFS="${WITH_LIBDWARFS:-ON}"
WITH_FLATBUFFERS="${WITH_FLATBUFFERS:-ON}"
WITH_THRIFT="${WITH_THRIFT:-OFF}"

echo
echo "=== CMake Configuration ==="
echo "  BUILD_TYPE:        $BUILD_TYPE"
echo "  WITH_TESTS:        $WITH_TESTS"
echo "  WITH_TOOLS:        $WITH_TOOLS"
echo "  WITH_LIBDWARFS:    $WITH_LIBDWARFS"
echo "  WITH_FLATBUFFERS:  $WITH_FLATBUFFERS"
echo "  WITH_THRIFT:       $WITH_THRIFT"
echo

# Run CMake
cmake .. -GNinja \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DWITH_TESTS="$WITH_TESTS" \
    -DWITH_TOOLS="$WITH_TOOLS" \
    -DWITH_LIBDWARFS="$WITH_LIBDWARFS" \
    -DDWARFS_WITH_FLATBUFFERS="$WITH_FLATBUFFERS" \
    -DDWARFS_WITH_THRIFT="$WITH_THRIFT"

echo
echo "=== Configuration Complete ==="
echo "To build, run:"
echo "  cd $BUILD_DIR"
echo "  ninja"
echo
echo "To run tests, run:"
echo "  cd $BUILD_DIR"
echo "  ctest -R metadata"