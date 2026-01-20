# x64-windows-dynamic triplet for x64 Windows
# This is a dynamic triplet for x64 Windows builds with MSVC

set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE dynamic)

set(VCPKG_CMAKE_SYSTEM_NAME Windows)

# Use MSVC on Windows
set(VCPKG_PLATFORM_TOOLSET x64)

# Windows-specific settings
set(VCPKG_USE_SYSTEM_LIBRARIES OFF)
