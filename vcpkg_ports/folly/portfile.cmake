if(VCPKG_TARGET_IS_WINDOWS)
    vcpkg_check_linkage(ONLY_STATIC_LIBRARY)
endif()

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO facebook/folly
    REF "v2025.12.29.00"
    SHA512 845e47618ad57a0fab5e29dd1a4f61980659e7e0f42497c4b54c5ac8e19707d288c2846409866843813367daf009ec4ec0d6c22dd005514a07f0df07a1990212
    HEAD_REF main
    PATCHES
        disable-uninitialized-resize-on-new-stl.patch
        fix-deps.patch
        fix-unistd-include.patch
        fix-absolute-dir.patch
)

# Note: posix_memalign conflict with GCC's mm_malloc.h is resolved in the
# jemalloc port itself (jemalloc source template patched to guard declaration)

string(COMPARE EQUAL "${VCPKG_CRT_LINKAGE}" "static" MSVC_USE_STATIC_RUNTIME)

vcpkg_check_features(OUT_FEATURE_OPTIONS FEATURE_OPTIONS
    FEATURES
        "libaio"     VCPKG_LOCK_FIND_PACKAGE_LibAIO
        "libsodium"  VCPKG_LOCK_FIND_PACKAGE_LIBSODIUM
        "liburing"   VCPKG_LOCK_FIND_PACKAGE_LibUring
        "lz4"        VCPKG_LOCK_FIND_PACKAGE_LZ4
        "snappy"     VCPKG_LOCK_FIND_PACKAGE_SNAPPY
        "zstd"       VCPKG_LOCK_FIND_PACKAGE_ZSTD
)

# Use vcpkg's jemalloc installation (already in include/lib paths via toolchain)
# Custom jemalloc overlay port provides unprefixed function names
set(JEMALLOC_CMAKE_ARGS)
if(NOT VCPKG_TARGET_IS_WINDOWS)
    # Ensure CMAKE_REQUIRED_INCLUDES points to vcpkg's include dir for CHECK_INCLUDE_FILE_CXX
    list(APPEND JEMALLOC_CMAKE_ARGS
        "-DCMAKE_REQUIRED_INCLUDES=${CURRENT_INSTALLED_DIR}/include"
        "-DCMAKE_EXE_LINKER_FLAGS=-L${CURRENT_INSTALLED_DIR}/lib -ljemalloc"
        "-DCMAKE_SHARED_LINKER_FLAGS=-L${CURRENT_INSTALLED_DIR}/lib -ljemalloc"
    )
endif()


vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DMSVC_USE_STATIC_RUNTIME=${MSVC_USE_STATIC_RUNTIME}
        -DCMAKE_INSTALL_DIR=share/folly
        -DCMAKE_POLICY_DEFAULT_CMP0167=NEW
        # Existing options
        -DVCPKG_LOCK_FIND_PACKAGE_fmt=ON
        -DVCPKG_LOCK_FIND_PACKAGE_LibDwarf=OFF
        -DVCPKG_LOCK_FIND_PACKAGE_Libiberty=OFF
        -DVCPKG_LOCK_FIND_PACKAGE_LibUnwind=${VCPKG_TARGET_IS_LINUX}
        -DVCPKG_LOCK_FIND_PACKAGE_ZLIB=ON
        ${FEATURE_OPTIONS}
        ${JEMALLOC_CMAKE_ARGS}
    MAYBE_UNUSED_VARIABLES
        MSVC_USE_STATIC_RUNTIME
)

vcpkg_cmake_install()
vcpkg_copy_pdbs()
vcpkg_fixup_pkgconfig()
vcpkg_cmake_config_fixup()

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
