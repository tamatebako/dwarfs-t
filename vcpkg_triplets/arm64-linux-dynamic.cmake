# arm64-linux-dynamic triplet for ARM64 Linux
# This is a dynamic triplet for arm64 Linux builds

set(VCPKG_TARGET_ARCHITECTURE arm64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE dynamic)

set(VCPKG_CMAKE_SYSTEM_NAME Linux)

# Use GNU compiler on Linux
set(VCPKG_PLATFORM_TOOLSET arm64)

# Compiler flags for position-independent code
set(VCPKG_C_FLAGS "-fPIC")
set(VCPKG_CXX_FLAGS "-fPIC")
