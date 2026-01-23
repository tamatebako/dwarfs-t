# xxHash dependency configuration
# Fast integrity checking (XXH3-64, XXH3-128)

message(STATUS "DEBUG: VCPKG_BUILD=${VCPKG_BUILD}")
message(STATUS "DEBUG: CMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}")

if(DEFINED VCPKG_BUILD AND VCPKG_BUILD)
  # vcpkg mode: use find_package with CONFIG
  message(STATUS "DEBUG: Using vcpkg mode, calling find_package(xxHash CONFIG)")
  find_package(xxHash CONFIG)

  if(NOT xxHash_FOUND)
    # Try lowercase name as fallback
    message(STATUS "DEBUG: xxHash not found, trying xxhash")
    find_package(xxhash CONFIG)
  endif()

  if(xxHash_FOUND OR xxhash_FOUND)
    # Create alias to match old pkg-config target name
    if(NOT TARGET PkgConfig::XXHASH)
      if(TARGET xxHash::xxhash)
        add_library(PkgConfig::XXHASH ALIAS xxHash::xxhash)
      elseif(TARGET xxhash::xxhash)
        add_library(PkgConfig::XXHASH ALIAS xxhash::xxhash)
      endif()
    endif()

    if(xxHash_FOUND)
      message(STATUS "Using xxHash from vcpkg: ${xxHash_VERSION}")
    else()
      message(STATUS "Using xxhash from vcpkg: ${xxhash_VERSION}")
    endif()
  else()
    message(FATAL_ERROR "xxHash/xxhash not found in vcpkg mode. Check that vcpkg installed xxHash correctly.")
  endif()
else()
  # Fallback to pkg-config (non-vcpkg builds)
  message(STATUS "DEBUG: Using pkg-config mode")
  pkg_check_modules(XXHASH REQUIRED IMPORTED_TARGET libxxhash>=${XXHASH_REQUIRED_VERSION})
  message(STATUS "Using xxHash from pkg-config: ${XXHASH_VERSION}")
endif()