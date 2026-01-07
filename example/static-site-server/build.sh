#!/bin/bash
# Build script for static-site-server example
#
# Prerequisites:
#   - vcpkg installed and VCPKG_ROOT set
#   - Run from within dwarfs repository for overlay ports
#
# Usage:
#   ./build.sh           # Build normally (incremental)
#   ./build.sh --clean   # Delete build artifacts (no build)
#   ./build.sh --rebuild # Clean then build

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CLEAN_ONLY=false
REBUILD=false

# Parse arguments
while [[ $# -gt 0 ]]; do
  case $1 in
    --help|-h)
      echo "Static Site Server Build Script"
      echo ""
      echo "Usage: $0 [OPTIONS]"
      echo ""
      echo "Options:"
      echo "  --help, -h    Show this help message"
      echo "  --clean       Delete build artifacts (no build)"
      echo "  --rebuild     Clean then build from scratch"
      echo ""
      echo "Prerequisites:"
      echo "  - vcpkg installed and VCPKG_ROOT environment variable set"
      echo "  - Run from example/static-site-server/ within dwarfs repository"
      echo ""
      echo "How it works:"
      echo "  - Uses vcpkg to install dwarfs from overlay port (vcpkg_ports/dwarfs)"
      echo "  - The dwarfs port builds from local source (../../)"
      echo "  - Jemalloc 5.5.0 is fetched from tamatebako/jemalloc via overlay port"
      echo "  - All boost and other dependencies come from vcpkg registry"
      echo ""
      echo "Examples:"
      echo "  $0              # Normal incremental build"
      echo "  $0 --clean      # Delete build files only (no build)"
      echo "  $0 --rebuild    # Clean then build from scratch"
      exit 0
      ;;
    --clean)
      CLEAN_ONLY=true
      shift
      ;;
    --rebuild)
      REBUILD=true
      shift
      ;;
    *)
      echo "Unknown option: $1"
      echo "Usage: $0 [--help] [--clean] [--rebuild]"
      exit 1
      ;;
  esac
done

# Clean artifacts
if [ "$CLEAN_ONLY" = true ] || [ "$REBUILD" = true ]; then
  echo "Cleaning build artifacts..."
  rm -rf "${SCRIPT_DIR}/build" "${SCRIPT_DIR}/vcpkg_installed"
  rm -f "${SCRIPT_DIR}/CMakeCache.txt"
  rm -rf "${SCRIPT_DIR}/CMakeFiles"
  echo "✓ Cleaned: build/, vcpkg_installed/, CMake cache"

  # Exit if --clean (don't build)
  if [ "$CLEAN_ONLY" = true ]; then
    exit 0
  fi
  echo ""
fi

echo "=== Static Site Server Build ==="
echo ""

# Check vcpkg
if [ -z "${VCPKG_ROOT}" ] || [ ! -x "${VCPKG_ROOT}/vcpkg" ]; then
  echo "ERROR: vcpkg not found at VCPKG_ROOT: ${VCPKG_ROOT}"
  echo "Please install vcpkg and set VCPKG_ROOT environment variable"
  exit 1
fi

# Determine triplet based on platform
if [ "$(uname)" = "Darwin" ]; then
  if [ "$(uname -m)" = "arm64" ]; then
    TRIPLET="arm64-osx-static"
  else
    TRIPLET="x64-osx-static"
  fi
elif [ "$(uname)" = "Linux" ]; then
  if [ "$(uname -m)" = "x86_64" ]; then
    TRIPLET="x64-linux-static"
  else
    TRIPLET="arm64-linux-static"
  fi
else
  TRIPLET="x64-windows-static"
fi

echo "Using vcpkg from: ${VCPKG_ROOT}"
echo "Using triplet: ${TRIPLET}"
echo ""

# Build configuration
BUILD_DIR="${SCRIPT_DIR}/build"

echo "Configuring..."
cmake -B "${BUILD_DIR}" \
  -S "${SCRIPT_DIR}" \
  -GNinja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE="${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" \
  -DVCPKG_TARGET_TRIPLET="${TRIPLET}"

echo ""
echo "Building..."
cmake --build "${BUILD_DIR}" --parallel

echo ""
echo "=== Build Complete ==="
echo ""
echo "Run server:"
echo "  cd ${SCRIPT_DIR}"
echo "  ./build/static-site-server --image candide.dff"
echo ""
echo "Or test with:"
echo "  ./test.sh"