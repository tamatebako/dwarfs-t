# x64-windows-mingw triplet for Windows with MinGW-w64
# Uses MinGW-w64 for native Windows GCC builds
# Based on vcpkg community triplet pattern

set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE static)
set(VCPKG_LIBRARY_LINKAGE static)
set(VCPKG_ENV_PASSTHROUGH PATH)

# Tell vcpkg to use the built-in MinGW toolchain file
# This tells vcpkg which compiler binary (GCC) to use for building dependencies
set(VCPKG_CHAINLOAD_TOOLCHAIN_FILE "${VCPKG_ROOT_DIR}/scripts/toolchains/windows/mingw.cmake")

set(VCPKG_CMAKE_SYSTEM_NAME MinGW)
