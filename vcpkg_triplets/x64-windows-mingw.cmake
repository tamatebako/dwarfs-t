# x64-windows-mingw triplet for Windows with MinGW-w64
# Uses MinGW-w64 for native Windows GCC builds
# Based on vcpkg community triplet pattern

set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE static)
set(VCPKG_LIBRARY_LINKAGE static)
set(VCPKG_ENV_PASSTHROUGH PATH)

# Tell vcpkg to use our custom MinGW toolchain file
# The toolchain file must be relative to the vcpkg triplets directory
get_filename_component(TRIPLET_DIR "${CMAKE_CURRENT_LIST_FILE}" DIRECTORY)
set(VCPKG_CHAINLOAD_TOOLCHAIN_FILE "${TRIPLET_DIR}/mingw.cmake")

set(VCPKG_CMAKE_SYSTEM_NAME MinGW)

# Prevent CMake from adding --out-implib flag which forces GUI mode
# The --out-implib flag causes MinGW to use GUI startup files expecting WinMain
# Console mode is the default for MinGW, no subsystem flags needed
set(VCPKG_CMAKE_CONFIGURE_OPTIONS
    -DCMAKE_IMPORT_LIBRARY_SUFFIX=
    -DCMAKE_WIN32_EXECUTABLE=OFF
)
