#!/bin/bash
#
# Multi-Triplet BZip2 Validation Script
# Tests BZip2 dependency resolution on native platform triplets
#

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo "═══════════════════════════════════════════════════════"
echo "  DwarFS vcpkg BZip2 Validation (Native Triplets)"
echo "═══════════════════════════════════════════════════════"
echo ""

# Check prerequisites
if [ -z "$VCPKG_ROOT" ]; then
  echo -e "${RED}ERROR: VCPKG_ROOT not set${NC}"
  echo "Set it with: export VCPKG_ROOT=/path/to/vcpkg"
  exit 1
fi

if [ ! -f "$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" ]; then
  echo -e "${RED}ERROR: vcpkg toolchain not found${NC}"
  exit 1
fi

# Overlay ports (optional but recommended)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
OVERLAY_PORTS="$PROJECT_DIR/vcpkg_ports"

if [ -d "$OVERLAY_PORTS" ]; then
  echo -e "${BLUE}Using overlay ports: $OVERLAY_PORTS${NC}"
  OVERLAY_ARG="-DVCPKG_OVERLAY_PORTS=$OVERLAY_PORTS"
else
  echo -e "${YELLOW}Warning: No overlay ports found${NC}"
  OVERLAY_ARG=""
fi
echo ""

# Detect host platform
HOST_OS=$(uname -s)
HOST_ARCH=$(uname -m)

# Determine native triplets based on host
TRIPLETS=()
case "$HOST_OS" in
  Darwin)
    echo "Detected: macOS ($HOST_ARCH)"
    if [ "$HOST_ARCH" = "arm64" ]; then
      TRIPLETS=("arm64-osx" "arm64-osx-dynamic")
    else
      TRIPLETS=("x64-osx" "x64-osx-dynamic")
    fi
    ;;
  Linux)
    echo "Detected: Linux ($HOST_ARCH)"
    if [ "$HOST_ARCH" = "aarch64" ]; then
      TRIPLETS=("arm64-linux" "arm64-linux-dynamic")
    else
      TRIPLETS=("x64-linux" "x64-linux-dynamic")
    fi
    ;;
  MINGW*|MSYS*|CYGWIN*)
    echo "Detected: Windows ($HOST_ARCH)"
    if [ "$HOST_ARCH" = "aarch64" ] || [ "$HOST_ARCH" = "arm64" ]; then
      TRIPLETS=("arm64-windows" "arm64-windows-static")
    else
      TRIPLETS=("x64-windows" "x64-windows-static")
    fi
    ;;
  *)
    echo -e "${RED}ERROR: Unsupported platform: $HOST_OS${NC}"
    exit 1
    ;;
esac

echo "Testing ${#TRIPLETS[@]} native triplets for $HOST_OS $HOST_ARCH"
echo ""

# Counters
TESTED=0
PASSED=0
FAILED=0

# Results file
RESULTS_FILE="/tmp/triplet-results-$$.txt"
rm -f "$RESULTS_FILE"

for triplet in "${TRIPLETS[@]}"; do
  ((TESTED++))

  echo -e "${YELLOW}[$TESTED/${#TRIPLETS[@]}] Testing triplet: $triplet${NC}"

  BUILD_DIR="build-test-$triplet"
  rm -rf "$BUILD_DIR"

  echo "  Configuring..."
  if cmake -B "$BUILD_DIR" -GNinja \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" \
      -DVCPKG_TARGET_TRIPLET="$triplet" \
      $OVERLAY_ARG \
      -DDWARFS_WITH_FLATBUFFERS=ON \
      -DDWARFS_WITH_THRIFT=OFF \
      -DWITH_TESTS=ON \
      > "$BUILD_DIR-config.log" 2>&1; then

    # Check BZip2  found
    if grep -q "Found BZip2" "$BUILD_DIR/CMakeCache.txt" 2>/dev/null; then
      BZIP2_VERSION=$(grep "BZIP2_VERSION_STRING" "$BUILD_DIR/CMakeCache.txt" | cut -d= -f2)
      echo -e "  ${GREEN}✓ BZip2 found: $BZIP2_VERSION${NC}"

      # Verify Boost uses BZip2
      if grep -q "Boost_IOSTREAMS_FOUND" "$BUILD_DIR/CMakeCache.txt" 2>/dev/null; then
        echo -e "  ${GREEN}✓ Boost iostreams found${NC}"
      fi

      # Try to build
      echo "  Building dwarfs_common..."
      if ninja -C "$BUILD_DIR" dwarfs_common > "$BUILD_DIR-build.log" 2>&1; then
        echo -e "  ${GREEN}✓ Build succeeded${NC}"
        ((PASSED++))
        echo "$triplet|PASSED|BZip2 $BZIP2_VERSION" >> "$RESULTS_FILE"
      else
        echo -e "  ${RED}✗ Build failed${NC}"
        tail -10 "$BUILD_DIR-build.log"
        ((FAILED++))
        echo "$triplet|FAILED|Build error" >> "$RESULTS_FILE"
      fi
    else
      echo -e "  ${RED}✗ BZip2 NOT found${NC}"
      grep -i "bzip" "$BUILD_DIR-config.log" | tail -5
      ((FAILED++))
      echo "$triplet|FAILED|BZip2 not found" >> "$RESULTS_FILE"
    fi
  else
    echo -e "  ${RED}✗ Configuration failed${NC}"
    tail -20 "$BUILD_DIR-config.log"
    ((FAILED++))
    echo "$triplet|FAILED|Config error" >> "$RESULTS_FILE"
  fi

  echo ""
done

# Summary
echo "═══════════════════════════════════════════════════════"
echo "  SUMMARY"
echo "═══════════════════════════════════════════════════════"
echo ""
echo "Platform: $HOST_OS $HOST_ARCH"
echo "Tested: $TESTED triplets"
echo -e "${GREEN}Passed: $PASSED${NC}"
if [ $FAILED -gt 0 ]; then
  echo -e "${RED}Failed: $FAILED${NC}"
else
  echo "Failed: $FAILED"
fi
echo ""

# Detailed results
echo "Detailed Results:"
echo "─────────────────────────────────────────────────────"
if [ -f "$RESULTS_FILE" ]; then
  while IFS='|' read -r triplet status details; do
    case "$status" in
      PASSED*)
        echo -e "$triplet: ${GREEN}$status${NC} ($details)"
        ;;
      FAILED*)
        echo -e "$triplet: ${RED}$status${NC} ($details)"
        ;;
      *)
        echo "$triplet: $status ($details)"
        ;;
    esac
  done < "$RESULTS_FILE"
fi
echo ""

# Cleanup
rm -f "$RESULTS_FILE"

# Exit code
if [ $FAILED -gt 0 ]; then
  echo -e "${RED}❌ Some triplets FAILED${NC}"
  echo ""
  echo "To debug, check logs in:"
  echo "  build-test-*-config.log"
  echo "  build-test-*-build.log"
  exit 1
else
  echo -e "${GREEN}✅ All tested triplets PASSED${NC}"
  echo ""
  echo "BZip2 fix validated on native platform triplets ✓"
  exit 0
fi