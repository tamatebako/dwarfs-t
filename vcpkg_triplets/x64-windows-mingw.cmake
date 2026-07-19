# x64-windows-mingw triplet for Windows with MinGW-w64
# Uses MinGW-w64 for native Windows GCC builds
#
# This triplet tells vcpkg to build for Windows using MinGW compilers.
# We don't set VCPKG_CMAKE_SYSTEM_NAME to MinGW because vcpkg doesn't
# properly support it. Instead, we rely on CMAKE_C_COMPILER and CMAKE_CXX_COMPILER
# being set in the CMake preset to point to gcc/g++.

set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE static)
set(VCPKG_LIBRARY_LINKAGE static)
set(VCPKG_ENV_PASSTHROUGH PATH)

# Don't set VCPKG_CMAKE_SYSTEM_NAME - vcpkg will use "Windows" default
# which works better with MinGW toolchains
