#!/bin/bash
# test-expect-fail.sh - Verify test detects missing headers (BEFORE fix)
#
# This script tests that our header test CORRECTLY FAILS when headers are missing.
# It simulates the BUG STATE (before the fix was applied).
#
# Expected Result: BUILD FAILURE with "file not found" errors
# This proves the test can detect header installation issues.

set -e  # Exit on error

echo "=================================================="
echo "DwarFS Header Test - EXPECT FAIL"
echo "=================================================="
echo ""
echo "This test verifies that our header test correctly"
echo "detects MISSING headers (the bug state)."
echo ""
echo "Expected result: BUILD FAILURE ✅"
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
BUILD_DIR="${SCRIPT_DIR}/build-expect-fail"
INSTALL_DIR="/tmp/dwarfs-broken-install"

echo "Configuration:"
echo "  Project Root: ${PROJECT_ROOT}"
echo "  Build Dir: ${BUILD_DIR}"
echo "  Install Dir: ${INSTALL_DIR}"
echo ""

# Step 1: Build DwarFS with BROKEN header installation
echo "Step 1: Building DwarFS with BROKEN header installation (simulating bug)..."
cd "${PROJECT_ROOT}"

# Save original file
cp cmake/libdwarfs.cmake cmake/libdwarfs.cmake.backup

# Break the installation (simulate the original bug)
sed -i.tmp 's/FILES_MATCHING PATTERN "\*.h"/PATTERN "internal" EXCLUDE/' cmake/libdwarfs.cmake
rm -f cmake/libdwarfs.cmake.tmp

# Build and install with broken config
mkdir -p build-broken && cd build-broken
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

# Restore original file
cd "${PROJECT_ROOT}"
mv cmake/libdwarfs.cmake.backup cmake/libdwarfs.cmake
rm -rf build-broken

echo -e "${YELLOW}✓${NC} Built with broken installation (headers excluded)"
echo ""

# Step 2: Verify critical headers are MISSING
echo "Step 2: Verifying that critical headers are MISSING..."
MISSING_COUNT=0

for header in \
  "internal/packed_ptr.h" \
  "internal/string_table.h" \
  "detail/file_view_impl.h"; do
  
  if [ ! -f "${INSTALL_DIR}/include/dwarfs/${header}" ]; then
    echo -e "  ${RED}✗${NC} ${header} missing (expected)"
    ((MISSING_COUNT++))
  else
    echo -e "  ${YELLOW}!${NC} ${header} found (unexpected!)"
  fi
done

if [ ${MISSING_COUNT} -eq 0 ]; then
  echo -e "${RED}ERROR: No headers missing - bug simulation failed!${NC}"
  exit 1
fi

echo -e "${GREEN}✓${NC} Critical headers are missing (as expected)"
echo ""

# Step 3: Try to build header test (should FAIL)
echo "Step 3: Building header test against broken installation..."
echo "  Expected: BUILD FAILURE"
echo ""

rm -rf "${BUILD_DIR}"
mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"

# Try to build - expect failure
if cmake .. -DCMAKE_PREFIX_PATH="${INSTALL_DIR}" > /dev/null 2>&1 && \
   cmake --build . > /dev/null 2>&1; then
  
  echo -e "${RED}✗ UNEXPECTED: Build succeeded!${NC}"
  echo -e "${RED}  The test should have failed with missing headers.${NC}"
  echo -e "${RED}  This means the test is not working correctly.${NC}"
  exit 1
else
  echo -e "${GREEN}✓ Build FAILED as expected${NC}"
  echo ""
  echo "Error details (last 10 lines):"
  cmake .. -DCMAKE_PREFIX_PATH="${INSTALL_DIR}" > /tmp/cmake-out.log 2>&1 || true
  cmake --build . > /tmp/build-out.log 2>&1 || true
  tail -10 /tmp/build-out.log | sed 's/^/  /'
  echo ""
fi

# Cleanup
cd "${PROJECT_ROOT}"
rm -rf "${INSTALL_DIR}"
rm -f /tmp/cmake-out.log /tmp/build-out.log

echo "=================================================="
echo -e "${GREEN}✅ EXPECT-FAIL TEST PASSED${NC}"
echo "=================================================="
echo ""
echo "Summary:"
echo "  - Built DwarFS with broken header installation"
echo "  - Verified critical headers were missing  "
echo "  - Header test correctly FAILED to build"
echo ""
echo "This proves the header test can detect missing headers!"
echo ""