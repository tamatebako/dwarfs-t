#!/bin/bash
set -e

echo "=== DwarFS Complete Clean ==="
echo ""

# Remove all build directories
echo "Removing build directories..."
rm -rf build-*/ build/ 2>/dev/null || true

# Remove CMake cache files
echo "Removing CMake cache..."
find . -name "CMakeCache.txt" -delete 2>/dev/null || true
find . -name "CMakeFiles" -type d -exec rm -rf {} + 2>/dev/null || true

# Remove generated Thrift files
echo "Removing generated Thrift files..."
rm -rf include/dwarfs/gen-cpp2/ 2>/dev/null || true

# Remove generated FlatBuffers files
echo "Removing generated FlatBuffers files..."
rm -rf include/dwarfs/gen-flatbuffers/ 2>/dev/null || true

# Remove vcpkg installed
echo "Removing vcpkg installed..."
rm -rf vcpkg_installed/ 2>/dev/null || true

# Remove test artifacts
echo "Removing test artifacts..."
rm -rf /tmp/dwarfs-test-* 2>/dev/null || true
rm -rf /tmp/test-*.{dff,dtc,dth,dwarfs} 2>/dev/null || true
rm -rf /tmp/extracted-* 2>/dev/null || true
rm -rf /tmp/meta-*.txt 2>/dev/null || true

echo ""
echo "✅ Clean complete. Ready for fresh build."