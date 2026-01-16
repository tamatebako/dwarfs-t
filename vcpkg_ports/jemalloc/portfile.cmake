vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO tamatebako/jemalloc
    REF 5.5.0
    SHA512 c24539d845f57290916fae7ed5892cc9f07a347580f65db71bee0c2f11c482004b7f8c27a082c889d7604c14fa5cf6b3be77eb1cf579af2949495865c1d7ed7f
    HEAD_REF master
)

# Determine build type based on linkage
if(VCPKG_LIBRARY_LINKAGE STREQUAL "dynamic")
    set(BUILD_SHARED ON)
    set(BUILD_STATIC OFF)
else()
    set(BUILD_SHARED OFF)
    set(BUILD_STATIC ON)
endif()

# Native CMake build on all platforms (Windows, Linux, macOS, FreeBSD)
# Tebako jemalloc has full native CMake support - no autotools needed!
vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DJEMALLOC_BUILD_SHARED=${BUILD_SHARED}
        -DJEMALLOC_BUILD_STATIC=${BUILD_STATIC}
        -DJEMALLOC_ENABLE_DOC=OFF
        -DJEMALLOC_ENABLE_PROF=OFF
        -DJEMALLOC_ENABLE_STATS=ON
)

vcpkg_cmake_install()

# Fix CMake config file paths
vcpkg_cmake_config_fixup(CONFIG_PATH lib/cmake/jemalloc)

# Copy PDB files (Windows only)
vcpkg_copy_pdbs()

# Remove duplicate files
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/share")

# Install copyright
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/COPYING")
