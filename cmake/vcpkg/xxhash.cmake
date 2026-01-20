# xxHash dependency configuration
# Fast integrity checking (XXH3-64, XXH3-128)

message(STATUS "DEBUG: VCPKG_BUILD=${VCPKG_BUILD}")
message(STATUS "DEBUG: CMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}")

if(DEFINED VCPKG_BUILD AND VCPKG_BUILD)
  # vcpkg mode: use find_package with CONFIG
  message(STATUS "DEBUG: Using vcpkg mode, calling find_package(xxHash CONFIG)")
  find_package(xxHash CONFIG)

  if(xxHash_FOUND)
    # Create alias to match old pkg-config target name
    if(NOT TARGET PkgConfig::XXHASH)
      add_library(PkgConfig::XXHASH ALIAS xxHash::xxhash)
    endif()

    message(STATUS "Using xxHash from vcpkg: ${xxHash_VERSION}")
  else()
    message(FATAL_ERROR "xxHash not found in vcpkg mode. Check that vcpkg installed xxHash correctly.")
  endif()
else()
  # Fallback to pkg-config (non-vcpkg builds)
  message(STATUS "DEBUG: Using pkg-config mode")
  pkg_check_modules(XXHASH REQUIRED IMPORTED_TARGET libxxhash>=${XXHASH_REQUIRED_VERSION})
  message(STATUS "Using xxHash from pkg-config: ${XXHASH_VERSION}")
endif()