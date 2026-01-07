#!/bin/bash

# DwarFS Complete Clean-Build Script
# Deletes all build directories, reconfigures, and builds from scratch

set -e

PROJECT_ROOT="$(cd "$(dirname "$0")/.." && pwd)"

# Build directory configuration
BUILD_DIR="${BUILD_DIR:-$PROJECT_ROOT/build}"
BUILD_TYPE="${BUILD_TYPE:-Release}"
USE_VCPKG="${USE_VCPKG:-true}"
VCPKG_ROOT="${VCPKG_ROOT:-/Users/mulgogi/vcpkg}"

# Parse command-line options
SKIP_CONFIRM=false
RUN_NINJA=true
while getopts "yn" opt; do
    case $opt in
        y)
            SKIP_CONFIRM=true
            ;;
        n)
            RUN_NINJA=false
            ;;
        \?)
            echo "Usage: $0 [-y] [-n]"
            echo "  -y: Skip confirmation prompt"
            echo "  -n: Skip ninja build (only configure)"
            echo ""
            echo "Environment variables:"
            echo "  BUILD_DIR    - Build directory (default: build)"
            echo "  USE_VCPKG    - Use vcpkg toolchain (default: true)"
            echo "  VCPKG_ROOT   - Vcpkg installation (default: /Users/mulgogi/vcpkg)"
            echo "  BUILD_TYPE   - CMake build type (default: Release)"
            echo "  WITH_TESTS   - Enable tests (default: ON)"
            echo "  WITH_TOOLS   - Enable tools (default: ON)"
            exit 1
            ;;
    esac
done

echo "=== DwarFS Complete Clean-Build ==="
echo "Project root: $PROJECT_ROOT"
echo "Build directory: $BUILD_DIR"
echo "Use vcpkg: $USE_VCPKG"
echo

# List all potential build directories to clean
BUILD_DIRS=(
    "$PROJECT_ROOT/build"
    "$PROJECT_ROOT/build-test"
    "$PROJECT_ROOT/build-vcpkg"
)

# Confirm action
if [ "$SKIP_CONFIRM" = false ]; then
    echo "This will DELETE the following build directories:"
    for dir in "${BUILD_DIRS[@]}"; do
        if [ -d "$dir" ]; then
            echo "  - $dir (exists)"
        else
            echo "  - $dir (not present)"
        fi
    done
    echo ""
    read -p "Continue with clean build? (y/N) " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        echo "Aborted."
        exit 1
    fi
fi

# Remove all build directories
echo "=== Removing Build Directories ==="
for dir in "${BUILD_DIRS[@]}"; do
    if [ -d "$dir" ]; then
        echo "Removing: $dir"
        rm -rf "$dir"
    fi
done
echo

# Create fresh build directory
echo "Creating fresh build directory: $BUILD_DIR"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Build configuration
WITH_TESTS="${WITH_TESTS:-ON}"
WITH_TOOLS="${WITH_TOOLS:-ON}"
WITH_LIBDWARFS="${WITH_LIBDWARFS:-ON}"
WITH_FLATBUFFERS="${WITH_FLATBUFFERS:-ON}"
WITH_THRIFT="${WITH_THRIFT:-OFF}"

echo
echo "=== CMake Configuration ==="
echo "  BUILD_TYPE:     $BUILD_TYPE"
echo "  WITH_TESTS:     $WITH_TESTS"
echo "  WITH_TOOLS:     $WITH_TOOLS"
echo "  WITH_LIBDWARFS: $WITH_LIBDWARFS"
echo "  WITH_FLATBUFFERS: $WITH_FLATBUFFERS"
echo "  WITH_THRIFT:    $WITH_THRIFT"
echo "  USE_VCPKG:      $USE_VCPKG"
echo

# Run CMake
if [ "$USE_VCPKG" = true ]; then
    echo "Configuring with vcpkg toolchain..."
    CMAKE_PREFIX_PATH="$BUILD_DIR/vcpkg_installed/arm64-osx-static"
    export CMAKE_PREFIX_PATH
    NO_CMAKE_PATH=1 NO_CMAKE_ENVIRONMENT_PATH=1 cmake "$PROJECT_ROOT" -GNinja \
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
    echo "Configuring with system packages..."
    cmake "$PROJECT_ROOT" -GNinja \
        -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
        -DWITH_TESTS="$WITH_TESTS" \
        -DWITH_TOOLS="$WITH_TOOLS" \
        -DWITH_LIBDWARFS="$WITH_LIBDWARFS" \
        -DDWARFS_WITH_FLATBUFFERS="$WITH_FLATBUFFERS" \
        -DDWARFS_WITH_THRIFT="$WITH_THRIFT"
fi

echo
echo "=== Configuration Complete ==="

# Run ninja if requested
if [ "$RUN_NINJA" = true ]; then
    echo
    echo "=== Running Ninja Build ==="
    echo "Note: If you see 'manifest dirty' errors, try: touch $BUILD_DIR/build.ninja"
    echo
    ninja -j$(sysctl -n hw.ncpu)
    echo
    echo "=== Build Complete ==="
    echo "Binary locations:"
    find "$BUILD_DIR" -maxdepth 3 -type f -executable -name "dwarfs" -o -name "mkdwarfs" -o -name "dwarfsck" -o -name "dwarfsextract" 2>/dev/null | head -10
else
    echo
    echo "To build, run:"
    echo "  cd $BUILD_DIR"
    echo "  ninja -j\$(sysctl -n hw.ncpu)"
fi

echo
