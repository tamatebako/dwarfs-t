# x64-windows-msys triplet for Windows with MSYS2
# Uses MSYS2 for native Windows GCC builds
#
# This triplet tells vcpkg to build for Windows using MSYS2 compilers.
# We don't set VCPKG_CMAKE_SYSTEM_NAME to MSYS because vcpkg doesn't
# properly support it. Instead, we rely on CMAKE_C_COMPILER and CMAKE_CXX_COMPILER
# being set in the CMake preset to point to gcc/g++.

set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE static)
set(VCPKG_LIBRARY_LINKAGE static)
set(VCPKG_ENV_PASSTHROUGH PATH)

# Don't set VCPKG_CMAKE_SYSTEM_NAME - vcpkg will use "Windows" default
# which works better with MSYS/MinGW toolchains

# Linker flags to ensure console subsystem is used
set(VCPKG_CMAKE_CONFIGURE_OPTIONS
    "-DCMAKE_EXE_LINKER_FLAGS=-mconsole -Wl,--subsystem,console -Wl,--disable-auto-import"
    "-DCMAKE_SHARED_LINKER_FLAGS=-mconsole -Wl,--subsystem,console -Wl,--disable-auto-import"
    "-DCMAKE_MODULE_LINKER_FLAGS=-mconsole -Wl,--subsystem,console -Wl,--disable-auto-import"
)
