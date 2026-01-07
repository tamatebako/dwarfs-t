# LZ4 dependency configuration
# Fast compression algorithm - MANDATORY for static builds

if(DEFINED VCPKG_BUILD AND VCPKG_BUILD)
  # vcpkg mode: use find_package with CONFIG
  find_package(lz4 ${LIBLZ4_REQUIRED_VERSION} REQUIRED CONFIG)
  
  # Create alias to match old pkg-config target name
  if(NOT TARGET PkgConfig::LIBLZ4)
    add_library(PkgConfig::LIBLZ4 ALIAS lz4::lz4)
  endif()
  
  set(LIBLZ4_FOUND TRUE)
  message(STATUS "Using lz4 from vcpkg: ${lz4_VERSION}")
else()
  # Fallback to pkg-config (non-vcpkg builds)
  pkg_check_modules(LIBLZ4 REQUIRED IMPORTED_TARGET liblz4>=${LIBLZ4_REQUIRED_VERSION})
  
  if(NOT LIBLZ4_FOUND)
    message(FATAL_ERROR "lz4 is MANDATORY but not found. Install liblz4-dev or set VCPKG_BUILD=ON")
  endif()
  
  message(STATUS "Using lz4 from pkg-config: ${LIBLZ4_VERSION}")
endif()