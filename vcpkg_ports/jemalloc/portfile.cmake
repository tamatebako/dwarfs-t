vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO tamatebako/jemalloc
    REF ea8e7eee91b6c9b291b15f4cf8b173400ec5c8f1
    SHA512 f3c06e495ea0984498deb975ab468f34f91d17e32493e9450765cb5579e574acbcf310391998b45c982c4cc82860f7155f8d5a5051077214277f704c82b523b3
    HEAD_REF master
)

# Suppress policy warnings for misplaced CMake files (autotools build installs CMake files during build)
set(VCPKG_POLICY_SKIP_MISPLACED_CMAKE_FILES_CHECK enabled)

if(VCPKG_TARGET_IS_WINDOWS)
    # Use native CMake build on Windows (autotools doesn't work with MSVC)
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

    vcpkg_cmake_build()

    vcpkg_cmake_install()

    # Copy MSVC compatibility headers if they exist
    if(EXISTS "${SOURCE_PATH}/include/msvc_compat/strings.h")
        file(COPY "${SOURCE_PATH}/include/msvc_compat/strings.h" DESTINATION "${CURRENT_PACKAGES_DIR}/include/jemalloc/msvc_compat")
        vcpkg_replace_string("${CURRENT_PACKAGES_DIR}/include/jemalloc/jemalloc.h" "<strings.h>" "\"msvc_compat/strings.h\"")
    endif()

    # Handle library naming
    if(VCPKG_LIBRARY_LINKAGE STREQUAL "static")
        # Static library - rename to jemalloc if needed
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
    vcpkg_cmake_configure(
        SOURCE_PATH "${SOURCE_PATH}"
        OPTIONS
            -DJEMALLOC_BUILD_STATIC=ON
            -DJEMALLOC_BUILD_SHARED=${VCPKG_LIBRARY_LINKAGE}
            -DJEMALLOC_ENABLE_PROF=OFF
            -DJEMALLOC_ENABLE_STATS=ON
            -DJEMALLOC_ENABLE_CXX=OFF
            -DJEMALLOC_ENABLE_DOC=OFF
            -DJEMALLOC_PREFIX=""
    )

    vcpkg_cmake_build()

    vcpkg_cmake_install()
endif()

vcpkg_fixup_pkgconfig()

# Install CMake config files
function(jemalloc_install_cmake_config)
    # Configure and install the CMake config file
    configure_file("${CMAKE_CURRENT_LIST_DIR}/jemallocConfig.cmake.in"
        "${CURRENT_PACKAGES_DIR}/share/jemalloc/jemallocConfig.cmake"
        @ONLY
    )

    # Create version file
    file(WRITE "${CURRENT_PACKAGES_DIR}/share/jemalloc/jemallocConfigVersion.cmake"
"
set(PACKAGE_VERSION \"5.5.0\")

if(PACKAGE_VERSION VERSION_LESS PACKAGE_FIND_VERSION)
    set(PACKAGE_VERSION_COMPATIBLE FALSE)
else()
    set(PACKAGE_VERSION_COMPATIBLE TRUE)
    if(PACKAGE_FIND_VERSION STREQUAL PACKAGE_VERSION)
        set(PACKAGE_VERSION_EXACT TRUE)
    endif()
endif()
"
    )
endfunction()

jemalloc_install_cmake_config()

# Fix JEMALLOC_USABLE_SIZE_CONST issue when using empty prefix
if(NOT VCPKG_TARGET_IS_WINDOWS)
    vcpkg_replace_string(
        "${CURRENT_PACKAGES_DIR}/include/jemalloc/jemalloc.h"
        "#undef JEMALLOC_USABLE_SIZE_CONST"
        "#undef JEMALLOC_USABLE_SIZE_CONST\n#define JEMALLOC_USABLE_SIZE_CONST const"
    )
endif()

vcpkg_copy_pdbs()

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/share")
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/tools")

# Handle copyright
file(INSTALL "${SOURCE_PATH}/COPYING" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}" RENAME copyright)
