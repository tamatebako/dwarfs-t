vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO tamatebako/jemalloc
    REF ea8e7eee91b6c9b291b15f4cf8b173400ec5c8f1
    SHA512 22a08ca229c5600bc003847585edbba3afa48266cd15170a149265d12dfc639e965734584e0400a8a2a22620a53b1ddff38806addcde41390df18189e82e0637
    HEAD_REF master
)

# Suppress policy warnings for misplaced CMake files (autotools build installs CMake files during build)
set(VCPKG_POLICY_SKIP_MISPLACED_CMAKE_FILES_CHECK enabled)

if(VCPKG_TARGET_IS_WINDOWS)
    # Use native CMake build on Windows (autotools doesn't work with MSVC)
    # Configure CMake
    vcpkg_cmake_configure(
        SOURCE_PATH "${SOURCE_PATH}"
        OPTIONS
            -DJEMALLOC_BUILD_STATIC=ON
            -DJEMALLOC_BUILD_SHARED=OFF
            -DJEMALLOC_ENABLE_PROF=OFF
            -DJEMALLOC_ENABLE_STATS=ON
            -DJEMALLOC_ENABLE_CXX=OFF
            -DJEMALLOC_ENABLE_DOC=OFF
    )

    # Build and install using CMake
    vcpkg_cmake_install()

    # Copy MSVC compatibility headers if they exist
    if(EXISTS "${SOURCE_PATH}/include/msvc_compat/strings.h")
        file(COPY "${SOURCE_PATH}/include/msvc_compat/strings.h" DESTINATION "${CURRENT_PACKAGES_DIR}/include/jemalloc/msvc_compat")
        vcpkg_replace_string("${CURRENT_PACKAGES_DIR}/include/jemalloc/jemalloc.h" "<strings.h>" "\"msvc_compat/strings.h\"")
    endif()

    # Handle library naming for Windows
    if(VCPKG_LIBRARY_LINKAGE STREQUAL "static")
        # Static library - the CMake build produces jemalloc_static.lib
        if(EXISTS "${CURRENT_PACKAGES_DIR}/lib/jemalloc_static.lib")
            file(RENAME "${CURRENT_PACKAGES_DIR}/lib/jemalloc_static.lib" "${CURRENT_PACKAGES_DIR}/lib/jemalloc.lib")
        endif()
        if(NOT VCPKG_BUILD_TYPE AND EXISTS "${CURRENT_PACKAGES_DIR}/debug/lib/jemalloc_static.lib")
            file(RENAME "${CURRENT_PACKAGES_DIR}/debug/lib/jemalloc_static.lib" "${CURRENT_PACKAGES_DIR}/debug/lib/jemalloc.lib")
        endif()
    else()
        # Dynamic library - move DLL to bin/
        if(EXISTS "${CURRENT_PACKAGES_DIR}/lib/jemalloc.dll")
            file(MAKE_DIRECTORY "${CURRENT_PACKAGES_DIR}/bin")
            file(RENAME "${CURRENT_PACKAGES_DIR}/lib/jemalloc.dll" "${CURRENT_PACKAGES_DIR}/bin/jemalloc.dll")
        endif()
        if(NOT VCPKG_BUILD_TYPE AND EXISTS "${CURRENT_PACKAGES_DIR}/debug/lib/jemalloc.dll")
            file(MAKE_DIRECTORY "${CURRENT_PACKAGES_DIR}/debug/bin")
            file(RENAME "${CURRENT_PACKAGES_DIR}/debug/lib/jemalloc.dll" "${CURRENT_PACKAGES_DIR}/debug/bin/jemalloc.dll")
        endif()
    endif()
else()
    # Build without je_ prefix for Folly compatibility on Unix
    # Determine if we should build shared library
    if(VCPKG_LIBRARY_LINKAGE STREQUAL "dynamic")
        set(BUILD_SHARED_OPTION "ON")
    else()
        set(BUILD_SHARED_OPTION "OFF")
    endif()

    vcpkg_cmake_configure(
        SOURCE_PATH "${SOURCE_PATH}"
        OPTIONS
            -DJEMALLOC_BUILD_STATIC=ON
            -DJEMALLOC_BUILD_SHARED=${BUILD_SHARED_OPTION}
            -DJEMALLOC_ENABLE_PROF=OFF
            -DJEMALLOC_ENABLE_STATS=ON
            -DJEMALLOC_ENABLE_CXX=OFF
            -DJEMALLOC_ENABLE_DOC=OFF
            -DJEMALLOC_PREFIX=""
    )

    vcpkg_cmake_install()
endif()

vcpkg_fixup_pkgconfig()

# Fix JEMALLOC_USABLE_SIZE_CONST issue when using empty prefix
if(NOT VCPKG_TARGET_IS_WINDOWS)
    vcpkg_replace_string(
        "${CURRENT_PACKAGES_DIR}/include/jemalloc/jemalloc.h"
        "#undef JEMALLOC_USABLE_SIZE_CONST"
        "#undef JEMALLOC_USABLE_SIZE_CONST\n#define JEMALLOC_USABLE_SIZE_CONST const"
    )
endif()

# Fix posix_memalign declaration conflict with GCC's mm_malloc.h on Linux/glibc
# When fast_float includes SSE headers, it pulls in mm_malloc.h which declares
# posix_memalign with throw(), causing C++20 compilation error with jemalloc's
# __attribute__((nothrow)) declaration after symbol de-mangling.
# Solution: Conditionally declare je_posix_memalign only if mm_malloc.h hasn't
# been included yet. This is safe for Tebako users because:
# 1. The check is at compile-time - no behavior change if mm_malloc.h isn't included
# 2. Users who need je_posix_memalign can include jemalloc.h first
# 3. System posix_memalign is still available via standard C library
if(NOT VCPKG_TARGET_IS_WINDOWS AND NOT VCPKG_TARGET_IS_ANDROID)
    vcpkg_replace_string(
        "${CURRENT_PACKAGES_DIR}/include/jemalloc/jemalloc.h"
"JEMALLOC_EXPORT int JEMALLOC_SYS_NOTHROW je_posix_memalign(
    void **memptr, size_t alignment, size_t size) JEMALLOC_CXX_THROW
    JEMALLOC_ATTR(nonnull(1));"
"#if !defined(_MM_MALLOC_H) && !defined(__MM_MALLOC_H)
JEMALLOC_EXPORT int JEMALLOC_SYS_NOTHROW je_posix_memalign(
    void **memptr, size_t alignment, size_t size) JEMALLOC_CXX_THROW
    JEMALLOC_ATTR(nonnull(1));
#endif
"
    )
endif()

vcpkg_copy_pdbs()

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/share")
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/tools")

# Handle copyright
file(INSTALL "${SOURCE_PATH}/COPYING" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}" RENAME copyright)
