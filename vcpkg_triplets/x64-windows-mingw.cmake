# x64-windows-mingw triplet for Windows with MinGW-w64
# Uses MinGW-w64 for native Windows GCC builds

set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)
set(VCPKG_ENV_PASSTHROUGH PATH)

# Tell vcpkg this is a MinGW environment
set(VCPKG_CMAKE_SYSTEM_NAME MinGW)
