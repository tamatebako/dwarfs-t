# LZMA dependency configuration
# LZMA compression algorithm - MANDATORY for static builds

if(DEFINED VCPKG_BUILD AND VCPKG_BUILD)
  # vcpkg mode: use find_package with CONFIG
  find_package(LibLZMA ${LIBLZMA_REQUIRED_VERSION} REQUIRED CONFIG)
  
  # Create alias to match old pkg-config target name
  if(NOT TARGET PkgConfig::LIBLZMA)
    add_library(PkgConfig::LIBLZMA ALIAS LibLZMA::LibLZMA)
  endif()
  
  set(LIBLZMA_FOUND TRUE)
  message(STATUS "Using liblzma from vcpkg: ${LibLZMA_VERSION}")
else()
  # Fallback to pkg-config (non-vcpkg builds)
  pkg_check_modules(LIBLZMA REQUIRED IMPORTED_TARGET liblzma>=${LIBLZMA_REQUIRED_VERSION})
  
  if(NOT LIBLZMA_FOUND)
    message(FATAL_ERROR "liblzma is MANDATORY but not found. Install liblzma-dev or set VCPKG_BUILD=ON")
  endif()
  
  message(STATUS "Using liblzma from pkg-config: ${LIBLZMA_VERSION}")
endif()