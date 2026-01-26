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
    # Build without je_ prefix on all Unix platforms for Folly compatibility
    # The je_ prefix causes issues with Folly which expects unprefixed function names
    # To avoid conflict with GCC's mm_malloc.h, we define _MM_MALLOC_H in Folly instead

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
    )

    vcpkg_cmake_install()
endif()

vcpkg_fixup_pkgconfig()

vcpkg_copy_pdbs()

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/share")
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/tools")

# Handle copyright
file(INSTALL "${SOURCE_PATH}/COPYING" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}" RENAME copyright)
