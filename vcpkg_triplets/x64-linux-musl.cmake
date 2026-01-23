# Alpine Linux (musl) triplet for x64
set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_CRT_LINKAGE static)
set(VCPKG_LIBRARY_LINKAGE static)

set(VCPKG_CRT_FLAGS
    -fPIC
    # Alpine-specific flags
    ${VCPKG_CRT_FLAGS})
set(VCPKG_CXX_FLAGS
    -fPIC
    # Alpine-specific flags
    ${VCPKG_CXX_FLAGS})

set(VCPKG_CHAINLOAD_TOOLCHAIN_FILE /cmake/fileos/vcpkg_tcfile_musl.cmake)

# Alpine Linux builds typically use musl instead of glibc
# This triplet assumes Alpine with musl libc
