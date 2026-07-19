# Fetch tamatebako/jemalloc from GitHub main branch (has CMake support)
vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO tamatebako/jemalloc
    REF main
    SHA512 2b9947c2706807909c501cbc7b07472ef70bdb1095ffd2d4831378030868c132f4bab1d2eff9cf70fd35888f08c81ca40bbccf56ba67541206373438278bbf9b
    HEAD_REF main
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
# No more MSBuild workaround - native CMake works everywhere!
vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DJEMALLOC_BUILD_SHARED=${BUILD_SHARED}
        -DJEMALLOC_BUILD_STATIC=${BUILD_STATIC}
        -DJEMALLOC_ENABLE_DOC=OFF
        -DJEMALLOC_ENABLE_PROF=OFF
        -DJEMALLOC_ENABLE_STATS=ON
        -DJEMALLOC_BUILD_TESTS=OFF
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