# x64-mingw-static triplet for Windows with MinGW-w64
# Standard vcpkg community triplet naming convention

set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)
set(VCPKG_ENV_PASSTHROUGH PATH)

set(VCPKG_CMAKE_SYSTEM_NAME MinGW)
set(VCPKG_CMAKE_SYSTEM_VERSION 10.0)

# Linker flags to ensure console subsystem is used
set(VCPKG_CMAKE_CONFIGURE_OPTIONS "-DCMAKE_EXE_LINKER_FLAGS=-mconsole" "-DCMAKE_MODULE_LINKER_FLAGS=-mconsole")
