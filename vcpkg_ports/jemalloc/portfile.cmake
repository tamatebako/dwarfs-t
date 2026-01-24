vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO tamatebako/jemalloc
    REF 5.5.0
    SHA512 c24539d845f57290916fae7ed5892cc9f07a347580f65db71bee0c2f11c482004b7f8c27a082c889d7604c14fa5cf6b3be77eb1cf579af2949495865c1d7ed7f
    HEAD_REF master
    # Suppress policy warnings for misplaced CMake files (autotools build installs CMake files during build)
    VCPKG_POLICY_SKIP_MISPLACED_CMAKE_FILES_CHECK
)

if(VCPKG_TARGET_IS_WINDOWS)
    # Use CMake on Windows (Tebako jemalloc has CMake support)
    vcpkg_cmake_configure(
        SOURCE_PATH "${SOURCE_PATH}"
        OPTIONS "-DJEMALLOC_PREFIX=" "-DJEMALLOC_VERSION=5.5.0"
    )

    vcpkg_cmake_build()
    vcpkg_cmake_install()

    # Copy MSVC compatibility headers
    file(COPY "${SOURCE_PATH}/include/msvc_compat/strings.h" DESTINATION "${CURRENT_PACKAGES_DIR}/include/jemalloc/msvc_compat")
    vcpkg_replace_string("${CURRENT_PACKAGES_DIR}/include/jemalloc/jemalloc.h" "<strings.h>" "\"msvc_compat/strings.h\"")

    # Handle dynamic library DLL placement
    if(VCPKG_LIBRARY_LINKAGE STREQUAL "dynamic")
        file(COPY "${CURRENT_BUILDTREES_DIR}/${TARGET_TRIPLET}-rel/lib/jemalloc.lib" DESTINATION "${CURRENT_PACKAGES_DIR}/lib")
        file(MAKE_DIRECTORY "${CURRENT_PACKAGES_DIR}/bin")
        file(RENAME "${CURRENT_PACKAGES_DIR}/lib/jemalloc.dll" "${CURRENT_PACKAGES_DIR}/bin/jemalloc.dll")
        if(NOT VCPKG_BUILD_TYPE)
            file(COPY "${CURRENT_BUILDTREES_DIR}/${TARGET_TRIPLET}-dbg/lib/jemalloc.lib" DESTINATION "${CURRENT_PACKAGES_DIR}/debug/lib")
            file(MAKE_DIRECTORY "${CURRENT_PACKAGES_DIR}/debug/bin")
            file(RENAME "${CURRENT_PACKAGES_DIR}/debug/lib/jemalloc.dll" "${CURRENT_PACKAGES_DIR}/debug/bin/jemalloc.dll")
        endif()
    endif

    # Fix pkgconfig file for static linkage
    if(VCPKG_LIBRARY_LINKAGE STREQUAL "static")
        vcpkg_replace_string("${CURRENT_PACKAGES_DIR}/lib/pkgconfig/jemalloc.pc" "install_suffix=" "install_suffix=_s")
        if(NOT VCPKG_BUILD_TYPE)
            vcpkg_replace_string("${CURRENT_PACKAGES_DIR}/debug/lib/pkgconfig/jemalloc.pc" "install_suffix=" "install_suffix=_s")
        endif()
    endif()
else()
    # Build without je_ prefix for Folly compatibility
    # Set version in jemalloc's expected format
    set(opts "--with-jemalloc-prefix=" "--with-version=5.5.0-0-g0000000000000000000000000000000000000000")

    vcpkg_make_configure(
        AUTORECONF
        SOURCE_PATH "${SOURCE_PATH}"
        OPTIONS ${opts}
    )

    vcpkg_make_install()
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
    # Check if the pattern exists before attempting replacement
    file(READ "${CURRENT_PACKAGES_DIR}/include/jemalloc/jemalloc.h" JEHEADER)
    if("${JEHEADER}" MATCHES "#undef JEMALLOC_USABLE_SIZE_CONST")
        vcpkg_replace_string(
            "${CURRENT_PACKAGES_DIR}/include/jemalloc/jemalloc.h"
            "#undef JEMALLOC_USABLE_SIZE_CONST"
            "#undef JEMALLOC_USABLE_SIZE_CONST\n#define JEMALLOC_USABLE_SIZE_CONST const"
        )
    endif()
endif()

vcpkg_copy_pdbs()

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/share")
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/tools")

# Handle copyright
file(INSTALL "${SOURCE_PATH}/COPYING" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}" RENAME copyright)
