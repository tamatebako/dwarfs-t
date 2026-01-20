#!/bin/bash

# DwarFS Clean Build Script
# Safely removes build directory and reconfigures with current Homebrew paths

set -e

PROJECT_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$PROJECT_ROOT/build-test}"

# Vcpkg configuration
VCPKG_ROOT="${VCPKG_ROOT:-/Users/mulgogi/vcpkg}"
USE_VCPKG="${USE_VCPKG:-false}"

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
            echo ""
            echo "Environment variables:"
            echo "  BUILD_DIR    - Build directory (default: build-test)"
            echo "  USE_VCPKG    - Use vcpkg toolchain (default: false)"
            echo "  VCPKG_ROOT   - Vcpkg installation (default: /Users/mulgogi/vcpkg)"
            echo "  WITH_THRIFT  - Enable Modern Thrift (default: OFF)"
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
echo "  USE_VCPKG:         $USE_VCPKG"
echo

# Run CMake
if [ "$USE_VCPKG" = true ]; then
    echo "Using vcpkg toolchain with overlay ports..."
    # Set CMAKE_PREFIX_PATH to prioritize vcpkg packages from this build
    # This ensures vcpkg overlay ports (folly, fbthrift, etc.) are found before Homebrew versions
    # The packages are installed to $BUILD_DIR/vcpkg_installed/$VCPKG_TARGET_TRIPLET
    CMAKE_PREFIX_PATH="$BUILD_DIR/vcpkg_installed/$VCPKG_TARGET_TRIPLET"
    export CMAKE_PREFIX_PATH
    NO_CMAKE_PATH=1 NO_CMAKE_ENVIRONMENT_PATH=1 cmake .. -GNinja \
        -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
        -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" \
        -DCMAKE_PREFIX_PATH="$CMAKE_PREFIX_PATH" \
        -DVCPKG_OVERLAY_PORTS="$PROJECT_ROOT/vcpkg_ports" \
        -DVCPKG_OVERLAY_TRIPLETS="$PROJECT_ROOT/vcpkg_triplets" \
        -DVCPKG_TARGET_TRIPLET=arm64-osx-static \
        -DWITH_TESTS="$WITH_TESTS" \
        -DWITH_TOOLS="$WITH_TOOLS" \
        -DWITH_LIBDWARFS="$WITH_LIBDWARFS" \
        -DDWARFS_WITH_FLATBUFFERS="$WITH_FLATBUFFERS" \
        -DDWARFS_WITH_THRIFT="$WITH_THRIFT"
else
    echo "Using system packages (pkg-config)..."
    cmake .. -GNinja \
        -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
        -DWITH_TESTS="$WITH_TESTS" \
        -DWITH_TOOLS="$WITH_TOOLS" \
        -DWITH_LIBDWARFS="$WITH_LIBDWARFS" \
        -DDWARFS_WITH_FLATBUFFERS="$WITH_FLATBUFFERS" \
        -DDWARFS_WITH_THRIFT="$WITH_THRIFT"
fi

echo
echo "=== Configuration Complete ==="
echo "To build, run:"
echo "  cd $BUILD_DIR"
echo "  ninja"
echo
echo "To run tests, run:"
echo "  cd $BUILD_DIR"
echo "  ctest -R metadata"