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
        fix-posix-memalign-conflict.patch
        tebako-jemalloc-support.patch
        link-jemalloc.patch
)

# Note: posix_memalign conflict with GCC's mm_malloc.h is fixed via patch
# See fix-posix-memalign-conflict.patch for details

# Disable exception tracer on ARM64 Linux due to multiple definition errors
# The exception tracer defines its own __cxa_* symbols which conflict with libstdc++
# when linking statically on ARM64 Linux
# Note: The CMake option is FOLLY_NO_EXCEPTION_TRACER (with NO_ prefix)
if(VCPKG_TARGET_IS_LINUX AND VCPKG_TARGET_ARCHITECTURE STREQUAL "arm64")
    set(FOLLY_NO_EXCEPTION_TRACER ON)
else()
    set(FOLLY_NO_EXCEPTION_TRACER OFF)
endif()

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

# Tebako jemalloc integration via tebako-jemalloc-support.patch
# This patch provides unprefixed aliases (nallocx, sdallocx, etc.) for
# Tebako's je_ prefixed jemalloc functions.
# Tebako's jemalloc supports all platforms including Windows (CMake build)
set(JEMALLOC_CMAKE_ARGS
    "-DCMAKE_REQUIRED_INCLUDES=${CURRENT_INSTALLED_DIR}/include"
)


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
        # Disable exception tracer on ARM64 Linux to avoid multiple definition errors
        -DFOLLY_NO_EXCEPTION_TRACER=${FOLLY_NO_EXCEPTION_TRACER}
        ${FEATURE_OPTIONS}
        ${JEMALLOC_CMAKE_ARGS}
    MAYBE_UNUSED_VARIABLES
        MSVC_USE_STATIC_RUNTIME
)

vcpkg_cmake_install()
vcpkg_copy_pdbs()
vcpkg_fixup_pkgconfig()
vcpkg_cmake_config_fixup()

# Tebako: Add jemalloc dependency to folly-config.cmake
# This is needed because folly_deps links against jemalloc but the dependency
# is not exported in the generated cmake config file.
# Works on all platforms since Tebako's jemalloc supports Windows
vcpkg_replace_string("${CURRENT_PACKAGES_DIR}/share/folly/folly-config.cmake"
    "# Find folly's dependencies"
    "# Tebako: Find jemalloc dependency for folly_deps\nfind_dependency(jemalloc CONFIG)\n\n# Find folly's dependencies")

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
