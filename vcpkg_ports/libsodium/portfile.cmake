vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO jedisct1/libsodium
    REF ${VERSION}
    SHA512 415c3c8c231472ab2d47188844c0c5bc5fb027ce00f01eb60207e52af1b5a7e7f3bd1fd1f4c3ddcfc5fdd05ab8e24e6d95368119803067f944279fc7abc7568b8d51bb9eeb69fc282aa9423f3bb10f0aa8c8078a3c5b5bfdec
    HEAD_REF master
    PATCHES
        arm-neon-fixes.patch
)

# ARM64 fix: make sure we don't try to build NEON code on incompatible architectures
if(VCPKG_TARGET_ARCHITECTURE MATCH "arm64|aarch64")
    # The ARM NEON code has compatibility issues with newer GCC
    # We'll compile but not use the ARM crypto implementation by default
    list(APPEND VCPKG_CMAKE_CONFIGURE_OPTIONS
        -DARM_NEON_IMPLEMENTATION=0
    )
endif()

vcpkg_configure_cmake(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DUTILS_DISABLE_TESTS=ON
)

vcpkg_install_cmake()

vcpkg_copy_pdbs()

# Clean up includes
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")

# Handle copyright
file(INSTALL "${SOURCE_PATH}/LICENSE" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}" RENAME copyright)
