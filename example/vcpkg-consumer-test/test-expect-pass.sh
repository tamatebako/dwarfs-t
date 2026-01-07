#!/bin/bash
# test-expect-pass.sh - Verify test passes with complete header installation (AFTER fix)
#
# This script tests that our header test PASSES when all headers are correctly installed.
# It uses the FIXED header installation rules.
#
# Expected Result: BUILD SUCCESS + test executable runs
# This proves the fix works correctly.

set -e  # Exit on error

echo "=================================================="
echo "DwarFS Header Test - EXPECT PASS"
echo "=================================================="
echo ""
echo "This test verifies that our header test correctly"
echo "PASSES when all headers are properly installed."
echo ""
echo "Expected result: BUILD SUCCESS ✅"
echo "=================================================="
echo ""

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build-expect-pass"
INSTALL_DIR="/tmp/dwarfs-fixed-install"

echo "Configuration:"
echo "  Project Root: ${PROJECT_ROOT}"
echo "  Build Dir: ${BUILD_DIR}"
echo "  Install Dir: ${INSTALL_DIR}"
echo ""

# Step 1: Build DwarFS with FIXED header installation
echo "Step 1: Building DwarFS with FIXED header installation..."
cd "${PROJECT_ROOT}"

mkdir -p build-fixed && cd build-fixed
cmake .. \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="${INSTALL_DIR}" \
  -DWITH_LIBDWARFS=ON \
  -DWITH_TOOLS=OFF \
  -DWITH_FUSE_DRIVER=OFF \
  -DWITH_TESTS=OFF \
  -DDWARFS_WITH_FLATBUFFERS=ON \
  -DDWARFS_WITH_THRIFT=OFF \
  -GNinja \
  > /dev/null 2>&1

ninja > /dev/null 2>&1
ninja install > /dev/null 2>&1

cd "${PROJECT_ROOT}"
rm -rf build-fixed

echo -e "${GREEN}✓${NC} Built with fixed installation (all headers included)"
echo ""

# Step 2: Verify critical headers are PRESENT
echo "Step 2: Verifying that critical headers are PRESENT..."
PRESENT_COUNT=0
TOTAL_HEADERS=0

CRITICAL_HEADERS=(
  "logger.h"
  "config.h"
  "internal/packed_ptr.h"
  "internal/string_table.h"
  "detail/file_view_impl.h"
  "detail/file_segment_impl.h"
  "reader/metadata_types.h"
  "reader/filesystem_v2.h"
  "reader/filesystem_loader.h"
  "utility/filesystem_extractor.h"
)

for header in "${CRITICAL_HEADERS[@]}"; do
  if [ -f "${INSTALL_DIR}/include/dwarfs/${header}" ]; then
    echo -e "  ${GREEN}✓${NC} ${header}"
    ((PRESENT_COUNT++))
  else
    echo -e "  ${RED}✗${NC} ${header} MISSING"
  fi
done

TOTAL_HEADERS=$(find "${INSTALL_DIR}/include/dwarfs" -name "*.h" | wc -l | tr -d ' ')

echo ""
echo "Header Statistics:"
echo "  Critical headers present: ${PRESENT_COUNT}/${#CRITICAL_HEADERS[@]}"
echo "  Total headers installed: ${TOTAL_HEADERS}"

if [ ${PRESENT_COUNT} -ne ${#CRITICAL_HEADERS[@]} ]; then
  echo -e "${RED}ERROR: Some critical headers are missing!${NC}"
  exit 1
fi

if [ ${TOTAL_HEADERS} -lt 100 ]; then
  echo -e "${YELLOW}WARNING: Too few headers installed (expected >100)${NC}"
fi

echo -e "${GREEN}✓${NC} All critical headers are present"
echo ""

# Step 3: Build header test (should SUCCEED)
echo "Step 3: Building header test against fixed installation..."
echo "  Expected: BUILD SUCCESS"
echo ""

rm -rf "${BUILD_DIR}"
mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"

# Try to build - expect success
if ! cmake .. -DCMAKE_PREFIX_PATH="${INSTALL_DIR}" > /tmp/cmake-out.log 2>&1; then
  echo -e "${RED}✗ CMake configuration FAILED${NC}"
  echo ""
  echo "Error details:"
  tail -20 /tmp/cmake-out.log | sed 's/^/  /'
  exit 1
fi

if ! cmake --build . > /tmp/build-out.log 2>&1; then
  echo -e "${RED}✗ Build FAILED${NC}"
  echo ""
  echo "Error details:"
  tail -20 /tmp/build-out.log | sed 's/^/  /'
  exit 1
fi

echo -e "${GREEN}✓${NC} Build succeeded"
echo ""

# Step 4: Run the header test
echo "Step 4: Running header test executable..."
echo ""

if [ ! -f "./header_test" ]; then
  echo -e "${RED}✗ header_test executable not found${NC}"
  exit 1
fi

if ! ./header_test > /tmp/test-out.log 2>&1; then
  echo -e "${RED}✗ Test execution FAILED${NC}"
  echo ""
  echo "Output:"
  cat /tmp/test-out.log | sed 's/^/  /'
  exit 1
fi

echo "Test Output:"
cat /tmp/test-out.log | sed 's/^/  /'
echo ""

# Cleanup
cd "${PROJECT_ROOT}"
rm -rf "${INSTALL_DIR}"
rm -f /tmp/cmake-out.log /tmp/build-out.log /tmp/test-out.log

echo "=================================================="
echo -e "${GREEN}✅ EXPECT-PASS TEST PASSED${NC}"
echo "=================================================="
echo ""
echo "Summary:"
echo "  - Built DwarFS with fixed header installation"
echo "  - Verified all critical headers were present"
echo "  - Header test built successfully"
echo "  - Test executable ran successfully"
echo ""
echo "This proves the header installation fix works correctly!"
echo ""