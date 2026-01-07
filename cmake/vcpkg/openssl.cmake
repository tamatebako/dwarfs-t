# OpenSSL dependency configuration
# Provides libcrypto for checksums (SHA-512/256)

if(DEFINED VCPKG_BUILD AND VCPKG_BUILD)
  # vcpkg mode: use find_package with CONFIG
  find_package(OpenSSL ${LIBCRYPTO_REQUIRED_VERSION} REQUIRED)
  
  # Create alias to match old pkg-config target name
  if(NOT TARGET PkgConfig::LIBCRYPTO)
    add_library(PkgConfig::LIBCRYPTO ALIAS OpenSSL::Crypto)
  endif()
  
  message(STATUS "Using OpenSSL from vcpkg: ${OPENSSL_VERSION}")
else()
  # Fallback to pkg-config (non-vcpkg builds)
  pkg_check_modules(LIBCRYPTO REQUIRED IMPORTED_TARGET libcrypto>=${LIBCRYPTO_REQUIRED_VERSION})
  message(STATUS "Using OpenSSL from pkg-config: ${LIBCRYPTO_VERSION}")
endif()