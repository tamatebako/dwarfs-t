# arm64-windows-dynamic triplet for ARM64 Windows
# This is a dynamic triplet for arm64 Windows builds with MSVC

set(VCPKG_TARGET_ARCHITECTURE arm64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE dynamic)

set(VCPKG_CMAKE_SYSTEM_NAME Windows)

# Use MSVC on Windows
set(VCPKG_PLATFORM_TOOLSET arm64)

# Windows-specific settings
set(VCPKG_USE_SYSTEM_LIBRARIES OFF)
