# x64-windows-mingw triplet for Windows with MinGW-w64
# Uses MinGW-w64 for native Windows GCC builds

set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE dynamic)
set(VCPKG_ENV_PASSTHROUGH PATH)

set(VCPKG_CMAKE_SYSTEM_NAME MinGW)
set(VCPKG_CXX_FLAGS "-Dmingw")
set(VCPKG_C_FLAGS "-Dmingw")

# Use MinGW toolchain
set(VCPKG_PLATFORM_TOOLSET mingw)

# Enable DLLs without LIBs policy for MinGW
set(VCPKG_POLICY_DLLS_WITHOUT_LIBS enabled)

# Disable metrics
set(VCPKG_DISABLE_METRICS ON)
