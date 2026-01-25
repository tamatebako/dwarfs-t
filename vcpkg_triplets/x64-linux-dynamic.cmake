# x64-linux-dynamic triplet for x64 Linux
# Uses dynamic library linkage (BUILD_SHARED_LIBS=ON)

set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE dynamic)

set(VCPKG_CMAKE_SYSTEM_NAME Linux)
set(VCPKG_PLATFORM_TOOLSET x64)

# Enable position-independent code
set(VCPKG_CMAKE_CONFIGURE_OPTIONS -PIC)

# Compiler flags (similar to x64-linux but adapted for dynamic)
set(VCPKG_C_FLAGS "-fPIC")
set(VCPKG_CXX_FLAGS "-fPIC")
