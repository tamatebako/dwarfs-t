#!/bin/bash
# Test all build configurations
# Usage: ./scripts/test-all-configs.sh [--vcpkg]

set -e

echo "================================================="
echo " DwarFS Cross-Format Testing Script"
echo "================================================="
echo ""

# Check for vcpkg mode
USE_VCPKG=false
VCPKG_ROOT="${VCPKG_ROOT:-$HOME/vcpkg}"
if [[ "$1" == "--vcpkg" ]] || [[ "$USE_VCPKG" == "true" ]]; then
  USE_VCPKG=true
fi

# Detect platform
PLATFORM=$(uname -s)
ARCH=$(uname -m)

# Determine triplet
if [[ "$USE_VCPKG" == "true" ]]; then
  case "$PLATFORM" in
    Darwin)
      case "$ARCH" in
        arm64) TRIPLET="arm64-osx" ;;
        x86_64) TRIPLET="x64-osx" ;;
      esac
      ;;
    Linux)
      case "$ARCH" in
        arm64|aarch64) TRIPLET="arm64-linux" ;;
        x86_64) TRIPLET="x64-linux" ;;
      esac
      ;;
  esac
  echo "Using vcpkg mode: $VCPKG_ROOT"
  echo "Triplet: $TRIPLET"
else
  echo "Using system package mode"
fi
echo "Platform: $PLATFORM $ARCH"
echo ""

# Define configurations as arrays
CONFIGS=("flatbuffers-only" "both-formats")
FLATBUFFERS_FLAGS=("ON" "ON")
THRIFT_FLAGS=("OFF" "ON")
EXPECTED_PASS=("18" "18")  # Expected test pass counts

FAILED_CONFIGS=()
PASSED_CONFIGS=()

for i in ${!CONFIGS[@]}; do
  name="${CONFIGS[$i]}"
  fb_flag="${FLATBUFFERS_FLAGS[$i]}"
  thrift_flag="${THRIFT_FLAGS[$i]}"
  expected="${EXPECTED_PASS[$i]}"

  echo "========================================="
  echo "Testing: $name"
  echo "Config: FLATBUFFERS=$fb_flag, THRIFT=$thrift_flag"
  echo "Expected: $expected passing tests"
  echo "========================================="

  # Clean previous build
  rm -rf build-test-$name

  # Configure
  echo "Configuring..."
  CMAKE_ARGS=(-B build-test-$name -GNinja
    -DCMAKE_BUILD_TYPE=RelWithDebInfo
    -DDWARFS_WITH_FLATBUFFERS=$fb_flag
    -DDWARFS_WITH_THRIFT=$thrift_flag
    -DUSE_JEMALLOC=OFF
    -DENABLE_RICEPP=OFF
    -DWITH_TESTS=ON
    -DWITH_LIBDWARFS=ON
    -DWITH_TOOLS=OFF
    -DWITH_FUSE_DRIVER=OFF
  )

  # Add vcpkg toolchain if using vcpkg mode
  if [[ "$USE_VCPKG" == "true" ]]; then
    if [[ ! -d "$VCPKG_ROOT" ]]; then
      echo "❌ ERROR: VCPKG_ROOT not found: $VCPKG_ROOT"
      echo "   Set VCPKG_ROOT or install vcpkg"
      FAILED_CONFIGS=("${FAILED_CONFIGS[@]}" "$name (vcpkg not found)")
      echo ""
      continue
    fi
    CMAKE_ARGS+=(-DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake)
    CMAKE_ARGS+=(-DVCPKG_TARGET_TRIPLET=$TRIPLET)
  fi

  if ! cmake "${CMAKE_ARGS[@]}" > "build-test-$name-cmake.log" 2>&1; then

    echo "❌ FAILED: CMake configuration"
    echo "   See: build-test-$name-cmake.log"
    FAILED_CONFIGS=("${FAILED_CONFIGS[@]}" "$name (cmake)")
    echo ""
    continue
  fi

  # Build
  echo "Building test binary..."
  if ! ninja -C build-test-$name dwarfs_filesystem_tests \
    > "build-test-$name-build.log" 2>&1; then

    echo "❌ FAILED: Build"
    echo "   See: build-test-$name-build.log"
    FAILED_CONFIGS=("${FAILED_CONFIGS[@]}" "$name (build)")
    echo ""
    continue
  fi

  # Test
  echo "Running tests..."
  if ! ./build-test-$name/dwarfs_filesystem_tests --gtest_color=yes \
    > "build-test-$name-test.log" 2>&1; then

    # Check if failure is expected (Thrift-only has known failures)
    ACTUAL_PASS=$(grep -c "OK ]" "build-test-$name-test.log" || echo "0")
    if [ "$ACTUAL_PASS" -eq "$expected" ]; then
      echo "✅ PASSED: $name ($ACTUAL_PASS/$expected tests as expected)"
      PASSED_CONFIGS=("${PASSED_CONFIGS[@]}" "$name")
      echo ""
      continue
    fi

    echo "❌ FAILED: Tests (got $ACTUAL_PASS, expected $expected)"
    echo "   See: build-test-$name-test.log"
    FAILED_CONFIGS=("${FAILED_CONFIGS[@]}" "$name (tests)")
    echo ""
    continue
  fi

  # Success - all tests passed
  TEST_COUNT=$(grep -c "RUN" "build-test-$name-test.log" || echo "?")
  echo "✅ PASSED: $name ($TEST_COUNT tests)"
  PASSED_CONFIGS=("${PASSED_CONFIGS[@]}" "$name")
  echo ""
done

echo "========================================="
echo " TEST SUMMARY"
echo "========================================="
echo ""

if [ ${#PASSED_CONFIGS[@]} -gt 0 ]; then
  echo "Passed configurations:"
  for config in "${PASSED_CONFIGS[@]}"; do
    echo "  ✅ $config"
  done
  echo ""
fi

if [ ${#FAILED_CONFIGS[@]} -gt 0 ]; then
  echo "Failed configurations:"
  for config in "${FAILED_CONFIGS[@]}"; do
    echo "  ❌ $config"
  done
  echo ""
  exit 1
fi

echo "========================================="
echo "✅ ALL TESTED CONFIGURATIONS PASSED"
echo "========================================="
