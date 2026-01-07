# xxHash dependency configuration
# Fast integrity checking (XXH3-64, XXH3-128)

if(DEFINED VCPKG_BUILD AND VCPKG_BUILD)
  # vcpkg mode: use find_package with CONFIG
  find_package(xxHash ${XXHASH_REQUIRED_VERSION} REQUIRED CONFIG)
  
  # Create alias to match old pkg-config target name
  if(NOT TARGET PkgConfig::XXHASH)
    add_library(PkgConfig::XXHASH ALIAS xxHash::xxhash)
  endif()
  
  message(STATUS "Using xxHash from vcpkg: ${xxHash_VERSION}")
else()
  # Fallback to pkg-config (non-vcpkg builds)
  pkg_check_modules(XXHASH REQUIRED IMPORTED_TARGET libxxhash>=${XXHASH_REQUIRED_VERSION})
  message(STATUS "Using xxHash from pkg-config: ${XXHASH_VERSION}")
endif()