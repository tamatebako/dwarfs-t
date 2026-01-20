vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO argtable/argtable3
    REF v3.3.1
    SHA512 4c22928566922d22bc9cc23aa4be0b8b8dc2316d1c5c85f6fb65e2e39f7f7f6e6e0f3e4c3c4f3c7f7c6f7e0f3e4c3c4f3c7f7c6f7e0f3e4c3c4f3c7f7c6f7e0f3e4c3c4f3c7f7c6f7e0f3e4c3c4f3c7f7c6f7e0f3e4c3c4f3c7f7c6f7e
    HEAD_REF master
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DARGTABLE3_ENABLE_TESTS=OFF
        -DARGTABLE3_ENABLE_EXAMPLES=OFF
)

vcpkg_cmake_install()

vcpkg_copy_pdbs()
vcpkg_cmake_config_fixup()

# Remove test and example directories
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/share")
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/tools")

# Handle copyright
file(INSTALL "${SOURCE_PATH}/LICENSE" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}" RENAME copyright)
