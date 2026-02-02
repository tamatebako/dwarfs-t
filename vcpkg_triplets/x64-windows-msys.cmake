# x64-windows-msys triplet for Windows with MSYS2
# Uses MSYS2 runtime for POSIX-like compatibility on Windows

set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE static)
set(VCPKG_LIBRARY_LINKAGE dynamic)
set(VCPKG_ENV_PASSTHROUGH PATH)

set(VCPKG_CMAKE_SYSTEM_NAME MSYS)
set(VCPKG_CXX_FLAGS "-DMSYS")
set(VCPKG_C_FLAGS "-DMSYS")

# Use MSYS2 paths
set(VCPKG_PLATFORM_TOOLSET msys)

# Enable DLLs without LIBs policy for MSYS
set(VCPKG_POLICY_DLLS_WITHOUT_LIBS enabled)

# Disable metrics
set(VCPKG_DISABLE_METRICS ON)
