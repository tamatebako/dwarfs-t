# Zstandard dependency configuration
# Primary compression algorithm

if(DEFINED VCPKG_BUILD AND VCPKG_BUILD)
  # vcpkg mode: use find_package with CONFIG
  find_package(zstd ${ZSTD_REQUIRED_VERSION} REQUIRED CONFIG)
  
  # Create alias to match old pkg-config target name
  if(NOT TARGET PkgConfig::ZSTD)
    add_library(PkgConfig::ZSTD ALIAS zstd::libzstd_static)
  endif()
  
  message(STATUS "Using zstd from vcpkg: ${zstd_VERSION}")
else()
  # Fallback to pkg-config (non-vcpkg builds)
  pkg_check_modules(ZSTD REQUIRED IMPORTED_TARGET libzstd>=${ZSTD_REQUIRED_VERSION})
  message(STATUS "Using zstd from pkg-config: ${ZSTD_VERSION}")
endif()