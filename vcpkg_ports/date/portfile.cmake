# Overlay port for date that fixes MSYS/MinGW builds
# Issue: vcpkg_cmake_config_fixup looks for config files in CMake/ for Windows targets,
# but MinGW/MSYS generates them in lib/cmake/date/
# Fix: Use correct CONFIG_PATH for MinGW/MSYS targets

if(VCPKG_TARGET_IS_WINDOWS)
  message(WARNING
    "You will need to also install https://raw.githubusercontent.com/unicode-org/cldr/master/common/supplemental/windowsZones.xml into your install location.\n"
    "See https://howardhinnant.github.io/date/tz.html"
  )
endif()

vcpkg_from_github(
  OUT_SOURCE_PATH SOURCE_PATH
  REPO HowardHinnant/date
  REF "v${VERSION}"
  SHA512 9bffca5c7cfd1769f66bef330fe4ef0ad2512a8afd229ddb4043a4f166741e697c7a5fbdddf29f7157b3fc2c2c2a80fa7cff45078f1d8ab248d3b07e14518fcf
  HEAD_REF master
)

vcpkg_check_features(OUT_FEATURE_OPTIONS FEATURE_OPTIONS
    INVERTED_FEATURES
    remote-api USE_SYSTEM_TZ_DB
)

vcpkg_cmake_configure(
  SOURCE_PATH "${SOURCE_PATH}"
  OPTIONS
    ${FEATURE_OPTIONS}
    -DBUILD_TZ_LIB=ON
)

vcpkg_cmake_install()

# For MSYS/MinGW, the date library's CMakeLists.txt doesn't properly install
# CMake config files. We need to manually copy them from the build tree.
if(VCPKG_TARGET_IS_MINGW OR VCPKG_CMAKE_SYSTEM_NAME STREQUAL "MSYS")
    # The date library generates config files during build but doesn't install them
    # We need to manually copy them from the build directory
    # The build creates an extra 'date/' subdirectory in the output
    file(MAKE_DIRECTORY "${CURRENT_PACKAGES_DIR}/lib/cmake/date")
    file(COPY "${CURRENT_BUILDTREES_DIR}/${PORT}/${TARGET_TRIPLET}-rel/date/dateConfig.cmake" DESTINATION "${CURRENT_PACKAGES_DIR}/lib/cmake/date/" ERROR_VARIABLE copy_error)
    file(COPY "${CURRENT_BUILDTREES_DIR}/${PORT}/${TARGET_TRIPLET}-rel/date/dateConfigVersion.cmake" DESTINATION "${CURRENT_PACKAGES_DIR}/lib/cmake/date/")
    file(COPY "${CURRENT_BUILDTREES_DIR}/${PORT}/${TARGET_TRIPLET}-rel/date/dateTargets.cmake" DESTINATION "${CURRENT_PACKAGES_DIR}/lib/cmake/date/")

    # Also copy debug config files
    file(MAKE_DIRECTORY "${CURRENT_PACKAGES_DIR}/debug/lib/cmake/date")
    file(COPY "${CURRENT_BUILDTREES_DIR}/${PORT}/${TARGET_TRIPLET}-dbg/date/dateConfig.cmake" DESTINATION "${CURRENT_PACKAGES_DIR}/debug/lib/cmake/date/")
    file(COPY "${CURRENT_BUILDTREES_DIR}/${PORT}/${TARGET_TRIPLET}-dbg/date/dateConfigVersion.cmake" DESTINATION "${CURRENT_PACKAGES_DIR}/debug/lib/cmake/date/")
    file(COPY "${CURRENT_BUILDTREES_DIR}/${PORT}/${TARGET_TRIPLET}-dbg/date/dateTargets.cmake" DESTINATION "${CURRENT_PACKAGES_DIR}/debug/lib/cmake/date/")
endif()

# Use CONFIG_PATH CMake for all Windows targets (including MinGW/MSYS)
# The date library hardcodes CONFIG_LOC to CMake for WIN32 targets
if(VCPKG_TARGET_IS_WINDOWS)
  vcpkg_cmake_config_fixup(CONFIG_PATH CMake)
else()
  vcpkg_cmake_config_fixup(CONFIG_PATH "lib/cmake/date")
endif()

vcpkg_copy_pdbs()

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE.txt")
file(INSTALL "${CMAKE_CURRENT_LIST_DIR}/usage" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}")
