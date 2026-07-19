#!/usr/bin/env bash
# Build all three metadata serialization configurations and run tests
# Usage: ./scripts/build-all-and-test.sh [--vcpkg]

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$PROJECT_ROOT"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Parse arguments
USE_VCPKG=false
for arg in "$@"; do
  case $arg in
    --vcpkg)
      USE_VCPKG=true
      shift
      ;;
  esac
done

echo "========================================"
echo "DwarFS Build All Configurations"
echo "========================================"
echo ""
echo "This script builds and tests 2 configurations:"
echo "  1. FlatBuffers-only (default)"
echo "  2. Both formats (FlatBuffers + Thrift)"
echo ""

# Configuration
JOBS=${JOBS:-8}
CMAKE_GENERATOR=${CMAKE_GENERATOR:-Ninja}
BUILD_TYPE=${BUILD_TYPE:-Release}

# vcpkg configuration
VCPKG_ROOT=${VCPKG_ROOT:-""}
VCPKG_TRIPLET=${VCPKG_TRIPLET:-""}

if [[ "$USE_VCPKG" == "true" ]]; then
  if [[ -z "$VCPKG_ROOT" ]]; then
    echo -e "${RED}ERROR: --vcpkg specified but VCPKG_ROOT not set${NC}"
    echo "Please set VCPKG_ROOT environment variable"
    exit 1
  fi

  if [[ ! -d "$VCPKG_ROOT" ]]; then
    echo -e "${RED}ERROR: VCPKG_ROOT directory not found: $VCPKG_ROOT${NC}"
    exit 1
  fi

  export CMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"

  # Auto-detect triplet if not set
  if [[ -z "$VCPKG_TRIPLET" ]]; then
    OS=$(uname -s | tr '[:upper:]' '[:lower:]')
    ARCH=$(uname -m)

    # Map architecture names
    case $ARCH in
      x86_64) ARCH="x64" ;;
      aarch64) ARCH="arm64" ;;
      arm64) ARCH="arm64" ;;
    esac

    # Map OS names
    case $OS in
      darwin) OS="osx" ;;
      linux) OS="linux" ;;
      *)
        echo -e "${RED}ERROR: Unsupported OS: $OS${NC}"
        exit 1
        ;;
    esac

    VCPKG_TRIPLET="${ARCH}-${OS}-static"
  fi

  export VCPKG_DEFAULT_TRIPLET="$VCPKG_TRIPLET"

  echo -e "${BLUE}Using vcpkg:${NC}"
  echo "  VCPKG_ROOT: $VCPKG_ROOT"
  echo "  Triplet: $VCPKG_TRIPLET"
  echo "  Toolchain: $CMAKE_TOOLCHAIN_FILE"
  echo
fi

# Build configurations
# Note: DWARFS_WITH_FLATBUFFERS defaults to ON, so we only specify when OFF
CONFIGS=(
  "fb-only:OFF:FlatBuffers-only"
  "both::Both-formats"
)

# Results tracking
declare -A BUILD_RESULTS
declare -A TEST_RESULTS

# Function to build a configuration
# Runs in a subshell to isolate environment
build_config() (
  local name=$1
  local thrift=$2
  local description=$3

  echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
  echo -e "${BLUE}Building: $description${NC}"
  echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"

  local build_dir="build-$name"

  # Clean
  rm -rf "$build_dir"
  mkdir -p "$build_dir"

  # Configure
  echo -e "${YELLOW}Configuring...${NC}"

  # Build CMake command dynamically
  CMAKE_ARGS=(
    -B "$build_dir"
    -G"$CMAKE_GENERATOR"
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
    -DWITH_TESTS=ON
    -DWITH_BENCHMARKS=ON
  )

  # Add vcpkg toolchain if requested
  if [[ -n "${CMAKE_TOOLCHAIN_FILE:-}" ]]; then
    CMAKE_ARGS+=(-DCMAKE_TOOLCHAIN_FILE="$CMAKE_TOOLCHAIN_FILE")
  fi

  if [[ -n "${VCPKG_DEFAULT_TRIPLET:-}" ]]; then
    CMAKE_ARGS+=(-DVCPKG_TARGET_TRIPLET="$VCPKG_DEFAULT_TRIPLET")
  fi

  # Set Thrift ON/OFF as needed
  if [[ -n "$thrift" ]]; then
    CMAKE_ARGS+=(-DDWARFS_WITH_THRIFT="$thrift")
  fi

  # Print cmake command for debugging
  echo "CMake command: cmake ${CMAKE_ARGS[*]}"

  if cmake "${CMAKE_ARGS[@]}"; then
    echo -e "${GREEN}✓ Configuration successful${NC}"
  else
    echo -e "${RED}✗ Configuration failed${NC}"
    return 1
  fi

  # Build
  echo -e "${YELLOW}Building...${NC}"
  if cmake --build "$build_dir" --target mkdwarfs dwarfsck dwarfsextract -j"$JOBS"; then
    echo -e "${GREEN}✓ Build successful${NC}"
  else
    echo -e "${RED}✗ Build failed${NC}"
    return 1
  fi

  # Test
  echo -e "${YELLOW}Testing...${NC}"
  if ctest --test-dir "$build_dir" --output-on-failure -j"$JOBS"; then
    echo -e "${GREEN}✓ Tests passed${NC}"
  else
    echo -e "${RED}✗ Tests failed${NC}"
    return 1
  fi

  return 0
)

# Build all configurations
for config in "${CONFIGS[@]}"; do
  IFS=':' read -r name thrift description <<< "$config"

  # Run build in subshell to isolate environment
  if ( build_config "$name" "$thrift" "$description" ); then
    echo -e "${GREEN}✓ $description: SUCCESS${NC}"
    BUILD_RESULTS[$name]="PASS"
    TEST_RESULTS[$name]="PASS"
  else
    echo -e "${RED}✗ $description: FAILED${NC}"
    BUILD_RESULTS[$name]="FAIL"
    TEST_RESULTS[$name]="FAIL"
  fi
  echo
done

# Summary
echo "========================================"
echo "Build Summary"
echo "========================================"
echo

for config in "${CONFIGS[@]}"; do
  IFS=':' read -r name fb thrift description <<< "$config"

  build_status="${BUILD_RESULTS[$name]:-SKIP}"
  test_status="${TEST_RESULTS[$name]:-SKIP}"

  if [[ "$build_status" == "PASS" ]] && [[ "$test_status" == "PASS" ]]; then
    echo -e "${GREEN}✓ : BUILD=$build_status TEST=$test_status${NC}"
  elif [[ "$build_status" == "FAIL" ]] || [[ "$test_status" == "FAIL" ]]; then
    echo -e "${RED}✗ : BUILD=$build_status TEST=$test_status${NC}"
  else
    echo -e "${YELLOW}⊘ : BUILD=$build_status TEST=$test_status${NC}"
  fi
done

echo
echo "========================================"
echo "Build Artifacts"
echo "========================================"
echo

for config in "${CONFIGS[@]}"; do
  IFS=':' read -r name fb thrift description <<< "$config"

  if [[ "${BUILD_RESULTS[$name]}" == "PASS" ]]; then
    echo -e "${BLUE}($description):${NC}"
    ls -lh "build-$name"/mkdwarfs "build-$name"/dwarfsck "build-$name"/dwarfsextract 2>/dev/null | \
      awk '{printf "  - %s (%s)\n", $9, $5}' || echo "  (not found)"
  fi
done

echo
echo "All builds complete!"
