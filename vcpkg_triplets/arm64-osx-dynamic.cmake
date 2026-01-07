# arm64-osx-dynamic triplet for Apple Silicon Macs
# This is a dynamic triplet for arm64 macOS builds

set(VCPKG_TARGET_ARCHITECTURE arm64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE dynamic)

set(VCPKG_CMAKE_SYSTEM_NAME Darwin)
set(VCPKG_OSX_ARCHITECTURES arm64)

set(VCPKG_PLATFORM_TOOLSET arm64)

# Enable shared libraries for specific packages that need them
set(VCPKG_ALLOW_APPLE_CLANG ON)

# Use system libraries where available
set(VCPKG_USE_SYSTEM_LIBRARIES ON)
