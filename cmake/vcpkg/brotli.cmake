# Brotli dependency configuration
# Brotli compression algorithm - MANDATORY for static builds

if(DEFINED VCPKG_BUILD AND VCPKG_BUILD)
  # vcpkg mode: use official brotli package
  # See: https://vcpkg.io/en/package/brotli
  find_package(unofficial-brotli REQUIRED CONFIG)

  # vcpkg's brotli provides unofficial::brotli targets
  # Create aliases to match old pkg-config target names
  if(NOT TARGET PkgConfig::LIBBROTLIDEC)
    add_library(PkgConfig::LIBBROTLIDEC ALIAS unofficial::brotli::brotlidec)
  endif()
  if(NOT TARGET PkgConfig::LIBBROTLIENC)
    add_library(PkgConfig::LIBBROTLIENC ALIAS unofficial::brotli::brotlienc)
  endif()

  set(LIBBROTLIDEC_FOUND TRUE)
  set(LIBBROTLIENC_FOUND TRUE)
  message(STATUS "Using brotli from vcpkg")
else()
  # Fallback to pkg-config (non-vcpkg builds)
  pkg_check_modules(LIBBROTLIDEC REQUIRED IMPORTED_TARGET libbrotlidec>=${LIBBROTLI_REQUIRED_VERSION})
  pkg_check_modules(LIBBROTLIENC REQUIRED IMPORTED_TARGET libbrotlienc>=${LIBBROTLI_REQUIRED_VERSION})

  if(NOT LIBBROTLIDEC_FOUND OR NOT LIBBROTLIENC_FOUND)
    message(FATAL_ERROR "brotli is MANDATORY but not found. Install libbrotli-dev or set VCPKG_BUILD=ON")
  endif()

  message(STATUS "Using brotli from pkg-config: ${LIBBROTLIDEC_VERSION}")
endif()