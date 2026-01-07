# FLAC dependency configuration
# Lossless audio compression - MANDATORY for static builds

if(DEFINED VCPKG_BUILD AND VCPKG_BUILD)
  # vcpkg mode: use find_package with CONFIG
  find_package(FLAC ${FLAC_REQUIRED_VERSION} REQUIRED CONFIG)
  
  # Create alias to match old pkg-config target name
  if(NOT TARGET PkgConfig::FLAC)
    add_library(PkgConfig::FLAC ALIAS FLAC::FLAC++)
  endif()
  
  set(FLAC_FOUND TRUE)
  message(STATUS "Using FLAC from vcpkg: ${FLAC_VERSION}")
else()
  # Fallback to pkg-config (non-vcpkg builds)
  pkg_check_modules(FLAC REQUIRED IMPORTED_TARGET flac++>=${FLAC_REQUIRED_VERSION})
  
  if(NOT FLAC_FOUND)
    message(FATAL_ERROR "FLAC is MANDATORY but not found. Install libflac++-dev or set VCPKG_BUILD=ON")
  endif()
  
  message(STATUS "Using FLAC from pkg-config: ${FLAC_VERSION}")
endif()