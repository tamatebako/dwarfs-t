#!/bin/bash
#
# Test script for vcpkg port installation
# Tests both libdwarfs and dwarfs ports
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

echo "==========================================="
echo "DwarFS vcpkg Installation Test"
echo "==========================================="
echo ""

# Check if vcpkg is installed
if [ ! -d ~/vcpkg ]; then
    echo "❌ vcpkg not found at ~/vcpkg"
    echo "Installing vcpkg..."
    git clone https://github.com/microsoft/vcpkg.git ~/vcpkg
    cd ~/vcpkg && ./bootstrap-vcpkg.sh
    echo "✅ vcpkg installed successfully"
fi

VCPKG_EXE=~/vcpkg/vcpkg

echo "vcpkg version:"
${VCPKG_EXE} version
echo ""

# Detect platform triplet
if [[ "$OSTYPE" == "darwin"* ]]; then
    if [[ "$(uname -m)" == "arm64" ]]; then
        TRIPLET="arm64-osx"
    else
        TRIPLET="x64-osx"
    fi
elif [[ "$OSTYPE" == "linux-gnu"* ]]; then
    if [[ "$(uname -m)" == "x86_64" ]]; then
        TRIPLET="x64-linux"
    elif [[ "$(uname -m)" == "aarch64" ]]; then
        TRIPLET="arm64-linux"
    else
        TRIPLET="$(uname -m)-linux"
    fi
else
    echo "⚠️  Unknown platform, using default triplet"
    TRIPLET="x64-linux"
fi

echo "Using triplet: ${TRIPLET}"
echo ""

# Clean up previous installations
echo "Cleaning up previous installations..."
${VCPKG_EXE} remove libdwarfs --triplet=${TRIPLET} 2>/dev/null || true
${VCPKG_EXE} remove dwarfs --triplet=${TRIPLET} 2>/dev/null || true
echo ""

# Test libdwarfs port
echo "==========================================="
echo "Testing libdwarfs port installation"
echo "==========================================="
echo ""

${VCPKG_EXE} install libdwarfs \
    --overlay-ports="${PROJECT_ROOT}/ports" \
    --triplet=${TRIPLET} \
    --no-binarycaching

if [ $? -eq 0 ]; then
    echo ""
    echo "✅ libdwarfs installed successfully"
else
    echo ""
    echo "❌ libdwarfs installation failed"
    exit 1
fi

# Test dwarfs port
echo ""
echo "==========================================="
echo "Testing dwarfs port installation"
echo "==========================================="
echo ""

${VCPKG_EXE} install dwarfs \
    --overlay-ports="${PROJECT_ROOT}/ports" \
    --triplet=${TRIPLET} \
    --no-binarycaching

if [ $? -eq 0 ]; then
    echo ""
    echo "✅ dwarfs tools installed successfully"
else
    echo ""
    echo "❌ dwarfs tools installation failed"
    exit 1
fi

# List installed packages
echo ""
echo "==========================================="
echo "Installed packages"
echo "==========================================="
${VCPKG_EXE} list | grep dwarfs

# Test CMake find_package integration
echo ""
echo "==========================================="
echo "Testing CMake find_package() integration"
echo "==========================================="
echo ""

TEST_DIR="/tmp/dwarfs-vcpkg-test-$$"
mkdir -p "${TEST_DIR}"
cd "${TEST_DIR}"

# Create test CMakeLists.txt
cat > CMakeLists.txt <<'EOF'
cmake_minimum_required(VERSION 3.20)
project(dwarfs_vcpkg_test)

find_package(dwarfs CONFIG REQUIRED)

add_executable(test_common test_common.cpp)
target_link_libraries(test_common PRIVATE dwarfs::dwarfs_common)

add_executable(test_reader test_reader.cpp)
target_link_libraries(test_reader PRIVATE dwarfs::dwarfs_reader)

message(STATUS "✅ find_package(dwarfs) successful")
message(STATUS "✅ All library targets found")
EOF

# Create test source file for dwarfs_common
cat > test_common.cpp <<'EOF'
#include <dwarfs/logger.h>
#include <iostream>

int main() {
    auto lgr = dwarfs::stream_logger::create(std::cout);
    lgr->info("DwarFS vcpkg test: dwarfs_common works!");
    return 0;
}
EOF

# Create test source file for dwarfs_reader
cat > test_reader.cpp <<'EOF'
#include <dwarfs/reader/filesystem_loader.h>
#include <iostream>

int main() {
    std::cout << "DwarFS vcpkg test: dwarfs_reader headers work!" << std::endl;
    return 0;
}
EOF

# Configure CMake
echo "Configuring CMake project..."
cmake -B build \
    -DCMAKE_TOOLCHAIN_FILE=~/vcpkg/scripts/buildsystems/vcpkg.cmake \
    -DCMAKE_BUILD_TYPE=Release

if [ $? -eq 0 ]; then
    echo "✅ CMake configuration successful"
else
    echo "❌ CMake configuration failed"
    cd "${PROJECT_ROOT}"
    rm -rf "${TEST_DIR}"
    exit 1
fi

# Build test programs
echo ""
echo "Building test programs..."
cmake --build build --parallel

if [ $? -eq 0 ]; then
    echo "✅ Build successful"
else
    echo "❌ Build failed"
    cd "${PROJECT_ROOT}"
    rm -rf "${TEST_DIR}"
    exit 1
fi

# Run test programs
echo ""
echo "Running test programs..."
./build/test_common
if [ $? -eq 0 ]; then
    echo "✅ test_common executed successfully"
else
    echo "❌ test_common execution failed"
    cd "${PROJECT_ROOT}"
    rm -rf "${TEST_DIR}"
    exit 1
fi

./build/test_reader
if [ $? -eq 0 ]; then
    echo "✅ test_reader executed successfully"
else
    echo "❌ test_reader execution failed"
    cd "${PROJECT_ROOT}"
    rm -rf "${TEST_DIR}"
    exit 1
fi

# Clean up test directory
cd "${PROJECT_ROOT}"
rm -rf "${TEST_DIR}"

echo ""
echo "==========================================="
echo "All vcpkg tests passed! ✅"
echo "==========================================="
echo ""
echo "Summary:"
echo "  ✅ libdwarfs port installs correctly"
echo "  ✅ dwarfs port installs correctly"
echo "  ✅ CMake find_package(dwarfs) works"
echo "  ✅ Test programs compile and link"
echo "  ✅ Test programs execute successfully"
echo ""
echo "vcpkg integration is production-ready!"